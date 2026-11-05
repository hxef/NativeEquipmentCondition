#include "UI/Hud/QuickContainer/RowLayout.h"

#include "Core/TraceLog.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Roles/Hud.h"

#include <cmath>

namespace QuickContainer
{
	using Scaleform::GFx::Value;

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

	// Hides the meter of a row NEC drew into, and gives the name back
	// the width the meter took, but only while the name still holds
	// NEC's write, so a width the movie set since stays. A row with no
	// data is hidden and sets its width at its next redraw, and
	// AddIconsToEntry fails on it, so it gets no give back.
	void Meters::Withdraw(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index)
	{
		auto& last = drawn[a_index];
		if (last.percent == NOT_DRAWN && !last.visible) {
			return;
		}

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

	// Lays out one row. a_percent is NO_CONDITION for an item that does
	// not wear, or nothing when no kept build matches.
	void Meters::Apply(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, const Shown& a_shown, std::optional<std::int32_t> a_percent, bool a_afterRedraw)
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
		if (!a_percent) {
			if (!last.written) {
				Hide(meter, last);
			}
			return;
		}

		const auto wanted = *a_percent >= 0 ? last.base - METER_GAP - METER_WIDTH : last.base;
		if (std::abs(width - wanted) >= SAME_WIDTH) {
			WriteWidth(a_movie, a_row, a_index, field, meter, wanted);
		}
		if (*a_percent < 0) {
			last.written.reset();
			Hide(meter, last);
			return;
		}
		last.written = wanted;

		const auto selected = Flash::Bool(a_row, "selected"sv);
		const auto changed = last.percent != *a_percent || last.selected != selected;
		if (changed) {
			Draw(meter, *a_percent, selected);
			last.percent = *a_percent;
			last.selected = selected;
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
}
