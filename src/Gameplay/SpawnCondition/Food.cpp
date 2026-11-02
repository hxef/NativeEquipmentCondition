#include "Gameplay/SpawnCondition/Food.h"

#include "Condition/Condition.h"
#include "Condition/Provenance/Provenance.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/SpawnCondition/Band.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Handover.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// Which food rolls
		// -------------------------------------------------------------------

		bool Rolls(const RE::TESBoundObject& a_object)
		{
			return Settings::bSpawnCondition.GetValue() && FoodSpoil::Spoils(a_object);
		}

		// Whether a_list is the player's or a living companion's.
		bool Carried(const RE::BGSInventoryList& a_list)
		{
			if (!FoodSpoil::Starts(a_list)) {
				return false;
			}
			const auto  owner = a_list.owner.get();
			const auto* actor = owner ? owner->As<RE::Actor>() : nullptr;
			return !actor || !actor->IsDead(true);
		}

		// What gives a_list food fresh, or nothing for found food.
		const char* FreshFrom(const RE::BGSInventoryList* a_list)
		{
			if (Restocking(a_list)) {
				return "a trader's stock";
			}
			if (FromScript()) {
				return "given by a script";
			}
			if (FromConsole() && ConsoleTarget(a_list)) {
				return "given by the console";
			}
			return nullptr;
		}

		// -------------------------------------------------------------------
		// What the trace log is told
		// -------------------------------------------------------------------

		// Whether a roll for a_list gets a food line: the player's, a console
		// command's or a restock's. A cell loading fills dozens of boxes and
		// characters at once, so the rest go to the quiet loot tag.
		bool Listed(const RE::BGSInventoryList* a_list)
		{
			return Players(a_list) || FromConsole() || Restocking(a_list);
		}

		template <class... T>
		void Tell(bool a_listed, std::format_string<T...> a_fmt, T&&... a_args)
		{
			TraceLog::Line(a_listed ? "food" : "loot", a_fmt, std::forward<T>(a_args)...);
		}
	}

	// -------------------------------------------------------------------
	// The roll
	// -------------------------------------------------------------------

	float RollIn(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, const RE::ExtraDataList* a_extra,
		std::uint32_t a_count, std::string_view a_how, bool a_listed)
	{
		// Food has no scale of its own, so its list counts by its weapons,
		// which only a list mixing food and weapons has.
		const auto origin = Provenance::Of(a_list, a_extra, Condition::Kind::kWeapon);
		const Aim  aim{ origin.Centre(), std::nullopt };
		const auto rolled = RollHealth(aim);
		if (TraceLog::IsOpen()) {
			Tell(a_listed, "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, {:s}, band around {:.2f}, {:s}{:s}{:s}",
				NameOf(a_object, "unnamed food"), a_object.formID, a_count, rolled.health, a_how, aim.centre, Describe(origin),
				Shifted(aim, rolled), rolled.upset ? ", ignored its band" : "");
		}
		return rolled.health;
	}

	// -------------------------------------------------------------------
	// Where food rolls
	// -------------------------------------------------------------------

	bool FoodBorn(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extra, std::uint32_t a_count,
		bool a_fill)
	{
		if (!a_list || !a_object || !a_extra || !Rolls(*a_object) || !FoodSpoil::Unrolled(*a_extra)) {
			return false;
		}
		// A fill still rolls for a body placed dead, which no death stamps.
		if (a_fill ? Carried(*a_list) : FoodSpoil::Starts(*a_list)) {
			return false;
		}
		const auto listed = Listed(a_list);
		if (const auto* fresh = FreshFrom(a_list)) {
			FoodSpoil::SetStart(*a_extra, Condition::MAX_HEALTH);
			Tell(listed, "{:<30s} [{:08X}] x{:<3d} starts fresh, {:s}", NameOf(*a_object, "unnamed food"), a_object->formID, a_count,
				fresh);
			return true;
		}
		FoodSpoil::SetStart(*a_extra,
			RollIn(*a_list, *a_object, a_extra, a_count, a_fill ? "in its container's fill" : "as it was added", listed));
		return true;
	}

	void FoodFound(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
	{
		auto* extra = a_stack ? a_stack->extra.get() : nullptr;
		if (!a_list || !a_object || !extra || !Rolls(*a_object)) {
			return;
		}
		// What the sort leaves alone, see Sort in SpawnCondition.cpp.
		if (Loading() || !Private(*a_stack) || UnderSpawnDupe() || BarterCopy(a_list)) {
			return;
		}
		const auto* handing = HandingOver(a_object, a_list);
		if (!handing) {
			FoodBorn(a_list, a_object, extra, a_stack->count, false);
			return;
		}

		// While the barter screen is open the player takes things only by
		// buying them, short of a script. A console command that moves food
		// is the player taking it, so only its additem gives food fresh.
		const char* fresh = nullptr;
		if (Players(a_list)) {
			fresh = RE::TESDataHandler::GetBarterContainer() ? "bought" : FromScript() ? "given by a script" : nullptr;
		}
		if (fresh) {
			const auto unrolled = FoodSpoil::Unrolled(*extra);
			if (FoodSpoil::Freshen(*extra) || unrolled) {
				Tell(true, "{:<30s} [{:08X}] x{:<3d} starts fresh, {:s}", NameOf(*a_object, "unnamed food"), a_object->formID,
					a_stack->count, fresh);
			}
			return;
		}
		if (FoodSpoil::Unrolled(*extra)) {
			const auto* how = handing->fromWorld ? "its first holder took it from the world" : "its first holder took it from a container";
			FoodSpoil::SetStart(*extra, RollIn(*a_list, *a_object, extra, a_stack->count, how, Listed(a_list)));
		}
	}

	std::optional<float> FoundFreshness(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object,
		const RE::ExtraDataList* a_extra, std::uint32_t a_count)
	{
		if (!Rolls(a_object)) {
			return std::nullopt;
		}
		return RollIn(a_list, a_object, a_extra, a_count, "carried as it died", true);
	}
}
