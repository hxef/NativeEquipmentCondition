#include "UI/Hud/PickupBar/Look.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace PickupBar
{
	namespace
	{
		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::OG;

		// -------------------------------------------------------------------
		// Where the game builds the prompt
		// -------------------------------------------------------------------

		// The HUD model's update calls HUDRolloverModel::Update whenever the
		// crosshair's pick or what the prompt reads changed, right after the
		// quick container's update. Not where the state is sent: that happens
		// only when the text changes, so 2 items of the same name side by side
		// would share 1 prompt.
		constexpr CallSite LOOK_SITES[] = {
			{ AE{ 2222046, 0x370 }, OG{ 90433, 0x380 }, "pickup prompt state" },
		};

		std::array<CallPatch::Link<void(RE::HUDRolloverModel*)>, 1> g_lookLink;

		// -------------------------------------------------------------------
		// What the prompt's item reads
		// -------------------------------------------------------------------

		// A whole percent, and whether the item still waits for its first
		// roll.
		struct Reading
		{
			std::int32_t percent{ 0 };
			bool         unrolled{ false };
		};

		// The condition of a weapon or a piece of armor that wears, or the
		// freshness of a food that spoils now, or nothing for anything else.
		// A piece with no condition yet reads full, as its pickup would give
		// it with Worn loot off. With Worn loot on, an item that waits for its
		// taker reads nothing until the take or the purchase rolls it.
		std::optional<Reading> Read(RE::TESObjectREFR& a_ref)
		{
			auto* base = a_ref.GetObjectReference();
			auto* extra = a_ref.extraList.get();
			if (!base) {
				return std::nullopt;
			}
			std::optional<Reading> reading;
			if (base->IsWeapon() || base->Is(RE::ENUM_FORM_ID::kARMO)) {
				if (const auto percent = Condition::Percent(*base, extra)) {
					reading = Reading{ static_cast<std::int32_t>(*percent), extra && Condition::IsUnrolled(extra->GetHealthPerc()) };
				}
			} else if (const auto fresh = FoodSpoil::FreshnessOf(*base, extra)) {
				reading = Reading{ static_cast<std::int32_t>(std::lround(*fresh * 100.0F)), extra && FoodSpoil::Unrolled(*extra) };
			}
			if (reading && reading->unrolled && Settings::bSpawnCondition.GetValue() && SpawnCondition::WaitsForTaker(a_ref)) {
				return std::nullopt;
			}
			return reading;
		}

		// -------------------------------------------------------------------
		// The kept item
		// -------------------------------------------------------------------

		// The game writes it and the HUD reads it, each on its own thread.
		std::mutex            g_keptLock;
		std::optional<Looked> g_kept;

		void Keep(std::optional<Looked> a_looked)
		{
			if (a_looked) {
				TraceLog::Line("cnd", "Pickup prompt shows {:s} at {:d}%", a_looked->name, a_looked->percent);
			}
			const std::scoped_lock l{ g_keptLock };
			g_kept = std::move(a_looked);
		}

		// Rolls a_item on F4SE's task queue, which runs it outside the HUD
		// model's update and every lock. The prompt sends nothing new for the
		// roll, as its text stays the same, so the kept item takes the rolled
		// percent from here while the prompt still shows it.
		void QueueRoll(RE::ObjectRefHandle a_item)
		{
			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				return;
			}
			tasks->AddTask([a_item]() {
				const auto ref = a_item.get();
				if (!ref || !SpawnCondition::RollOnLook(*ref)) {
					return;
				}
				const auto reading = Read(*ref);
				if (!reading) {
					return;
				}
				const std::scoped_lock l{ g_keptLock };
				if (g_kept && g_kept->item == a_item) {
					g_kept->percent = reading->percent;
					TraceLog::Line("cnd", "Pickup prompt's {:s} rolled on the first look, now {:d}%", g_kept->name, reading->percent);
				}
			});
		}

		// Stands in for HUDRolloverModel::Update. The crosshair's pick is the
		// item "E) Take" picks up, which is what the prompt was just built for.
		void LookHk(RE::HUDRolloverModel* a_model)
		{
			g_lookLink[0](a_model);

			if (!g_lookLink[0].Live() || !Settings::bPickupBar.GetValue()) {
				Keep(std::nullopt);
				return;
			}
			auto*      pick = RE::CrosshairPickData::GetSingleton();
			const auto item = pick ? pick->GetTargetHandle() : RE::ObjectRefHandle{};
			const auto ref = item.get();
			const auto reading = ref ? Read(*ref) : std::nullopt;
			const auto* name = reading ? ref->GetDisplayFullName() : nullptr;
			if (!name || !*name) {
				Keep(std::nullopt);
				return;
			}
			Keep(Looked{ name, reading->percent, item });
			if (reading->unrolled) {
				QueueRoll(item);
			}
		}
	}

	// -------------------------------------------------------------------
	// Patching the prompt and reading its item
	// -------------------------------------------------------------------

	bool PatchLook()
	{
		const auto hooks = CallPatch::PerSite<std::size(LOOK_SITES)>([]<std::size_t I>() { return &LookHk; });
		return CallPatch::PatchAll(LOOK_SITES, RE::ID::HUDRolloverModel::Update, hooks, g_lookLink,
			       "The pickup prompt notes the condition of the item it shows") > 0;
	}

	std::optional<Looked> Kept()
	{
		const std::scoped_lock l{ g_keptLock };
		return g_kept;
	}
}
