#include "UI/Hud/QuickContainer/Rows.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
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

		// The row AddItemRows adds last when more entries follow, see
		// HUDQuickContainerDataModel.h. RowHk never sees it.
		constexpr std::string_view DOTS_ROW{ "..." };

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

		std::array<CallPatch::Link<RE::InventoryItemDisplayData*(RE::InventoryItemDisplayData*, const RE::ObjectRefHandle&, const RE::InventoryUserUIInterfaceEntry&)>, 1> g_rowLink;
		std::array<CallPatch::Link<void(RE::HUDQuickContainerDataModel*, RE::QuickContainerStateData&)>, 1> g_rowsLink;

		// Stands in for InventoryItemDisplayData's constructor in AddItemRows.
		// The owner's handle arrives by address.
		RE::InventoryItemDisplayData* RowHk(RE::InventoryItemDisplayData* a_this, const RE::ObjectRefHandle& a_inventoryRef,
			const RE::InventoryUserUIInterfaceEntry& a_entry)
		{
			const auto result = g_rowLink[0](a_this, a_inventoryRef, a_entry);

			if (auto* building = t_building; building && building->made < MAX_ROWS && g_rowLink[0].Live()) {
				building->percents[building->made++] = ConditionOf(a_entry);
			}
			return result;
		}

		// Stands in for AddItemRows. a_state is the state it fills.
		void RowsHk(RE::HUDQuickContainerDataModel* a_model, RE::QuickContainerStateData& a_state)
		{
			if (!g_rowsLink[0].Live()) {
				g_rowsLink[0](a_model, a_state);
				return;
			}
			Building building;
			t_building = &building;
			g_rowsLink[0](a_model, a_state);
			t_building = nullptr;

			// AddItemRows makes its item rows first, each through RowHk, then
			// the row of dots when more entries follow. Rows are matched to
			// conditions by order, so a skipped row would shift every later
			// meter. A build whose item rows do not line up with the
			// conditions noted shows no meters, which is safe.
			Rows        rows;
			const auto& items = a_state.itemData;
			rows.size = std::min<std::size_t>(items.size(), MAX_ROWS);
			const bool dots = rows.size == building.made + 1 && items[rows.size - 1].itemName == DOTS_ROW;
			const bool aligned = building.made == rows.size || dots;
			for (std::size_t i = 0; i < rows.size; i++) {
				auto& row = rows.rows[i];
				row.name = items[i].itemName;
				row.count = items[i].itemCount;
				row.percent = aligned && i < building.made ? building.percents[i] : NO_CONDITION;
				row.better = items[i].isBetterThanEquippedItem;
			}
			Publish(rows);
			TraceRows(rows);
		}

		// -------------------------------------------------------------------
		// Reading a name on screen
		// -------------------------------------------------------------------

		// The characters a sorting tag opens and closes with.
		constexpr std::string_view TAG_OPENS{ "[({|" };
		constexpr std::string_view TAG_CLOSES{ "])}|" };

		// Whether a row's text is the name the game built. A UI mod may put a
		// sorting tag and a space before the name once the rows reach the
		// widget and draw the tag as an icon, so "Shotgun Shell" reads
		// "[AmmoShells] Shotgun Shell".
		bool SameName(std::string_view a_shown, std::string_view a_name)
		{
			if (a_shown == a_name) {
				return true;
			}
			if (a_shown.size() < a_name.size() + 3 || !a_shown.ends_with(a_name)) {
				return false;
			}
			const auto tag = a_shown.substr(0, a_shown.size() - a_name.size() - 1);
			return a_shown[tag.size()] == ' ' && TAG_OPENS.contains(tag.front()) && TAG_CLOSES.contains(tag.back());
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
				same = build.rows[i].count == a_shown[i].count && SameName(a_shown[i].text, build.rows[i].name.c_str());
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
		const auto rowHooks = CallPatch::PerSite<std::size(ROW_SITES)>([]<std::size_t I>() { return &RowHk; });
		const auto row = CallPatch::PatchAll(ROW_SITES, RE::ID::InventoryItemDisplayData::ctor, rowHooks, g_rowLink,
			"Quick container rows note their CND");

		const auto rowsHooks = CallPatch::PerSite<std::size(ROWS_SITES)>([]<std::size_t I>() { return &RowsHk; });
		const auto rows = CallPatch::PatchAll(ROWS_SITES, RE::ID::HUDQuickContainerDataModel::AddItemRows, rowsHooks, g_rowsLink,
			"Quick container rows hand their CND to the HUD");

		return row > 0 && rows > 0;
	}
}
