#include "Gameplay/FoodSpoil/Stamp.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/Food.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"

#include <algorithm>
#include <cstdint>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The stored day
		// -------------------------------------------------------------------

		// Added to the day before it is stored, see Stamp.h.
		constexpr float DAY_OFFSET = 10000.0F;

		// What the sliders allow, see fRawFoodDays.
		constexpr float FEWEST_DAYS = 0.25F;
		constexpr float MOST_DAYS = 30.0F;

		float DaysOf(Kind a_kind)
		{
			const auto days = (a_kind == Kind::kCooked ? Settings::fCookedFoodDays : Settings::fRawFoodDays).GetValue();
			return std::clamp(days, FEWEST_DAYS, MOST_DAYS);
		}

		// The lowest starting freshness, so a roll never reads as spoiled.
		constexpr float LEAST_START = 0.01F;

		// Whether a_health is a starting freshness: above the spoiled health,
		// below 1 and not the mark Worn loot leaves on a world item.
		bool IsStart(float a_health)
		{
			return a_health > SPOILED_HEALTH && a_health < 1.0F && !Condition::IsUnrolled(a_health);
		}
	}

	// -------------------------------------------------------------------
	// Who food starts to spoil with
	// -------------------------------------------------------------------

	// The owner is looked up by the handle the list names it with, as
	// BarterCopy in SpawnCondition does under the same lock.
	bool Starts(const RE::BGSInventoryList& a_list)
	{
		if (a_list.owner == RE::ObjectRefHandle{ RE::PlayerCharacter::GetPlayerHandle() }) {
			return true;
		}
		const auto owner = a_list.owner.get();
		const auto* actor = owner ? owner->As<RE::Actor>() : nullptr;
		return actor && (actor->IsPlayerTeammate() || actor->IsDead(true));
	}

	// -------------------------------------------------------------------
	// Found food
	// -------------------------------------------------------------------

	bool Unrolled(RE::ExtraDataList& a_extra)
	{
		const auto health = a_extra.GetHealthPerc();
		return !StampOf(&a_extra) && (health < 0.0F || Condition::IsUnrolled(health));
	}

	void SetStart(RE::ExtraDataList& a_extra, float a_freshness)
	{
		a_extra.SetHealthPerc(std::clamp(a_freshness, LEAST_START, Condition::MAX_HEALTH));
	}

	bool Freshen(RE::ExtraDataList& a_extra)
	{
		if (StampOf(&a_extra) || !(a_extra.GetHealthPerc() > SPOILED_HEALTH)) {
			return false;
		}
		a_extra.SetHealthPerc(Condition::INVALID_HEALTH);
		return true;
	}

	std::optional<float> StartOf(RE::ExtraDataList* a_extra)
	{
		if (!a_extra || !Settings::bSpawnCondition.GetValue() || StampOf(a_extra)) {
			return std::nullopt;
		}
		const auto health = a_extra->GetHealthPerc();
		return IsStart(health) ? std::optional{ health } : std::nullopt;
	}

	// -------------------------------------------------------------------
	// The stamp
	// -------------------------------------------------------------------

	std::optional<float> Today()
	{
		const auto* calendar = RE::Calendar::GetSingleton();
		const auto* days = calendar ? calendar->gameDaysPassed : nullptr;
		return days ? std::optional{ days->GetValue() } : std::nullopt;
	}

	std::optional<float> StampOf(const RE::ExtraDataList* a_extra)
	{
		const auto* left = a_extra ? a_extra->GetByType<RE::ExtraTimeLeft>() : nullptr;
		if (!left || !(left->timeLeft < 0.0F)) {
			return std::nullopt;
		}
		return -left->timeLeft - 1.0F - DAY_OFFSET;
	}

	bool IsSpoiled(RE::ExtraDataList* a_extra)
	{
		if (!a_extra) {
			return false;
		}
		const auto health = a_extra->GetHealthPerc();
		return health >= 0.0F && health <= SPOILED_HEALTH;
	}

	void Stamp(RE::ExtraDataList& a_extra, float a_day)
	{
		const auto stored = -(a_day + DAY_OFFSET) - 1.0F;
		// Written with no lock, as the game writes a light's time left.
		if (auto* left = a_extra.GetByType<RE::ExtraTimeLeft>()) {
			left->timeLeft = stored;
		} else {
			a_extra.AddExtra(new RE::ExtraTimeLeft(stored));
		}
	}

	void StampAt(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, std::uint32_t a_count, float a_start, float a_today)
	{
		// As far back as makes a_start of its time left today.
		const auto day = a_today - (1.0F - std::min(a_start, 1.0F)) * DaysOf(KindOf(a_object));
		Stamp(a_extra, day);
		if (a_extra.GetHealthPerc() > SPOILED_HEALTH) {
			a_extra.SetHealthPerc(Condition::INVALID_HEALTH);
		}
		if (a_start < 1.0F) {
			TraceLog::Line("food", "{:s} [{:08X}] x{:d} found at {:.4f}, stamped on day {:.3f}, today is {:.3f}",
				RE::TESFullName::GetFullName(a_object), a_object.formID, a_count, a_start, day, a_today);
		}
	}

	void Spoil(RE::ExtraDataList& a_extra)
	{
		a_extra.RemoveByType(static_cast<std::uint8_t>(RE::EXTRA_DATA_TYPE::kTimeLeft));
		a_extra.SetHealthPerc(SPOILED_HEALTH);
	}

	float Freshness(Kind a_kind, float a_start, float a_today)
	{
		if (a_kind == Kind::kNever) {
			return 1.0F;
		}
		return std::min(1.0F, 1.0F - (a_today - a_start) / DaysOf(a_kind));
	}

	float FreshnessIn(Kind a_kind, RE::ExtraDataList* a_extra, bool a_fridge, float a_today)
	{
		if (IsSpoiled(a_extra)) {
			return 0.0F;
		}
		const auto stamp = StampOf(a_extra);
		if (!stamp) {
			return StartOf(a_extra).value_or(1.0F);
		}
		return Freshness(a_kind, a_fridge ? Thaw(*stamp, a_today) : *stamp, a_today);
	}

	namespace
	{
		// What FreshnessOf reads, where a_inFridge says whether the food sits in
		// a fridge. It is asked last, since it searches the fridge's stacks.
		template <class FridgeCheck>
		std::optional<float> ReadFreshness(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra, FridgeCheck a_inFridge)
		{
			if (!Spoils(a_object)) {
				return std::nullopt;
			}
			const auto today = Today();
			if (!today) {
				return IsSpoiled(a_extra) ? 0.0F : 1.0F;
			}
			// A trade shows the trader's food fresh, as buying starts it so.
			if (RE::TESDataHandler::GetBarterContainer() && !StampOf(a_extra) && !IsSpoiled(a_extra)) {
				return 1.0F;
			}
			return std::max(0.0F, FreshnessIn(KindOf(a_object), a_extra, a_inFridge(), *today));
		}
	}

	std::optional<float> FreshnessOf(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra)
	{
		return ReadFreshness(a_object, a_extra, [&] { return InOpenFridge(a_extra); });
	}

	std::optional<float> FreshnessOf(const RE::TESBoundObject& a_object, RE::ExtraDataList* a_extra, const RE::TESObjectREFR* a_holder)
	{
		return ReadFreshness(a_object, a_extra, [&] { return InFridge(a_holder, a_extra); });
	}

	// -------------------------------------------------------------------
	// The door
	// -------------------------------------------------------------------

	bool Arrive(const RE::BGSInventoryList& a_list, const RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack,
		const RE::TESObjectREFR* a_from)
	{
		auto*      extra = a_stack.extra.get();
		const auto today = Today();
		if (!extra) {
			// Vanilla's item adds and a container's fill give each stack a list
			// of its own, so none is known to come here.
			TraceLog::Line("food", "{:s} [{:08X}] x{:d} left alone, no extra data list to stamp", RE::TESFullName::GetFullName(a_object),
				a_object.formID, a_stack.count);
			return false;
		}
		if (!today || IsSpoiled(extra)) {
			return false;
		}

		// Stamped food keeps its stamp wherever it goes, rewritten as it
		// goes into or out of a fridge. One whose time ran out turns spoiled
		// before the add, so it joins the spoiled stack.
		if (const auto stamp = StampOf(extra)) {
			const auto out = IsFridgeRef(a_from);
			const auto start = out ? Thaw(*stamp, *today) : *stamp;
			if (Freshness(KindOf(a_object), start, *today) > 0.0F) {
				const auto owner = a_list.owner.get();
				if (const auto into = IsFridgeRef(owner.get()); into != out) {
					Restamp(*extra, a_object, a_stack.count, start, into, *today);
				}
				return true;
			}
			Spoil(*extra);
			TraceLog::Line("food", "{:s} [{:08X}] x{:d} spoiled as it arrived, stamped on day {:.3f}, today is {:.3f}",
				RE::TESFullName::GetFullName(a_object), a_object.formID, a_stack.count, start, *today);
			return false;
		}

		if (!Starts(a_list)) {
			return false;
		}
		StampAt(*extra, a_object, a_stack.count, StartOf(extra).value_or(1.0F), *today);
		return true;
	}
}
