#include "UI/Hud/QuickContainer/Rows.h"

#include "Condition/Condition.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <format>
#include <mutex>
#include <string_view>

namespace QuickContainer
{
	namespace
	{
		using CallPatch::CallSite;

		// -------------------------------------------------------------------
		// Where the game builds the rows
		// -------------------------------------------------------------------

		// HUDQuickContainerDataModel::AddItemRows builds the rows, one
		// InventoryItemDisplayData each, once the model knows the container and
		// before it sends them to the HUD.
		constexpr CallSite ROWS_SITES[] = {
			{ 2221633, 0xA64, "quick container rows" },
		};

		// The one place AddItemRows makes a row for an inventory entry. The row
		// of dots and the row for a locked container come from elsewhere.
		constexpr CallSite ROW_SITES[] = {
			{ RE::ID::HUDQuickContainerDataModel::AddItemRows.id(), 0x26F, "quick container row" },
		};

		// -------------------------------------------------------------------
		// The rows of one build
		// -------------------------------------------------------------------

		// One row as the game built it and the condition of the stack behind
		// it. The better mark is kept for the trace alone.
		struct Row
		{
			RE::BSFixedStringCS name;
			std::uint32_t       count{ 0 };
			std::int32_t        percent{ NO_CONDITION };
			bool                better{ false };
		};

		// The rows of one build, in the order the widget shows them.
		struct Rows
		{
			std::array<Row, MAX_ROWS> rows;
			std::size_t               size{ 0 };
		};

		bool Same(const Rows& a_lhs, const Rows& a_rhs)
		{
			if (a_lhs.size != a_rhs.size) {
				return false;
			}
			for (std::size_t i = 0; i < a_lhs.size; i++) {
				const auto& lhs = a_lhs.rows[i];
				const auto& rhs = a_rhs.rows[i];
				if (!(lhs.name == rhs.name) || lhs.count != rhs.count || lhs.percent != rhs.percent) {
					return false;
				}
			}
			return true;
		}

		// How many builds are kept for the HUD to match against. The HUD shows
		// a build a frame later and every move of the selection makes a new
		// one.
		constexpr std::size_t KEPT_BUILDS = 4;

		// The kept builds, newest first. The game writes and the HUD reads,
		// each on its own thread.
		std::mutex                    g_buildsLock;
		std::array<Rows, KEPT_BUILDS> g_builds;

		// The rows as the trace last wrote them, kept apart since the better
		// mark changes only the trace.
		std::mutex  g_tracedLock;
		std::string g_traced;

		// One line whenever the rows change, as the widget shows them.
		void TraceRows(const Rows& a_rows)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}

			std::string text;
			for (std::size_t i = 0; i < a_rows.size; i++) {
				const auto& row = a_rows.rows[i];
				text += i == 0 ? "" : ", ";
				text += row.name.c_str();
				if (row.count > 1) {
					text += std::format(" ({:d})", row.count);
				}
				if (row.percent >= 0) {
					text += std::format(" {:d}%", row.percent);
				}
				if (row.better) {
					text += " better";
				}
			}

			{
				const std::scoped_lock l{ g_tracedLock };
				if (text == g_traced) {
					return;
				}
				g_traced = text;
			}
			TraceLog::Line("cnd", "quick container rows {:s}", text.empty() ? "none"sv : std::string_view{ text });
		}

		void Publish(const Rows& a_rows)
		{
			const std::scoped_lock l{ g_buildsLock };
			if (Same(g_builds.front(), a_rows)) {
				return;
			}
			std::shift_right(g_builds.begin(), g_builds.end(), 1);
			g_builds.front() = a_rows;
		}

		// -------------------------------------------------------------------
		// Noting each row's condition as the game builds it
		// -------------------------------------------------------------------

		// The condition of the item an entry stands for, or NO_CONDITION. The
		// entry names its item by inventory handle and its stacks by index, and
		// the first stack is where the row's name comes from.
		std::int32_t ConditionOf(const RE::InventoryUserUIInterfaceEntry& a_entry)
		{
			const auto* inventory = RE::BGSInventoryInterface::GetSingleton();
			const auto* item = inventory && !a_entry.stackIndex.empty() ? inventory->RequestInventoryItem(a_entry.invHandle.id) : nullptr;
			const auto* stack = item ? item->GetStackByID(a_entry.stackIndex[0]) : nullptr;
			const auto  percent = stack ? Condition::Percent(*item, stack) : std::nullopt;
			return percent ? static_cast<std::int32_t>(*percent) : NO_CONDITION;
		}

		// The conditions of the rows AddItemRows is making. The 2 hooks run one
		// inside the other on one thread, and this is how the inner hands the
		// outer what it found.
		struct Building
		{
			std::array<std::int32_t, MAX_ROWS> percents{};
			std::size_t                        made{ 0 };
		};

		thread_local Building* t_building = nullptr;

		// Stands in for InventoryItemDisplayData's constructor in AddItemRows.
		// The owner's handle arrives by address.
		RE::InventoryItemDisplayData* RowHk(RE::InventoryItemDisplayData* a_this, const RE::ObjectRefHandle& a_inventoryRef,
			const RE::InventoryUserUIInterfaceEntry& a_entry)
		{
			const REL::Relocation<decltype(&RowHk)> original{ RE::ID::InventoryItemDisplayData::ctor };
			const auto                              result = original(a_this, a_inventoryRef, a_entry);

			if (auto* building = t_building; building && building->made < MAX_ROWS) {
				building->percents[building->made++] = ConditionOf(a_entry);
			}
			return result;
		}

		// Stands in for AddItemRows. a_state is the state it fills.
		void RowsHk(RE::HUDQuickContainerDataModel* a_model, RE::QuickContainerStateData& a_state)
		{
			Building building;
			t_building = &building;
			a_model->AddItemRows(a_state);
			t_building = nullptr;

			// AddItemRows makes its item rows before the row of dots, so the
			// conditions line up with the first rows.
			Rows        rows;
			const auto& items = a_state.itemData;
			rows.size = std::min<std::size_t>(items.size(), MAX_ROWS);
			for (std::size_t i = 0; i < rows.size; i++) {
				auto& row = rows.rows[i];
				row.name = items[i].itemName;
				row.count = items[i].itemCount;
				row.percent = i < building.made ? building.percents[i] : NO_CONDITION;
				row.better = items[i].isBetterThanEquippedItem;
			}
			Publish(rows);
			TraceRows(rows);
		}
	}

	// -------------------------------------------------------------------
	// Matching the rows on screen to a build
	// -------------------------------------------------------------------

	std::optional<std::array<std::int32_t, MAX_ROWS>> FindConditions(const std::array<Shown, MAX_ROWS>& a_shown, std::size_t a_size)
	{
		const std::scoped_lock l{ g_buildsLock };
		for (const auto& build : g_builds) {
			if (build.size != a_size) {
				continue;
			}

			auto same = true;
			for (std::size_t i = 0; i < a_size && same; i++) {
				same = build.rows[i].count == a_shown[i].count && a_shown[i].text == build.rows[i].name.c_str();
			}
			if (!same) {
				continue;
			}

			std::array<std::int32_t, MAX_ROWS> percents{};
			percents.fill(NO_CONDITION);
			for (std::size_t i = 0; i < build.size; i++) {
				percents[i] = build.rows[i].percent;
			}
			return percents;
		}
		return std::nullopt;
	}

	bool PatchRows()
	{
		const auto row = CallPatch::PatchAll(ROW_SITES, RE::ID::InventoryItemDisplayData::ctor,
			CallPatch::Repeat<std::size(ROW_SITES)>(reinterpret_cast<std::uintptr_t>(&RowHk)),
			"Quick container rows note their CND");

		const auto rows = CallPatch::PatchAll(ROWS_SITES, RE::ID::HUDQuickContainerDataModel::AddItemRows,
			CallPatch::Repeat<std::size(ROWS_SITES)>(reinterpret_cast<std::uintptr_t>(&RowsHk)),
			"Quick container rows hand their CND to the HUD");

		return row > 0 && rows > 0;
	}
}
