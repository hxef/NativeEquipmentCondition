#include "UI/Repair/Workbench/SpareModBox.h"

#include "Condition/Condition.h"
#include "Condition/Materials/Loose.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Repair/Columns/Columns.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/SpareBox.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Workbench
{
	namespace
	{
		// The rows of the box: SCRAP THE REST, then 1 per mod listed, then
		// KEEP THE REST and CANCEL. The highlight starts on the first mod.
		constexpr std::size_t SCRAP_ROW = 0;
		constexpr std::size_t FIRST_MOD_ROW = 1;

		// A row's cells: its name, a mod's value as a loose mod and its
		// number. Only the name is ever cut.
		constexpr std::size_t ROW_CELLS = 3;
		constexpr std::size_t NAME_CELL_OF_ROW = 0;

		// iSpareModRows at Result. Any other number is Change.
		constexpr std::int32_t ROWS_RESULT = 1;

		// 605 tenths as 60.5, for the trace.
		[[nodiscard]] double AsPercent(std::int32_t a_tenths)
		{
			return a_tenths / 10.0;
		}

		// a_tenths as the box writes it, "-" before a number below 0 and "+"
		// before any other with a_plus. At CND decimals Off always with 1
		// decimal, 60.5, 64.0 and 100.0, while the card shows the whole part.
		// Else as the card writes it, see Condition::CardText, so the box and
		// the card after it agree: 64.0 or 64, and 100.
		[[nodiscard]] std::string Number(std::int32_t a_tenths, bool a_plus)
		{
			// Wide, so the sign comes off even the lowest number.
			const auto  size = static_cast<std::uint32_t>(a_tenths < 0 ? -static_cast<std::int64_t>(a_tenths) : a_tenths);
			std::string out{ a_tenths < 0 ? "-" : (a_plus ? "+" : "") };
			return out + (Condition::Decimals() == Condition::CardDecimals::kOff ? Condition::TenthText(size) : Condition::CardText(size));
		}

		// The number a row of the box ends with, the row bringing the item
		// from a_from to a_to, both in tenths. At Change it is the step,
		// +3.7% with a_plus, -4.2%, or 0.0% with no sign. At Result it is
		// a_to, the condition the repair reaches, 64.2%. Both boxes write
		// every row by it.
		[[nodiscard]] std::string RowNumber(std::int32_t a_from, std::int32_t a_to, bool a_plus)
		{
			if (Settings::iSpareModRows.GetValue() == ROWS_RESULT) {
				return Text::SpareModGain(Number(a_to, false));
			}
			return Text::SpareModGain(Number(a_to - a_from, a_plus));
		}

		// A mod row's answer: the mod, and where its pick brings the item.
		struct ModRow
		{
			std::uint32_t form{ 0 };
			std::string   name;
			std::int32_t  reached{ 0 };
		};
	}

	// -----------------------------------------------------------------------
	// The box that picks the mods
	// -----------------------------------------------------------------------

	void AskMods(RE::TESBoundObject& a_object, const RE::ExtraDataList* a_item, const RE::ExtraDataList* a_spare,
		const Priced& a_priced, std::vector<std::uint32_t> a_picked, KeepMods a_setting)
	{
		const auto gone = [&] {
			TraceLog::Line("menu", "Workbench found the item or the spare gone, so it asks again with nothing used");
			AskWhichSpare(a_object, a_item, a_priced);
		};
		const auto item = RepairsIn(OpenBench()) ? Held(a_object, a_item) : std::nullopt;
		const auto whole = item && item->Worn() ? SpareOn(*item, a_priced, a_spare) : std::nullopt;
		if (!whole) {
			gone();
			return;
		}

		// The mods listed are those not picked, 1 for each form ID picked.
		// The box that picks mods to use keeps every mod listed until it
		// ends, the one that picks mods to keep keeps only the picks.
		const bool                       keep = a_setting == KeepMods::kChooseKeep;
		std::vector<Materials::LooseMod> listed;
		std::vector<std::uint32_t>       picked;
		std::vector<std::uint32_t>       rest;
		auto                             unmatched = a_picked;
		for (const auto& mod : whole->mods) {
			if (const auto it = std::ranges::find(unmatched, mod.FormID()); it != unmatched.end()) {
				unmatched.erase(it);
				picked.push_back(mod.FormID());
			} else {
				listed.push_back(mod);
				rest.push_back(mod.FormID());
			}
		}
		// The plans of KEEP THE REST and SCRAP THE REST.
		auto keepRest = keep ? picked : rest;
		if (keep) {
			keepRest.insert(keepRest.end(), rest.begin(), rest.end());
		}
		const auto scrapRest = keep ? picked : std::vector<std::uint32_t>{};
		const auto plan = SpareOn(*item, a_priced, a_spare, keep ? scrapRest : keepRest);
		if (!plan) {
			gone();
			return;
		}
		// The last mod listed leaves nothing to ask, so its pick ends the box.
		const auto& last = keep ? keepRest : scrapRest;
		if (listed.empty()) {
			Pick(a_object, a_item, a_spare, a_priced, last);
			return;
		}

		// The title is where the plan brings the item, and each row the step
		// 1 more pick makes from there: up as a mod is scrapped, down as a
		// mod is kept.
		const auto title = ShownTenths(*plan, *item, a_priced);
		const auto shown = [&](const std::vector<std::uint32_t>& a_plan) {
			const auto worked = SpareOn(*item, a_priced, a_spare, a_plan);
			return worked ? ShownTenths(*worked, *item, a_priced) : title;
		};
		Columns::Table table;
		table.kept = ROW_CELLS;
		table.name = NAME_CELL_OF_ROW;
		table.rows.push_back({ Text::SpareScrapRest(), std::string{}, RowNumber(shown(keepRest), shown(scrapRest), true) });
		std::vector<ModRow> mods;
		for (std::size_t i = 0; i < listed.size(); i++) {
			auto with = keep ? picked : rest;
			if (keep) {
				with.push_back(rest[i]);
			} else {
				with.erase(with.begin() + static_cast<std::ptrdiff_t>(i));
			}
			const auto reached = shown(with);
			table.rows.push_back({ listed[i].name, Text::SpareModValue(listed[i].caps), RowNumber(title, reached, !keep) });
			mods.push_back({ rest[i], listed[i].name, reached });
		}
		auto* bench = OpenBench();
		auto  rows = Columns::Lay(bench ? bench->uiMovie.get() : nullptr, table, "Workbench mod box");
		rows.push_back(Text::SpareKeepRest());
		TraceLog::Line("menu", "Workbench mod box on {:s}, {:s}: {:d} mods listed, the item at {:.1f}%", item->Name(), keep ? "keep" : "use",
			listed.size(), AsPercent(title));

		// Nothing is used before the box ends, so CANCEL drops the picks.
		const bool full = title >= static_cast<std::int32_t>(Condition::FULL_TENTHS);
		const auto question = keep ? Text::SpareModKeepQuestion(item->Name(), Number(title, false)) :
		                      full ? Text::SpareModFull(item->Name()) :
		                             Text::SpareModQuestion(item->Name(), Number(title, false));
		RepairPrompt::AskRows({}, question, std::move(rows),
			[object = &a_object, a_item, a_spare, priced = a_priced, picks = std::move(a_picked), mods = std::move(mods), keepRest,
				scrapRest, keep, a_setting](std::size_t a_row) {
				const auto use = [&](std::string_view a_by, const std::vector<std::uint32_t>& a_plan) {
					TraceLog::Line("menu", "Workbench mod box ends by {:s}", a_by);
					Pick(*object, a_item, a_spare, priced, a_plan);
				};
				if (a_row == SCRAP_ROW) {
					use("SCRAP THE REST", scrapRest);
					return;
				}
				if (a_row - FIRST_MOD_ROW >= mods.size()) {
					use("KEEP THE REST", keepRest);
					return;
				}
				const auto& mod = mods[a_row - FIRST_MOD_ROW];
				TraceLog::Line("menu", "Workbench mod box {:s} {:s}, the item now at {:.1f}%", keep ? "keeps" : "scraps", mod.name,
					AsPercent(mod.reached));
				if (mods.size() == 1) {
					use("the last mod", keep ? keepRest : scrapRest);
					return;
				}
				auto more = picks;
				more.push_back(mod.form);
				AskMods(*object, a_item, a_spare, priced, std::move(more), a_setting);
			},
			[object = &a_object, a_item, priced = a_priced] {
				TraceLog::Line("menu", "Workbench mod box closed with nothing used, back to the box of spares");
				AskWhichSpare(*object, a_item, priced);
			},
			FIRST_MOD_ROW);
	}
}
