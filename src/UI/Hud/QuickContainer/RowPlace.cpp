#include "UI/Hud/QuickContainer/RowLayout.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Roles/Hud.h"
#include "UI/Roles/QuickRow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <string>

namespace QuickContainer
{
	using Scaleform::GFx::Value;

	namespace
	{
		// The middle of a line's capitals and digits below its top, by the
		// font's own ascent and CAPITALS_SHARE, in the field's own units.
		double CapitalsMiddle(const Flash::TextLine& a_line)
		{
			return a_line.baseline - (CAPITALS_SHARE * (a_line.baseline - a_line.top) / 2.0);
		}

		// a_line, the first line of a_name as drawn, with the ascent its letters
		// really have. A shrunk name reads a whole ascent from getLineMetrics,
		// 15 for the 15.83 it draws at textHeight 20. The number's line holds
		// the same letters at full size, so the shown name's ascent is the
		// number's times the share of its height the name keeps. A hidden
		// name or one of more lines keeps its line as read.
		Flash::TextLine Unrounded(const Flash::TextLine& a_line, const Value& a_name, const Value& a_number, const Flash::TextLine& a_own)
		{
			const auto shown = Flash::Number(a_name, "textHeight"sv);
			const auto full = Flash::Number(a_number, "textHeight"sv);
			if (!Flash::Bool(a_name, "visible"sv) || Flash::Number(a_name, "numLines"sv) != 1.0 || !(shown > 0.0) || !(full > shown)) {
				return a_line;
			}
			auto line = a_line;
			line.baseline = line.top + ((a_own.baseline - a_own.top) * shown / full);
			return line;
		}

		// The measures a row's percent is centred by, for the trace: the
		// name's first line as getLineMetrics gives it and as it is drawn,
		// its field's box, and the number's line.
		void TraceMeasures(std::size_t a_index, Value& a_name, const Flash::TextLine& a_line, const Flash::TextLine& a_own)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}
			Value       metrics;
			const Value line{ 0 };
			if (!a_name.Invoke("getLineMetrics", &metrics, &line, 1) || !metrics.IsObject()) {
				return;
			}
			TraceLog::Line("menu",
				"Quick container row {:d} name's line 0 has ascent {:.2f}, descent {:.2f}, height {:.2f} and leading {:.2f}, drawn {:.2f} lower, in a field at y {:.2f}, {:.2f} high at scaleY {:.3f} with textHeight {:.2f}, and the CND's line has its top at {:.2f} and its baseline at {:.2f}",
				a_index, Flash::Number(metrics, "ascent"sv), Flash::Number(metrics, "descent"sv), Flash::Number(metrics, "height"sv), Flash::Number(metrics, "leading"sv),
				a_line.drop, Flash::Number(a_name, "y"sv), Flash::Number(a_name, "height"sv), Flash::Number(a_name, "scaleY"sv, 1.0), Flash::Number(a_name, "textHeight"sv),
				a_own.top, a_own.baseline);
		}
	}

	std::string Standing(std::optional<double> a_edge)
	{
		return a_edge ? std::format("standing at the row's right edge at {:.1f}", *a_edge) : "standing after the name"s;
	}

	// -------------------------------------------------------------------
	// Where a row's CND stands
	// -------------------------------------------------------------------

	// After the name's end and its icons, the first icon 4 past that end as
	// CalcIconWidth counts it. With bQuickContainerCndAtRight the CND's right
	// end stands at a_end, the row's right edge, instead: the smallest end of
	// the rooms the movie gives the text of the list's rows with a CND, see
	// Measure, so every row's CND ends at 1 x. A HUD mod may move a row's
	// name field further right than it narrows it, as for a row with an icon
	// before its name, so rows end their room at different x. The name
	// narrows by the CND in both places, so a centred name stays centred in
	// the room left of it. The CND stays after the name where the shown text
	// reaches its place, or where a component of the row or its list shows
	// there. That is looked for again only when the place moves or the row
	// redraws.
	Meters::Spot Meters::Place(Scaleform::GFx::Movie& a_movie, Value& a_row, const Value& a_name, Drawn& a_drawn, double a_nameEnd, double a_icons, double a_width,
		double a_end, bool a_afterRedraw)
	{
		const Spot after{ a_nameEnd + a_icons + METER_GAP, std::nullopt };
		if (!Settings::bQuickContainerCndAtRight.GetValue()) {
			return after;
		}
		const auto left = Flash::Number(a_name, "x"sv);
		const auto x = a_end - a_width;
		if (!std::isfinite(x) || x < after.x) {
			return after;
		}
		if (a_afterRedraw || a_drawn.probed != x) {
			const auto top = Flash::Number(a_name, "y"sv);
			const auto bottom = top + Flash::Number(a_name, "height"sv);
			a_drawn.probed = x;
			a_drawn.edgeFree = Roles::QuickRow::Clear(a_row, Flash::Box{ x, top, a_end, bottom }, Flash::Box{ left, top, a_end, bottom });
			if (!a_drawn.edgeFree) {
				Roles::Noted(a_movie, "free right edge on a quick container row"sv, "that row's CND follows its name"sv);
			}
		}
		return a_drawn.edgeFree ? Spot{ x, a_end } : after;
	}

	// -------------------------------------------------------------------
	// A row's number at Percent
	// -------------------------------------------------------------------

	void Meters::HideNumber(Value& a_row, Drawn& a_drawn)
	{
		if (!a_drawn.numberShown) {
			return;
		}
		auto number = Flash::Child(a_row, NUMBER_NAME);
		Flash::Set(number, "visible"sv, Value(false));
		a_drawn.numberShown = false;
		a_drawn.number.clear();
	}

	// The widest text the row can write: CND and the number of a worn item
	// at its most or of a full one, see HudParts::Readout::WIDEST, in the
	// name's letters. Measured again only when those texts or letters
	// change, as CND decimals or a new name font does, and a measure leaves
	// the number to be written again.
	std::optional<double> Meters::Widest(Value& a_number, const Value& a_name, Drawn& a_drawn)
	{
		const auto letters = LettersOf(a_name);
		if (!letters) {
			return std::nullopt;
		}
		// The word is read back from the field, so it is translated once a
		// HUD movie and the field written afresh after.
		if (a_drawn.word.empty()) {
			a_drawn.word = HudParts::Translated(a_number, HudParts::CND_TEXT);
			a_drawn.number.clear();
		}
		std::array<std::string, HudParts::Readout::WIDEST.size()> texts;
		std::ranges::transform(HudParts::Readout::WIDEST, texts.begin(), [&](std::uint32_t a_tenths) { return a_drawn.word + " " + HudParts::Readout::PercentText(a_tenths); });
		const auto of = texts[0] + "|" + texts[1];
		if (letters == a_drawn.letters && of == a_drawn.widestOf) {
			return a_drawn.widest;
		}
		std::optional<double> widest;
		for (const auto& text : texts) {
			const auto wide = Write(a_number, text, *letters, false);
			if (!wide) {
				return std::nullopt;
			}
			widest = std::max(widest.value_or(*wide), *wide);
		}
		a_drawn.letters = letters;
		a_drawn.widestOf = of;
		a_drawn.widest = *widest;
		a_drawn.number.clear();
		return widest;
	}

	// Writes the row's number when it or the highlight changes, gives it the
	// colour the name has of its own, as the meter takes it, and puts its
	// letters where Place says, not rounded, so every row's text ends at 1
	// x. Its capitals and digits are centred on the name's as the game draws
	// them after its own shrink, each line's capitals' middle taken from the
	// font's own ascent and baseline by CAPITALS_SHARE.
	void Meters::ShowNumber(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, Value& a_number, Value& a_name, const Reading& a_reading, double a_icons,
		double a_end, bool a_afterRedraw)
	{
		auto&      last = drawn[a_index];
		const auto selected = Flash::Bool(a_row, "selected"sv);
		const auto number = HudParts::Readout::PercentText(a_reading.tenths);
		const auto text = last.word + " " + number;
		const auto renumbered = number != last.number;
		const auto changed = renumbered || selected != last.numberSelected;
		if (changed) {
			last.numberWidth = Write(a_number, text, *last.letters, selected).value_or(last.widest);
			last.number = number;
			last.numberSelected = selected;
		}
		if (changed || a_afterRedraw) {
			const auto wanted = Flash::TintOf(a_name);
			const auto now = Flash::TintOf(a_number);
			if (wanted && (!now || !now->Near(*wanted))) {
				Flash::SetTint(a_number, *wanted);
			}
		}

		const auto nameEnd = Roles::Hud::TextEnd(a_movie, a_row);
		if (!nameEnd) {
			Roles::Noted(a_movie, "quick container name's end"sv, "that row shows no CND"sv);
			HideNumber(a_row, last);
			return;
		}
		const auto spot = Place(a_movie, a_row, a_name, last, *nameEnd, a_icons, last.numberWidth, a_end, a_afterRedraw);
		const auto x = spot.x;
		const auto moved = x != last.numberX || !last.numberShown;
		if (!changed && !a_afterRedraw && !moved) {
			return;
		}
		const auto name = Flash::DrawnLine(a_name);
		const auto own = Flash::FirstLine(a_number);
		if (!name || !own) {
			Roles::Noted(a_movie, "quick container name's line"sv, "that row shows no CND"sv);
			HideNumber(a_row, last);
			return;
		}
		const auto middle = Flash::Number(a_name, "y"sv) + (CapitalsMiddle(Unrounded(*name, a_name, a_number, *own)) * Flash::Number(a_name, "scaleY"sv, 1.0));
		Flash::Set(a_number, "x"sv, Value(x - (Flash::FIELD_MARGIN * Flash::Number(a_number, "scaleX"sv, 1.0))));
		Flash::Set(a_number, "y"sv, Value(middle - (CapitalsMiddle(*own) * Flash::Number(a_number, "scaleY"sv, 1.0))));
		Flash::Set(a_number, "visible"sv, Value(true));
		last.numberX = x;
		last.numberShown = true;
		if (renumbered || moved) {
			TraceLog::Line("menu", "Quick container row {:d} CND drawn as {:s} at {:.0f}, after icons {:.0f} wide from the name's end at {:.1f}, {:s}, centred on the name's capitals at y {:.1f}",
				a_index, text, x, a_icons, *nameEnd, Standing(spot.edge), middle);
			TraceMeasures(a_index, a_name, *name, *own);
		}
	}
}
