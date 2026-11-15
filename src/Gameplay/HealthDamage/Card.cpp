#include "Gameplay/HealthDamage/Hooks.h"

#include "Condition/Condition.h"
#include "Condition/Excluded.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog/TraceLog.h"
#include "Gameplay/HealthDamage/Curve.h"
#include "Gameplay/HealthDamage/HealthDamage.h"
#include "Gameplay/HealthDamage/Trace.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace HealthDamage
{
	namespace
	{
		// -------------------------------------------------------------------
		// Where the engine works damage out
		// -------------------------------------------------------------------

		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::NO_OG_COVERED;
		using CallPatch::OG;

		// Both sit inside PipboyInventoryUtils::FillDamageTypeInfo, which adds
		// a weapon's damage types up for its item card. The first calls
		// CombatFormulas::GetWeaponDisplayDamage and the second
		// BGSEntryPoint::HandleEntryPoint.
		constexpr CallSite CARD_HEALTH_SITE{ AE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.ae(), 0x0B6 }, OG{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.og(), 0x0BD }, "card health" };
		constexpr CallSite CARD_TYPES_SITE{ AE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.ae(), 0x1B7 }, OG{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.og(), 0x25B }, "card types" };

		// The same function adds each object effect's magnitude to its damage
		// type through 2 walks, one over the effects the mods add and one for
		// the weapon's own and its blast's. Each asks the perk system about
		// every magnitude. OG makes both calls from 1 place, so the
		// mod effects have none of their own there and the one call scales
		// both walks.
		constexpr CallSite CARD_EFFECT_SITES[] = {
			{ AE{ 2225299, 0x10A }, NO_OG_COVERED, "card mod effects" },
			{ AE{ 2225344, 0x0DA }, OG{ 282246, 0x063 }, "card effects" },
		};

		// Inside CombatFormulas::GetWeaponDisplayDamage, the one call about the
		// blast half of a weapon's damage before the 2 halves are added.
		constexpr CallSite CARD_BLAST_SITE{ AE{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.ae(), 0x14D }, OG{ RE::ID::CombatFormulas::GetWeaponDisplayDamage.og(), 0x155 }, "card blast" };

		// GetWeaponDisplayDamage has 4 callers on both games. Beside the card,
		// this is the one that passes the health a stack carries. It works out
		// 1 stack's damage, and the only weapon list that uses it is the HUD
		// loot list's sort. The other 2 pass full health, which scales nothing.
		// LIST_STACK_SITE sits in that function too.
		constexpr CallSite STACK_DAMAGE_SITE{ AE{ 2222649, 0x13E }, OG{ 307737, 0x12F }, "stack damage" };

		// The 2 calls to ExtraDataList::GetHealthPerc right before the card
		// health call and the stack damage call, each on the extra data list
		// of the stack whose damage follows. That list holds the stack's mods,
		// so a weapon mod's line in NEC/Wear can count. A missing stack skips
		// the call and passes full health.
		//
		// The first is in FillDamageTypeInfo, and every caller passes its own
		// stack through it: the card's item, the compare item, the item list
		// entries and CardDamage. The second is in the stack damage site's
		// function. Of its 3 callers, 1 ranks the HUD loot list of a container
		// with fewer items than uQCInventorySortThreshold, 1 is never reached
		// and 1 only sees the player's ammo. So a listed mod's floor also
		// reaches that list's damage sort, as the card shows it.
		constexpr CallSite CARD_STACK_SITE{ AE{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.ae(), 0x0A7 }, OG{ RE::ID::PipboyInventoryUtils::FillDamageTypeInfo.og(), 0x0A6 }, "card stack" };
		constexpr CallSite LIST_STACK_SITE{ AE{ 2222649, 0x0FE }, OG{ 307737, 0x0EF }, "stack list" };

		// -------------------------------------------------------------------
		// The item card hooks
		// -------------------------------------------------------------------

		// A health and the floor of the copy it belongs to, see Curve.h.
		struct Worn
		{
			float health = Condition::INVALID_HEALTH;
			Floor floor;
		};

		// The condition of the item whose card is being built, handed from the
		// first Pip-Boy hook to the second. The second stands at 3 places
		// inside the one call that adds up the card, on the thread building it,
		// and the first always runs before them, so a thread local carries it.
		// The types hook takes it once, so a card whose health call another
		// DLL skipped never takes the condition of the card before it.
		thread_local Worn t_cardHealth;

		// What the types hook took, for the same card's object effects. The
		// health hook clears it, so a card whose types call was skipped leaves
		// its effects at full too.
		thread_local Worn t_cardEffects;

		// Whether the call to GetWeaponDisplayDamage that the card health hook
		// or the stack damage hook hands on is for a weapon on a list in
		// NEC/Exclude, for the display site inside it. That site sees only the
		// player and the instance data, never which weapon it is.
		thread_local bool t_listed = false;

		// The floor of the copy that call is about, for the display site,
		// set and put back the same way.
		thread_local Floor t_displayFloor;

		// The health the display site scaled the card by, for the blast hook
		// a few instructions later in the same call. Read once, so a card
		// whose display hook did not run shows the vanilla explosion
		// damage, never the last card's.
		thread_local Worn t_displayHealth;

		// The list each stack site read, for the damage hook of the same
		// stack 1 call later. Each pair has its own, which its damage hook
		// takes once and checks against the call's instance data, see
		// LineFloorOf.
		thread_local const RE::ExtraDataList* t_cardStack = nullptr;
		thread_local const RE::ExtraDataList* t_listStack = nullptr;

		// The health CardDamage asks the card about, in place of the one the
		// stack carries, and nothing while the card stays vanilla.
		thread_local std::optional<float> t_askedHealth;

		// The 2 Pip-Boy hooks. The object effect sites read what they hand
		// on, so they ask it too.
		CallPatch::Held g_card;

		// The 3 type sites are the pair's types call and the 2 object effect
		// calls, in order.
		using CardHealth_t = float (*)(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, const RE::TESAmmo*, float);
		using CardTypes_t = void (*)(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const void*, void*, float*);
		using CardBlast_t = void (*)(RE::BGSEntryPoint::ENTRY_POINT, RE::Actor*, const void*, float*);

		CallPatch::Link<CardHealth_t>                g_cardHealthLink;
		std::array<CallPatch::Link<CardTypes_t>, 3>  g_cardTypeLinks;
		CallPatch::Link<CardBlast_t>                 g_cardBlastLink;
		CallPatch::Link<CardHealth_t>                g_stackDamageLink;

		// ExtraDataList::GetHealthPerc: the list in, its health out.
		using StackHealth_t = float (*)(RE::ExtraDataList*);
		CallPatch::Link<StackHealth_t> g_cardStackLink;
		CallPatch::Link<StackHealth_t> g_listStackLink;

		// The 2 stack hooks note the list and hand the vanilla health on.
		float CardStackHk(RE::ExtraDataList* a_extra)
		{
			const auto health = g_cardStackLink(a_extra);
			t_cardStack = g_cardStackLink.Live() ? a_extra : nullptr;
			return health;
		}

		float ListStackHk(RE::ExtraDataList* a_extra)
		{
			const auto health = g_listStackLink(a_extra);
			t_listStack = g_listStackLink.Live() ? a_extra : nullptr;
			return health;
		}

		// The floor a file in NEC/Wear sets for the copy a_weapon is, with
		// the mods on a_stack, the list its stack site read. That list counts
		// only when it holds a_weapon's instance data, so a list of another
		// stack never lends its mods, and the weapon's own line and its
		// keywords' still count. For a stack with no instance data the card
		// passes the weapon's own, which matches, and the item lists pass
		// none, which does not, so in a list such a stack loses its mod lines.
		Floor LineFloorOf(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::ExtraDataList* a_stack)
		{
			// An ammo stack reaches the stack damage hook with no weapon.
			const auto* weapon = a_weapon.object ? a_weapon.object->As<RE::TESObjectWEAP>() : nullptr;
			if (!weapon || !WearList::Any()) {
				return {};
			}
			const auto* data = static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
			const auto  mine = a_stack && &Condition::InstanceDataOf<RE::TESObjectWEAP::InstanceData>(a_stack, weapon->weaponData) == data;
			return WearList::Of(*weapon, data, mine ? a_stack : nullptr).damage;
		}

		// Records the condition of the item, or -1 while the pair does not
		// run and for a weapon on a list in NEC/Exclude. The physical damage
		// is already scaled, since this call reaches CalcWeaponDamage through
		// the display site in Combat.cpp. The vanilla call gets the same
		// health, which vanilla scales nothing by below full, so its base
		// number stays vanilla. A health CardDamage asks about stands in for
		// the stack's, and the vanilla call hands it on to the display site.
		// The item's floor goes with its condition to every hook of the card.
		float CardHealthHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESAmmo* a_ammo, float a_health)
		{
			const auto* stack = std::exchange(t_cardStack, nullptr);
			const auto  health = t_askedHealth.value_or(a_health);
			const auto  listed = a_weapon.object && Excluded::Has(*a_weapon.object);
			const auto  floor = listed ? Floor{} : LineFloorOf(a_weapon, stack);
			t_cardEffects = {};
			t_cardHealth = g_card.Runs(g_cardHealthLink) && !listed ? Worn{ health, floor } : Worn{};

			// Once per change, since the Pip-Boy builds a card for every item.
			// The compare item and each item list entry pass here too, and
			// nothing cheap tells the card's own item apart, so a list with 2
			// weapons with a damage line in NEC/Wear writes 1 line an entry
			// each time it is built. The tag "card" is in TraceLog::QUIET, so
			// the line shows only while a developer takes the tag out.
			if (floor) {
				static std::atomic<std::uint64_t> last{ 0 };
				if (!Repeats(last, { floor->value, static_cast<float>(floor->file), static_cast<float>(floor->line) }, a_weapon.object->formID)) {
					TraceLog::Line("card", "{:s}{:s}", TraceLog::Who{ a_weapon.object }, FloorTail{ floor });
				}
			}

			const auto was = std::exchange(t_listed, listed);
			const auto wasFloor = std::exchange(t_displayFloor, floor);
			const auto damage = g_cardHealthLink(a_weapon, a_ammo, health);
			t_listed = was;
			t_displayFloor = wasFloor;
			return damage;
		}

		// Marks a stack of a weapon on a list in NEC/Exclude, so its damage
		// and blast stay vanilla whatever health it carries, and hands the
		// display site the floor of any other.
		float StackDamageHk(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESAmmo* a_ammo, float a_health)
		{
			// Taken first, so an ammo stack clears it too.
			const auto* stack = std::exchange(t_listStack, nullptr);
			const auto  live = g_stackDamageLink.Live();
			const auto  listed = a_weapon.object && live && Excluded::Has(*a_weapon.object);
			const auto  floor = live && !listed ? LineFloorOf(a_weapon, stack) : Floor{};
			const auto  was = std::exchange(t_listed, listed);
			const auto  wasFloor = std::exchange(t_displayFloor, floor);
			const auto  damage = g_stackDamageLink(a_weapon, a_ammo, a_health);
			t_listed = was;
			t_displayFloor = wasFloor;
			return damage;
		}

		// The card's damage types are scaled the same way as in combat: the
		// engine asks the perk system for one multiplier over the whole weapon
		// and multiplies every damage type by it, so returning it multiplied by
		// the condition scales all of them. The entry point is
		// kModAttackDamage, the perk entry that raises or lowers damage, and
		// the object effects go through kModSpellMagnitude. HandleEntryPoint
		// takes a variable number of arguments, 3 pointers here with the number
		// to change last, passed like a fixed list on x64.
		// I is 0 for the pair's types call, 1 and 2 for the object effect
		// calls, which stand alone and read whether the pair runs too.
		template <std::size_t I>
		void CardTypesHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance,
			void* a_unused, float* a_out)
		{
			g_cardTypeLinks[I](a_entryPoint, a_perkOwner, a_instance, a_unused, a_out);
			Worn worn;
			auto live = false;
			if constexpr (I == 0) {
				worn = std::exchange(t_cardHealth, Worn{});
				live = g_card.Runs(g_cardTypeLinks[0]);
				t_cardEffects = live ? worn : Worn{};
			} else {
				worn = t_cardEffects;
				live = g_card.Intact() && g_cardTypeLinks[I].Live();
			}
			if (a_out && live) {
				*a_out *= DamageMult(worn.health, worn.floor);
			}
		}

		// The explosion part of the item card, the one call the engine makes
		// about it, so the only place the card can be told the shell came out
		// of a worn weapon. The entry point is the one Demolition Expert uses.
		// One pointer fewer than CardTypesHk: the weapon, then the number.
		void CardBlastHk(RE::BGSEntryPoint::ENTRY_POINT a_entryPoint, RE::Actor* a_perkOwner, const void* a_instance, float* a_out)
		{
			// Read first, so a weapon with no blast clears the note too.
			const auto [health, floor] = std::exchange(t_displayHealth, Worn{});
			g_cardBlastLink(a_entryPoint, a_perkOwner, a_instance, a_out);
			if (!a_out || !(*a_out > 0.0F) || !g_cardBlastLink.Live()) {
				return;
			}

			// 0 is what every weapon that fires nothing explosive reports, so
			// the guard keeps the whole inventory out of the log.
			const auto mult = DamageMult(health, floor);

			static std::atomic<std::uint64_t> last{ 0 };
			if (!Repeats(last, { health, *a_out, mult })) {
				TraceLog::Line("card", "blast  health {:.6f}  {:.2f} x {:.4f} = {:.2f}{:s}",
					health, *a_out, mult, *a_out * mult, FloorTail{ floor });
			}

			*a_out *= mult;
		}
	}

	float DisplayHealth(float a_condition)
	{
		t_displayHealth = t_listed ? Worn{} : Worn{ a_condition, t_displayFloor };
		return t_displayHealth.health;
	}

	Floor DisplayFloor()
	{
		return t_displayFloor;
	}

	std::vector<std::pair<std::uint32_t, float>> CardDamage(const RE::BGSInventoryItem& a_item,
		const RE::BGSInventoryItem::Stack* a_stack, float a_health)
	{
		// The hooks this call goes through leave healths for the card's later
		// hooks, which the next card built on this thread could read, so they
		// are put back as they were.
		RE::BSScrapArray<RE::BSTTuple<std::uint32_t, float>> values;
		const auto kept = std::tuple{ t_cardHealth, t_cardEffects, t_displayHealth };
		const auto was = std::exchange(t_askedHealth, a_health);
		RE::PipboyInventoryUtils::FillDamageTypeInfo(a_item, a_stack, values);
		t_askedHealth = was;
		std::tie(t_cardHealth, t_cardEffects, t_displayHealth) = kept;
		std::vector<std::pair<std::uint32_t, float>> out;
		out.reserve(values.size());
		for (const auto& entry : values) {
			out.emplace_back(entry.first, entry.second);
		}
		return out;
	}

	void InstallCard()
	{
		const auto& entryPoint = RE::ID::BGSEntryPoint::HandleEntryPoint;

		g_card = CallPatch::PatchTogether({
			{ CARD_HEALTH_SITE, RE::ID::CombatFormulas::GetWeaponDisplayDamage, reinterpret_cast<std::uintptr_t>(&CardHealthHk), &g_cardHealthLink },
			{ CARD_TYPES_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardTypesHk<0>), &g_cardTypeLinks[0] },
		}, Part::kCardDamage);

		if (!g_card) {
			REX::WARN("The item card will keep printing damage types at full strength.");
		} else {
			REX::INFO("The item card prints damage types at the condition the item is in.");

			// The card's object effects read the condition the pair records, so
			// they go in only once the pair has.
			const auto effectHooks = CallPatch::PerSite<std::size(CARD_EFFECT_SITES)>([]<std::size_t I>() { return &CardTypesHk<1 + I>; });
			CallPatch::PatchAll(CARD_EFFECT_SITES, RE::ID::BGSEntryPoint::HandleEntryPoint, effectHooks, std::span{ g_cardTypeLinks }.subspan(1),
				"The item card prints object effects at the condition the item is in", Part::kCardDamage);

			// The card health hook reads the list this site notes, so it goes
			// in only once the pair has. Without it a weapon mod's line in
			// NEC/Wear never counts on the card.
			if (!CallPatch::PatchCall(CARD_STACK_SITE, RE::ID::ExtraDataList::GetHealthPerc, reinterpret_cast<std::uintptr_t>(&CardStackHk), g_cardStackLink, Part::kCardDamage)) {
				REX::WARN("The item card reads wear list lines by weapon and keyword only.");
			} else {
				REX::INFO("The item card reads each weapon's own wear list line.");
			}
		}

		// Installed on its own, since it is in a different function and reads a
		// different value. A card that lost one is still right about the other.
		if (!CallPatch::PatchCall(CARD_BLAST_SITE, entryPoint, reinterpret_cast<std::uintptr_t>(&CardBlastHk), g_cardBlastLink, Part::kCardDamage)) {
			REX::WARN("The item card will keep printing a worn explosive weapon at full damage.");
		} else {
			REX::INFO("The item card prints the blast at the condition the weapon is in.");
		}

		// Only the HUD's list of a container's items uses these 2, to sort
		// weapons by damage, see STACK_DAMAGE_SITE. Without the first a listed
		// weapon's stack there counts by a health it carries and reads no wear
		// list line, and every other number works. The second hands the first
		// the stack's mods, so it goes in only once the first has.
		if (!CallPatch::PatchCall(STACK_DAMAGE_SITE, RE::ID::CombatFormulas::GetWeaponDisplayDamage, reinterpret_cast<std::uintptr_t>(&StackDamageHk), g_stackDamageLink)) {
			REX::WARN("The HUD's list of a container's items sorts a weapon on a list in NEC/Exclude by its worn damage and reads no wear list line.");
		} else if (!CallPatch::PatchCall(LIST_STACK_SITE, RE::ID::ExtraDataList::GetHealthPerc, reinterpret_cast<std::uintptr_t>(&ListStackHk), g_listStackLink)) {
			REX::WARN("The HUD's list of a container's items reads wear list lines by weapon and keyword only.");
		} else {
			REX::INFO("The HUD's list of a container's items sorts weapons by their worn damage and wear list lines.");
		}
	}
}
