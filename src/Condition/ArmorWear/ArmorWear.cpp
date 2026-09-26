#include "Condition/ArmorWear/ArmorWear.h"

#include "Condition/ArmorWear/Rate.h"
#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"

#include <string>

namespace ArmorWear
{
	void Load()
	{
		REX::INFO("Armor wears from the blows that land on it, about {:.0f} blows of an ordinary weapon from new to broken. A piece at 0 stays on until it is taken off, and goes back on only once repaired.",
			BlowsToBreak());
	}

	namespace
	{
		// Whether the record protects against anything: a rating, which is the
		// physical resistance, or a damage type value, energy, radiation and
		// the rest, see ArmorRating.h. Read from the base record, not from one
		// copy, since a mod can raise a piece's rating but never give a skin
		// one.
		bool Protects(const RE::TESObjectARMO& a_armor)
		{
			const auto& data = a_armor.armorData;
			if (data.rating > 0) {
				return true;
			}
			if (data.damageTypes) {
				for (const auto& entry : *data.damageTypes) {
					if (entry.second.i > 0) {
						return true;
					}
				}
			}
			return false;
		}

		// Whether a piece gives anything to wear it for: protection, its own
		// effect, or what a mod could add through one of its slots. A wedding
		// ring or Dogmeat's bandana gives none of the 3, so wear would only cut
		// its price.
		bool GivesAnything(const RE::TESObjectARMO& a_armor)
		{
			return Protects(a_armor) || a_armor.GetBaseEnchanting() || a_armor.attachParents.size > 0;
		}
	}

	const char* WhyNoCondition(const RE::TESObjectARMO& a_armor)
	{
		// Every power armor piece carries the keyword the game finds them by,
		// and the game already wears those, see ArmorWear.h.
		if (a_armor.HasKeyword(RE::PowerArmor::GetArmorKeyword(), nullptr)) {
			return "power armor";
		}

		// Everything the player can carry that gives anything. A null
		// instance reads the flag off the record itself.
		if (a_armor.GetPlayable(nullptr)) {
			return GivesAnything(a_armor) ? nullptr : "gives nothing";
		}

		// A creature's own armor, see WhyNoCondition in ArmorWear.h. The name
		// is what tells a super mutant's chest piece from a behemoth's shell.
		if (Protects(a_armor) && !RE::TESFullName::GetFullName(a_armor).empty()) {
			return nullptr;
		}
		return "not playable and not a creature's armor";
	}

	bool IsClothing(const RE::TESObjectARMO& a_armor)
	{
		// Read from the base record, like Protects: a lining can add a
		// resistance to a dress, and the dress stays clothing.
		const auto body = 1U << static_cast<std::uint32_t>(RE::BIPED_OBJECT::kBody);
		return (a_armor.bipedModelData.bipedObjectSlots & body) != 0 || !Protects(a_armor);
	}

	namespace
	{
		// Wears down the one stack the engine hands it, and remembers whether
		// the health moved and whether that finished the piece. The engine has
		// already split the stack if it held more than one piece, see
		// Equipped::WriteEquipped, so the piece worn and the piece that wears
		// stay the same one.
		class WearStack final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			WearStack(float a_damage, const char* a_source, bool a_theirs) noexcept :
				damage(a_damage),
				source(a_source),
				theirs(a_theirs)
			{}

			void WriteDataImpl(RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (!a_stack.extra) {
					return;
				}

				// WearsOut has already made sure this is armor.
				const auto& armor = static_cast<const RE::TESObjectARMO&>(a_object);
				const auto  amount = Rate(armor, a_stack.extra.get(), damage, theirs);
				moved = Condition::Decrease(*a_stack.extra, a_object, amount, source, 1.0F, theirs);

				// The engine's own broken check, run on the health just
				// written. It is the same check that stops the piece going back
				// on, see MIN_HEALTH in Condition.h.
				broke = moved && a_stack.extra->IsItemBroken();
			}

			float       damage;
			const char* source;
			bool        theirs;
			bool        moved{ false };
			bool        broke{ false };
		};

		// What happens to a piece worn to 0. It stays on, as a broken piece did
		// in the older games. The game refuses to put a broken piece back on,
		// see MIN_HEALTH in Condition.h, but has no rule for one that breaks
		// while worn, so the message in the corner and the faded name in the
		// Pip-Boy are the only signs until it comes off. Meanwhile it protects
		// at the floor, see ArmorRating.h, and keeps its bonuses. Queued, since
		// this runs in the middle of a hit the engine has not finished. F4SE
		// runs the task a moment later from the game's own queue. Forms are
		// only freed on a full reset, which waits for the player to answer a
		// prompt, so the armor pointer is still valid then.
		void AfterBreak(RE::TESObjectARMO& a_armor)
		{
			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				return;
			}

			auto* armor = &a_armor;
			tasks->AddTask([armor]() {
				const auto name = RE::TESFullName::GetFullName(*armor);
				const auto said = Text::ArmorWornOut(name);
				RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, false);

				TraceLog::Line("wear", "{:s} [{:08X}] is worn out and stays on until it is taken off", name, armor->formID);
			});
		}
	}

	bool Wear(RE::Actor& a_owner, RE::TESObjectARMO& a_armor, float a_damage, const char* a_source, bool a_theirs)
	{
		WearStack  wear{ a_damage, a_source, a_theirs };
		const auto count = Equipped::WriteEquipped(a_owner, a_armor, wear);

		// A split that leaves the health unchanged merges straight back, so
		// only a change is worth reporting.
		if (wear.moved && count > 1) {
			TraceLog::For(a_theirs).Line("wear", "worn copy split off a stack of {:d}", count);
		}

		// The inventory lock is released by here, which is what makes queueing
		// safe. Only the player sees the message in the corner of the screen.
		if (wear.broke && !a_theirs) {
			AfterBreak(a_armor);
		}
		return wear.moved;
	}
}
