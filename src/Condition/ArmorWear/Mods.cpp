#include "Condition/ArmorWear/Mods.h"

#include "Condition/ArmorWear/ArmorWear.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ArmorWear
{
	namespace
	{
		// -------------------------------------------------------------------
		// The table
		// -------------------------------------------------------------------

		using Mod = RE::BGSMod::Attachment::Mod;
		using Keywords = std::vector<const RE::BGSKeyword*>;
		using Added = std::unordered_set<const RE::TESForm*>;

		// What a piece needs for a mod in one slot that adds something to fit
		// it. anyPiece is a mod that asks for no keyword, or only for ones an
		// armor mod adds. Each list in needs is the rest of what one mod asks
		// for. The game's fit check wants every one, and GivingModFits looks
		// for them on the piece's own record.
		struct Slot
		{
			bool                  anyPiece{ false };
			std::vector<Keywords> needs;
		};

		// Slots by the attach point's keyword index, the number a piece's
		// attachParents and a mod's attachPoint both hold.
		using Slots = std::unordered_map<std::uint16_t, Slot>;

		// Empty while no table is built. Readers hold the lock shared, see
		// MeasureModsAgain in ArmorWear.h.
		std::shared_mutex    g_lock;
		std::optional<Slots> g_slots;

		// What the log said last, so measuring again speaks only when a
		// number moved. Only the main thread touches it.
		struct Counts
		{
			std::size_t giving{ 0 };
			std::size_t carried{ 0 };
			std::size_t nothing{ 0 };
		};
		Counts g_said;

		// -------------------------------------------------------------------
		// Reading the armor mods
		// -------------------------------------------------------------------

		// Whether a mod adds protection, an effect or a bonus on its own: a
		// rating, an enchantment, a damage type or an actor value, with a
		// value that is not 0. Weight, price and looks do not count.
		bool GivesItself(const Mod& a_mod)
		{
			using TARGET = RE::BGSMod::Property::ARMOR_TARGET;
			const auto properties = a_mod.GetBuffer<RE::BGSMod::Property::Mod>(std::to_underlying(RE::BGSMod::Property::BLOCKIDS::kPMOD));
			return std::ranges::any_of(properties, [](const RE::BGSMod::Property::Mod& a_property) {
				switch (static_cast<TARGET>(a_property.target)) {
					case TARGET::kRating:
						return a_property.data.mm.min.i != 0;
					case TARGET::kEnchantments:
						return a_property.data.form != nullptr;
					case TARGET::kDamageTypeValue:
					case TARGET::kActorValues:
						return a_property.data.fv.formID != 0 && a_property.data.fv.value != 0.0F;
					default:
						return false;
				}
			});
		}

		// Whether a mod is for armor. The game applies a mod with no target
		// type to armor too.
		bool ForArmor(const Mod& a_mod)
		{
			const auto type = a_mod.targetFormType.get();
			return type == RE::ENUM_FORM_ID::kARMO || type == RE::ENUM_FORM_ID::kNONE;
		}

		// A mod with no target type counts only when it adds something
		// itself. In the game files these are mod collections, which ask
		// for no keyword but bring mods that sit in the same slot under
		// their own keywords. Counting a collection through those would let
		// any piece in the slot fit.
		bool NoTarget(const Mod& a_mod)
		{
			return a_mod.targetFormType.get() == RE::ENUM_FORM_ID::kNONE;
		}

		// Every armor mod that adds something, on its own or through a mod it
		// brings along.
		std::unordered_set<const Mod*> GivingMods()
		{
			std::vector<const Mod*>        armor;
			std::unordered_set<const Mod*> giving;
			for (const auto* mod : g_dataHandler->GetFormArray<Mod>()) {
				if (!mod || !ForArmor(*mod)) {
					continue;
				}
				armor.push_back(mod);
				if (GivesItself(*mod)) {
					giving.insert(mod);
				}
			}

			// A mod that brings along one that adds something adds something
			// too, repeated until a pass finds no more.
			for (bool more = true; more;) {
				more = false;
				for (const auto* mod : armor) {
					if (NoTarget(*mod)) {
						continue;
					}
					const auto brings = mod->GetBuffer<RE::BGSMod::Attachment::Instance>(std::to_underlying(RE::BGSMod::Property::BLOCKIDS::kOMOD));
					if (!giving.contains(mod) && std::ranges::any_of(brings, [&](const auto& a_brought) { return giving.contains(a_brought.mod); })) {
						giving.insert(mod);
						more = true;
					}
				}
			}
			return giving;
		}

		// Every keyword an armor mod adds to the copy of a piece it is on.
		// The game's fit check, ID 2197512, reads the copy's keywords, so a
		// mod that asks for one of these fits once the mod that adds it is
		// on. It is not checked that the adding mod fits the piece, or that
		// a mod that sets a keyword takes the copy's other keywords off, so
		// in doubt a piece wears. A keyword that a piece's object template
		// adds is not counted. The forms are only compared, never read.
		Added AddedKeywords()
		{
			using Property = RE::BGSMod::Property::Mod;
			using TARGET = RE::BGSMod::Property::ARMOR_TARGET;
			using TYPE = RE::BGSMod::Property::TYPE;
			using OP = RE::BGSMod::Property::OP;
			Added added;
			for (const auto* mod : g_dataHandler->GetFormArray<Mod>()) {
				if (!mod || !ForArmor(*mod)) {
					continue;
				}
				for (const auto& property : mod->GetBuffer<Property>(std::to_underlying(RE::BGSMod::Property::BLOCKIDS::kPMOD))) {
					if (static_cast<TARGET>(property.target) == TARGET::kKeywords && property.type == TYPE::kForm &&
						(property.op == OP::kAdd || property.op == OP::kSet) && property.data.form) {
						added.insert(property.data.form);
					}
				}
			}
			return added;
		}

		// -------------------------------------------------------------------
		// Building the table
		// -------------------------------------------------------------------

		// The keywords a mod asks a piece to carry, without repeats and
		// without the ones an armor mod adds.
		Keywords AskedFor(const Mod& a_mod, const Added& a_added)
		{
			Keywords    asked;
			const auto* keywords = a_mod.GetTargetOMODKeywords();
			for (std::uint32_t i = 0; keywords && keywords->array && i < keywords->size; i++) {
				const auto* keyword = RE::BGSKeyword::GetTypedKeywordByIndex(RE::KeywordType::kModAssociation, keywords->array[i].keywordIndex);
				if (keyword && !a_added.contains(keyword) && std::ranges::find(asked, keyword) == asked.end()) {
					asked.push_back(keyword);
				}
			}
			std::ranges::sort(asked);
			return asked;
		}

		// The table, or nothing when no armor mod adds anything, which is a
		// misread on any real load order. a_giving is how many mods with a
		// slot add something.
		std::optional<Slots> Measure(std::size_t& a_giving)
		{
			const auto giving = GivingMods();
			const auto added = AddedKeywords();

			// The armor mods in each slot.
			std::unordered_map<std::uint16_t, std::vector<const Mod*>> bySlot;
			for (const auto* mod : g_dataHandler->GetFormArray<Mod>()) {
				if (mod && ForArmor(*mod) &&
					RE::BGSKeyword::GetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, mod->attachPoint.keywordIndex)) {
					bySlot[mod->attachPoint.keywordIndex].push_back(mod);
					a_giving += giving.contains(mod) ? 1 : 0;
				}
			}
			if (a_giving == 0) {
				return std::nullopt;
			}

			// A slot counts when a mod in it adds something, or opens a slot
			// that counts, like a plain frame and its lining in the next slot.
			std::unordered_set<std::uint16_t> counting;
			const auto counts = [&](const Mod* a_mod) {
				const std::span opens{ a_mod->attachParents.array, a_mod->attachParents.array ? a_mod->attachParents.size : 0 };
				return giving.contains(a_mod) ||
				       (!NoTarget(*a_mod) && std::ranges::any_of(opens, [&](const auto& a_open) { return counting.contains(a_open.keywordIndex); }));
			};
			for (bool more = true; more;) {
				more = false;
				for (const auto& [slot, mods] : bySlot) {
					if (!counting.contains(slot) && std::ranges::any_of(mods, counts)) {
						counting.insert(slot);
						more = true;
					}
				}
			}

			Slots slots;
			for (const auto& [index, mods] : bySlot) {
				for (const auto* mod : mods) {
					if (!counts(mod)) {
						continue;
					}
					auto& slot = slots[index];
					auto  asked = AskedFor(*mod, added);
					if (asked.empty()) {
						slot.anyPiece = true;
					} else if (std::ranges::find(slot.needs, asked) == slot.needs.end()) {
						slot.needs.push_back(std::move(asked));
					}
				}
			}
			return slots;
		}

		// -------------------------------------------------------------------
		// Swapping the table in and saying so
		// -------------------------------------------------------------------

		// Builds a table, swaps it in whole, and counts what it leaves out.
		// The old table is freed after the lock is let go.
		Counts MeasureAndSwap()
		{
			Counts counts;
			auto   fresh = Measure(counts.giving);
			{
				const std::unique_lock l{ g_lock };
				g_slots.swap(fresh);
			}

			// The pieces the player can carry, and how many of them give
			// nothing. Power armor never takes part.
			for (const auto* armor : g_dataHandler->GetFormArray<RE::TESObjectARMO>()) {
				if (!armor || !armor->GetPlayable(nullptr) || IsPowerArmor(*armor)) {
					continue;
				}
				counts.carried++;
				counts.nothing += WhyNoCondition(*armor) ? 1 : 0;
			}
			return counts;
		}

		void WarnUnread()
		{
			REX::WARN("No armor mod adds protection, an effect or a bonus, so the armor mods could not be read. Every piece with a slot for a mod wears, and no piece is put back to full.");
		}
	}

	// -------------------------------------------------------------------
	// Measuring and asking
	// -------------------------------------------------------------------

	void LoadMods()
	{
		UnloadMods();
		if (!g_dataHandler) {
			return;
		}

		g_said = MeasureAndSwap();
		if (!ModsMeasured()) {
			WarnUnread();
			return;
		}
		REX::INFO("{:d} armor mods add protection, an effect or a bonus. Of the {:d} pieces the player can carry, {:d} give nothing, with no protection, no effect and no such mod that fits, so they never wear.",
			g_said.giving, g_said.carried, g_said.nothing);
	}

	void MeasureModsAgain()
	{
		if (!g_dataHandler) {
			return;
		}

		const bool was = ModsMeasured();
		const auto counts = MeasureAndSwap();
		if (!ModsMeasured()) {
			if (was) {
				WarnUnread();
			}
		} else if (!was || counts.giving != g_said.giving || counts.carried != g_said.carried || counts.nothing != g_said.nothing) {
			REX::INFO("{:d} armor mods add protection, an effect or a bonus, and {:d} of the {:d} pieces the player can carry give nothing, as a mod changed armor or its mods after game data loaded.",
				counts.giving, counts.nothing, counts.carried);
		}
		g_said = counts;
	}

	void UnloadMods()
	{
		std::optional<Slots> old;
		{
			const std::unique_lock l{ g_lock };
			g_slots.swap(old);
		}
		g_said = {};
	}

	std::optional<bool> GivingModFits(const RE::TESObjectARMO& a_armor)
	{
		const std::shared_lock l{ g_lock };
		if (!g_slots) {
			return std::nullopt;
		}

		const auto& parents = a_armor.attachParents;
		for (std::uint32_t i = 0; parents.array && i < parents.size; i++) {
			const auto found = g_slots->find(parents.array[i].keywordIndex);
			if (found == g_slots->end()) {
				continue;
			}
			if (found->second.anyPiece) {
				return true;
			}
			for (const auto& needs : found->second.needs) {
				if (std::ranges::all_of(needs, [&](const RE::BGSKeyword* a_keyword) { return a_armor.HasKeyword(a_keyword, nullptr); })) {
					return true;
				}
			}
		}
		return false;
	}

	bool ModsMeasured()
	{
		const std::shared_lock l{ g_lock };
		return g_slots.has_value();
	}
}
