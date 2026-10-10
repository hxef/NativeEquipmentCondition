#include "UI/Repair/Workbench/Display.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Label.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Workbench
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Params = Scaleform::GFx::FunctionHandler::Params;

		// -------------------------------------------------------------------
		// Fading the names of equipped rows
		// -------------------------------------------------------------------

		// The list down the left of the bench, showing whichever of its lists
		// is in use, and the inventory it shows first. BaseInstance is the menu
		// itself.
		constexpr const char* LIST_PATH = "_root.BaseInstance.InventoryBase_mc.InventoryList_mc";
		constexpr const char* INVENTORY_PATH = "_root.BaseInstance.InventoryListObject";

		// How strongly the bench draws a row of no use.
		constexpr double FADED = 0.5;

		// A stop for the walk along the rows. The list keeps 6 clips and hands
		// them round.
		constexpr std::uint32_t MAX_ROWS = 64;

		// No row at all, which is where the list puts the clips it is not
		// using.
		constexpr std::uint32_t NO_ROW = std::numeric_limits<std::uint32_t>::max();

		// Which row a clip is showing. The list reuses its clips for other rows
		// as it scrolls, so this is asked every time.
		[[nodiscard]] std::uint32_t RowOf(const Value& a_clip)
		{
			const auto row = Flash::Number(a_clip, "itemIndex"sv, -1.0);
			return row >= 0.0 && row < NO_ROW ? static_cast<std::uint32_t>(row) : NO_ROW;
		}

		// Whether a row is one MarkWorn greyed. The bench marks every row live
		// as it builds it, so a row neither live nor buildable is one of this
		// plugin's.
		[[nodiscard]] bool Faded(const Value& a_row)
		{
			return !Flash::Bool(a_row, "enabled"sv) && !Flash::Bool(a_row, "hasRequired"sv);
		}

		// Called by the bench every frame. The rows are walked, not watched,
		// since a row redraws whenever it is scrolled past, picked or put down
		// and sends no event. Only a name that should be faded and is not is
		// written. The button's word is kept first, since the checks below
		// can return.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (a_params.movie) {
					Relabel(*a_params.movie);
				}

				// The inventory answers what is picked only while it is the
				// list on show. In the slots and the mods the rows belong to
				// somebody else.
				Value inventory;
				Value picked;
				Value list;
				Value rows;
				if (!a_params.movie ||
					!a_params.movie->GetVariable(&inventory, INVENTORY_PATH) || !inventory.IsObject() ||
					!inventory.GetMember("selectedEntry"sv, &picked) || !picked.IsObject() ||
					!a_params.movie->GetVariable(&list, LIST_PATH) || !list.IsDisplayObject() ||
					!list.GetMember("entryList"sv, &rows) || !rows.IsArray()) {
					return;
				}

				for (std::uint32_t i = 0; i < MAX_ROWS; i++) {
					Value      clip;
					const auto index = Value(i);
					if (!list.Invoke("GetClipByIndex", &clip, &index, 1) || !clip.IsDisplayObject()) {
						break;
					}

					Value row;
					Value name;
					const auto at = RowOf(clip);
					if (at == NO_ROW || !rows.GetElement(at, &row) || !row.IsObject() || !Faded(row) ||
						!clip.GetMember("textField"sv, &name) || !name.IsDisplayObject()) {
						continue;
					}

					if (std::abs(Flash::Number(name, "alpha"sv, 1.0) - FADED) > 0.01) {
						name.SetMember("alpha"sv, Value(FADED));

						// Said once per bench, and only when something was
						// written.
						if (!said) {
							said = true;
							TraceLog::Line("menu", "Workbench faded an equipped row, {:s}",
								Flash::String(name, "text"sv));
						}
					}
				}
			}

			void Reset()
			{
				said = false;
			}

		private:
			bool said = false;
		};

		FrameListener g_frameListener;

		// -------------------------------------------------------------------
		// The rows greyed or listed for repairs
		// -------------------------------------------------------------------

		// The items MarkWorn has listed since the bench opened, by their
		// inventory handle, which is the item's and not a stack's. The bench
		// is a menu, so only the menu's own thread comes here.
		std::vector<std::uint32_t> g_listed;

		[[nodiscard]] bool Listed(std::uint32_t a_handle)
		{
			return std::ranges::find(g_listed, a_handle) != g_listed.end();
		}
	}

	void Dim(RE::ExamineMenu* a_menu, Value& a_list, std::string_view a_what)
	{
		Value rows;
		if (!a_menu || !a_list.IsObject() ||
			!a_list.GetMember("entryList"sv, &rows) || !rows.IsArray()) {
			return;
		}

		const auto count = rows.GetArraySize();
		for (std::uint32_t i = 0; i < count; i++) {
			Value row;
			if (!rows.GetElement(i, &row) || !row.IsObject()) {
				continue;
			}
			row.SetMember("hasRequired"sv, Value(false));
			row.SetMember("enabled"sv, Value(false));
			row.SetMember("hasLooseMod"sv, Value(false));
		}

		a_list.SetMember("entryList"sv, rows);
		a_list.Invoke("RefreshList");
		Flash::Call(a_menu->menuObj, "UpdateButtons");

		TraceLog::Line("menu", "Workbench greyed out {:d} {:s} rows", count, a_what);
	}

	void Clear(RE::ExamineMenu* a_menu, Value& a_list)
	{
		if (!a_menu || !a_menu->uiMovie || !a_list.IsObject()) {
			return;
		}

		Value empty;
		a_menu->uiMovie->CreateArray(&empty);
		a_list.SetMember("entryList"sv, empty);
		a_list.Invoke("RefreshList");
		Flash::Call(a_menu->menuObj, "UpdateButtons");
	}

	void MarkWorn(RE::ExamineMenu* a_menu)
	{
		Value rows;
		if (!a_menu || !a_menu->itemList.IsObject() ||
			!a_menu->itemList.GetMember("entryList"sv, &rows) || !rows.IsArray()) {
			return;
		}

		// The flag the bench's own rows carry is the one its filter shows,
		// weapons at the weapon bench and apparel at the armor bench. A row the
		// bench left out carries 0, which the filter never shows.
		const auto kind = WorksOn(a_menu);
		Value      filterer;
		Value      shown;
		const bool lists = kind && a_menu->itemList.GetMember("filterer"sv, &filterer) && filterer.IsObject() &&
		                   filterer.GetMember("itemFilter"sv, &shown) && Flash::AsNumber(shown) != 0.0;

		const auto&   carried = a_menu->invInterface.stackedEntries;
		const auto    count = std::min(rows.GetArraySize(), static_cast<std::uint32_t>(carried.size()));
		std::uint32_t listed = 0;
		std::uint32_t faded = 0;
		std::uint32_t inHand = 0;

		for (std::uint32_t i = 0; i < count; i++) {
			Value row;
			if (!rows.GetElement(i, &row) || !row.IsObject()) {
				continue;
			}

			// A row left out is listed while its item is worn, and once listed
			// it stays so.
			const Selection item{ SelectedItem::Read(carried[static_cast<std::size_t>(i)]) };
			const bool      list = lists && item.object && item.kind == *kind && (item.Worn() || Listed(item.handle)) &&
			                  Flash::Number(row, "filterFlag"sv, 1.0) == 0.0;
			if (!list && !item.TooWorn()) {
				continue;
			}

			if (list) {
				row.SetMember("filterFlag"sv, shown);
				if (!Listed(item.handle)) {
					g_listed.push_back(item.handle);
				}
				listed++;
			}
			if (item.TooWorn()) {
				faded++;
				if (Flash::Number(row, "equipState"sv, 0.0) > 0.0) {
					inHand++;
				}
			}
			row.SetMember("enabled"sv, Value(false));
			row.SetMember("hasRequired"sv, Value(false));
		}

		if (listed > 0) {
			TraceLog::Line("menu", "Workbench listed {:d} items no mod here fits, for repairs", listed);
		}
		if (faded > 0) {
			TraceLog::Line("menu", "Workbench faded {:d} of {:d} rows, {:d} of them equipped",
				faded, count, inHand);
		}
	}

	bool NoModFits(const SelectedItem::Item& a_item)
	{
		return a_item.object && Listed(a_item.handle);
	}

	void ForgetListed()
	{
		g_listed.clear();
	}

	// -------------------------------------------------------------------
	// Starting the fade on each bench
	// -------------------------------------------------------------------

	void WatchEquipped(Scaleform::GFx::Movie& a_movie)
	{
		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The workbench has no stage to listen on, so a worn item equipped is drawn at full strength.");
			return;
		}

		// A new bench has logged nothing yet.
		g_frameListener.Reset();

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The workbench refused the frame listener, so a worn item equipped is drawn at full strength.");
		}
	}
}
