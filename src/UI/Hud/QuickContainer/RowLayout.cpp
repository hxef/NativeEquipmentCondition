#include "UI/Hud/QuickContainer/RowLayout.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Roles/Hud.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace QuickContainer
{
	using Scaleform::GFx::Value;

	namespace
	{
		// A row's number field, made hidden the first time it is asked for,
		// or a value that is not a display object when the movie refuses
		// it. A row that never shows a number holds no field.
		Value NumberOf(Scaleform::GFx::Movie& a_movie, Value& a_row)
		{
			if (auto made = Flash::Child(a_row, NUMBER_NAME); made.IsDisplayObject()) {
				return made;
			}
			if (!HudParts::AddField(a_movie, a_row, NUMBER_NAME)) {
				return Value{};
			}
			auto made = Flash::Child(a_row, NUMBER_NAME);
			Flash::Set(made, "visible"sv, Value(false));
			return made;
		}
	}

	// -------------------------------------------------------------------
	// A row's name width and layout
	// -------------------------------------------------------------------

	// Whether a_field's width is still NEC's last write in a_drawn.
	bool Meters::HoldsWrite(const Value& a_field, const Drawn& a_drawn)
	{
		return a_drawn.written && std::abs(Flash::Number(a_field, "width"sv) - *a_drawn.written) < SAME_WIDTH;
	}

	// Writes the name's width. A centred name's text moves when its
	// field narrows, so the parts the movie placed against the text's
	// start move as far, and the row lays its icons out again after
	// the name's new end. A HUD mod may paint the row's text fields
	// plain on that call, so each shown text field gets back the
	// colour it had.
	void Meters::WriteWidth(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, Value& a_field, const Value& a_meter, double a_width)
	{
		auto before = Roles::Hud::TextStart(a_movie, a_row, a_field, a_meter);
		a_field.SetMember("width"sv, Value(a_width));
		const auto after = before && !before->against.empty() ? Roles::Hud::TextStart(a_movie, a_row, a_field, a_meter) : std::nullopt;
		if (after) {
			for (auto& part : before->against) {
				Flash::Set(part, "x"sv, Value(Flash::Number(part, "x"sv) + after->x - before->x));
			}
		}
		auto kept = Flash::TextTints(a_row);
		a_row.Invoke("AddIconsToEntry");
		if (const auto back = Flash::PutBackTints(kept); back > 0) {
			TraceLog::Line("menu", "Quick container row {:d} text colour put back on {:d} of {:d} shown text fields after its icon layout", a_index, back, kept.size());
		}
	}

	// Hides the meter of a row NEC drew into or narrowed, and gives the
	// name back the width the meter took, but only while the name still
	// holds NEC's write, so a width the movie set since stays. A row with
	// no data is hidden and sets its width at its next redraw, and
	// AddIconsToEntry fails on it, so it gets no give back.
	void Meters::Withdraw(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index)
	{
		auto& last = drawn[a_index];
		if (last.percent == NOT_DRAWN && !last.visible && !last.numberShown && !last.written) {
			return;
		}
		HideNumber(a_row, last);

		Value field;
		auto  meter = Flash::Child(a_row, METER_NAME);
		if (!a_row.IsDisplayObject() || !a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
			return;
		}
		Hide(meter, last);

		Value data;
		if (a_row.GetMember("data"sv, &data) && data.IsObject() && HoldsWrite(field, last)) {
			WriteWidth(a_movie, a_row, a_index, field, meter, last.base);
		}
		last = Drawn{};
	}

	// Lays out one row. a_reading's percent is NO_CONDITION for an item
	// that does not wear, and a_reading is nothing when no kept build
	// matches. a_asNumber is true at Percent.
	void Meters::Apply(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, const Shown& a_shown, std::optional<Reading> a_reading, bool a_asNumber,
		bool a_afterRedraw)
	{
		Value field;
		auto  meter = Flash::Child(a_row, METER_NAME);
		if (!a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
			return;
		}
		auto& last = drawn[a_index];

		// The row makes room for its icons by narrowing the name by
		// what CalcIconWidth says, and a movie may narrow it more for
		// icons of its own. Narrowing it by the meter too keeps name,
		// icons and meter centred and shrinks a long name just enough.
		Value iconsWidth;
		a_row.Invoke("CalcIconWidth", &iconsWidth);
		const auto icons = Flash::AsNumber(iconsWidth);

		// A slot showing another item takes its name's width afresh,
		// since the movie's new width can equal NEC's last write. Any
		// other width than that write is the movie's own. A change the
		// frame listener sees first came without a redraw, so a width
		// still at NEC's write is NEC's and goes back first.
		if (last.item.text != a_shown.text || last.item.count != a_shown.count || last.icons != icons) {
			if (!a_afterRedraw && HoldsWrite(field, last)) {
				WriteWidth(a_movie, a_row, a_index, field, meter, last.base);
			}
			last.item = a_shown;
			last.icons = icons;
			last.written.reset();
		}
		const auto width = Flash::Number(field, "width"sv);
		if (!last.written || std::abs(width - *last.written) >= SAME_WIDTH) {
			last.base = width;
			last.written.reset();
		}

		// With no build to go on the row is left alone, unless the game
		// just redrew it and a meter left showing would sit on its
		// name.
		if (!a_reading) {
			if (!last.written) {
				Hide(meter, last);
				HideNumber(a_row, last);
			}
			return;
		}

		// At Percent the name narrows by the widest number the row can
		// write in place of the meter. A row whose number cannot be made or
		// measured shows nothing.
		const auto  percent = a_reading->percent;
		auto        number = a_asNumber && percent >= 0 ? NumberOf(a_movie, a_row) : Value{};
		const auto  widest = number.IsDisplayObject() ? Widest(number, field, last) : std::nullopt;
		const auto  narrows = percent >= 0 && (!a_asNumber || widest);
		const auto  wanted = narrows ? last.base - METER_GAP - (a_asNumber ? *widest : METER_WIDTH) : last.base;
		if (std::abs(width - wanted) >= SAME_WIDTH) {
			WriteWidth(a_movie, a_row, a_index, field, meter, wanted);
		}
		if (percent < 0) {
			last.written.reset();
			Hide(meter, last);
			HideNumber(a_row, last);
			return;
		}
		last.written = wanted;

		if (a_asNumber) {
			Hide(meter, last);
			last.percent = NOT_DRAWN;
			if (!widest) {
				Roles::Noted(a_movie, "quick container name's font"sv, "that row shows no CND"sv);
				HideNumber(a_row, last);
				return;
			}
			ShowNumber(a_movie, a_row, a_index, number, field, *a_reading, icons, a_afterRedraw);
			return;
		}
		HideNumber(a_row, last);

		const auto selected = Flash::Bool(a_row, "selected"sv);
		const auto fromRight = Settings::bQuickContainerBarFlipped.GetValue();
		const auto changed = last.percent != percent || last.selected != selected || last.fromRight != fromRight;
		if (changed) {
			Draw(meter, percent, selected, fromRight);
			last.percent = percent;
			last.selected = selected;
			last.fromRight = fromRight;
			HudParts::Readout::TellFlipped(fromRight, saidFlipped, "Quick container CND meters"sv);
		}
		if (changed || a_afterRedraw) {
			Recolor(meter, field, a_index);
		}

		// The first icon sits 4 past the name's end, counted in
		// CalcIconWidth, so the icons end at the name's end plus that
		// width. A row with no shown text to measure keeps the meter
		// hidden.
		const auto nameEnd = Roles::Hud::TextEnd(a_movie, a_row);
		if (!nameEnd) {
			Roles::Noted(a_movie, "quick container name's end"sv, "that row shows no CND meter"sv);
			Hide(meter, last);
			return;
		}

		// Whole pixels keep the one pixel frame sharp.
		if (Show(meter, last, std::round(*nameEnd + icons + METER_GAP))) {
			TraceLog::Line("menu", "Quick container row {:d} CND meter at {:.0f}, after icons {:.0f} wide from the name's end at {:.1f}", a_index, last.x, icons, *nameEnd);
		}
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
	// letters where the meter starts, its baseline on the name's as the
	// game draws it after its own shrink.
	void Meters::ShowNumber(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, Value& a_number, Value& a_name, const Reading& a_reading, double a_icons,
		bool a_afterRedraw)
	{
		auto&      last = drawn[a_index];
		const auto selected = Flash::Bool(a_row, "selected"sv);
		const auto number = HudParts::Readout::PercentText(a_reading.tenths);
		const auto text = last.word + " " + number;
		const auto renumbered = number != last.number;
		const auto changed = renumbered || selected != last.numberSelected;
		if (changed) {
			Write(a_number, text, *last.letters, selected);
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
		const auto x = std::round(*nameEnd + a_icons + METER_GAP);
		if (!changed && !a_afterRedraw && x == last.numberX && last.numberShown) {
			return;
		}
		const auto name = Flash::DrawnLine(a_name);
		const auto own = Flash::FirstLine(a_number);
		if (!name || !own) {
			Roles::Noted(a_movie, "quick container name's line"sv, "that row shows no CND"sv);
			HideNumber(a_row, last);
			return;
		}
		const auto scale = Flash::Number(a_number, "scaleX"sv, 1.0);
		const auto baseline = Flash::Number(a_name, "y"sv) + (name->baseline * Flash::Number(a_name, "scaleY"sv, 1.0));
		Flash::Set(a_number, "x"sv, Value(x - (own->left * scale)));
		Flash::Set(a_number, "y"sv, Value(baseline - (own->baseline * scale)));
		Flash::Set(a_number, "visible"sv, Value(true));
		last.numberX = x;
		last.numberShown = true;
		if (renumbered) {
			TraceLog::Line("menu", "Quick container row {:d} CND drawn as {:s} at {:.0f}, after icons {:.0f} wide from the name's end at {:.1f}", a_index, text, x, a_icons, *nameEnd);
		}
	}
}
