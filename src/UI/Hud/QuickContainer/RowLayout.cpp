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
#include <optional>
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
		LayIcons(a_row, a_index);
	}

	// Lays the row's icons out again after its name's new end.
	void Meters::LayIcons(Value& a_row, std::size_t a_index)
	{
		auto kept = Flash::TextTints(a_row);
		a_row.Invoke("AddIconsToEntry");
		if (const auto back = Flash::PutBackTints(kept); back > 0) {
			TraceLog::Line("menu", "Quick container row {:d} text colour put back on {:d} of {:d} shown text fields after its icon layout", a_index, back, kept.size());
		}
	}

	// Narrows the other fields the row's name is drawn in, so their text ends
	// by a_limit, room for the icons and the CND after it, as ItemName_tf's
	// does. With no a_limit each gets its own width back. A field's own width
	// is its width unless that is still NEC's last write.
	void Meters::NarrowOthers(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, std::optional<double> a_limit)
	{
		auto& last = drawn[a_index];
		auto  wrote = false;
		for (auto& field : Roles::QuickRow::NameFields(a_movie, a_row)) {
			const auto name = Flash::String(field, "name"sv);
			auto       other = std::ranges::find(last.others, name, &Other::name);
			if (other == last.others.end()) {
				other = std::ranges::find(last.others, ""s, &Other::name);
				if (other == last.others.end()) {
					Roles::Noted(a_movie, "room for a quick container name in more fields"sv, "that field keeps its width"sv);
					continue;
				}
				other->name = name;
			}
			const auto width = Flash::Number(field, "width"sv);
			if (!other->written || std::abs(width - *other->written) >= SAME_WIDTH) {
				other->base = width;
				other->written.reset();
			}
			const auto wanted = a_limit ? std::clamp(*a_limit - Flash::Number(field, "x"sv), 0.0, other->base) : other->base;
			if (std::abs(width - wanted) >= SAME_WIDTH) {
				Flash::Set(field, "width"sv, Value(wanted));
				wrote = true;
			}
			other->written = a_limit ? std::optional{ wanted } : std::nullopt;
		}
		if (wrote) {
			LayIcons(a_row, a_index);
		}
	}

	// Gives every other field the name was drawn in its own width back, a
	// hidden one too, while it still holds NEC's write. True when one got
	// it back.
	bool Meters::GiveOthersBack(Value& a_row, Drawn& a_drawn)
	{
		auto gave = false;
		for (auto& other : a_drawn.others) {
			auto field = other.name.empty() ? Value{} : Flash::Child(a_row, other.name.c_str());
			if (other.written && field.IsDisplayObject() && std::abs(Flash::Number(field, "width"sv) - *other.written) < SAME_WIDTH) {
				gave = Flash::Set(field, "width"sv, Value(other.base)) || gave;
			}
			other = Other{};
		}
		return gave;
	}

	// Hides the meter of a row NEC drew into or narrowed, and gives the
	// name back the width the meter took, but only while the name still
	// holds NEC's write, so a width the movie set since stays. A row with
	// no data is hidden and sets its width at its next redraw, and
	// AddIconsToEntry fails on it, so it gets no give back.
	void Meters::Withdraw(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index)
	{
		auto&      last = drawn[a_index];
		const auto narrowed = std::ranges::any_of(last.others, [](const Other& a_other) { return a_other.written.has_value(); });
		if (last.percent == NOT_DRAWN && !last.visible && !last.numberShown && !last.written && !narrowed) {
			return;
		}
		HideNumber(a_row, last);
		const auto gave = GiveOthersBack(a_row, last);

		Value field;
		auto  meter = Flash::Child(a_row, METER_NAME);
		if (!a_row.IsDisplayObject() || !a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
			return;
		}
		Hide(meter, last);

		Value data;
		if (a_row.GetMember("data"sv, &data) && data.IsObject()) {
			if (HoldsWrite(field, last)) {
				WriteWidth(a_movie, a_row, a_index, field, meter, last.base);
			} else if (gave) {
				LayIcons(a_row, a_index);
			}
		}
		last = Drawn{};
	}

	// The first half of a row's layout: its icons' width, and its name's own
	// width, taken afresh when the movie set it. Keeps where the room the
	// movie gives the row's text ends: the name field's right edge at the
	// movie's own width plus what the movie took off it for icons, or the
	// left edge of a column a HUD mod adds when that comes first. False for
	// a row with no name field or meter.
	bool Meters::Measure(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, const Shown& a_shown, bool a_afterRedraw)
	{
		auto& last = drawn[a_index];
		last.room.reset();
		Value field;
		auto  meter = Flash::Child(a_row, METER_NAME);
		if (!a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
			return false;
		}

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
			last.probed.reset();
		}
		const auto width = Flash::Number(field, "width"sv);
		if (!last.written || std::abs(width - *last.written) >= SAME_WIDTH) {
			last.base = width;
			last.written.reset();
		}
		const auto left = Flash::Number(field, "x"sv);
		auto       room = left + last.base + icons;
		if (const auto column = Roles::QuickRow::ColumnStart(a_movie, a_row, left)) {
			room = std::min(room, *column);
		}
		if (std::isfinite(room)) {
			last.room = room;
		}
		return true;
	}

	// Lays out the rest of one measured row. a_reading's percent is
	// NO_CONDITION for an item that does not wear, and a_reading is nothing
	// when no kept build matches. a_asNumber is true at Percent. a_edge is
	// the smallest room end of the list's rows, see Place.
	void Meters::Apply(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, std::optional<Reading> a_reading, bool a_asNumber,
		std::optional<double> a_edge, bool a_afterRedraw)
	{
		Value field;
		auto  meter = Flash::Child(a_row, METER_NAME);
		auto& last = drawn[a_index];
		if (!a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject() || !last.room) {
			return;
		}
		const auto icons = last.icons;
		const auto width = Flash::Number(field, "width"sv);

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

		// The CND ends at the row's right edge, or after the name inside
		// the row's own room. At Percent the name narrows by the widest
		// number the row can write in place of the meter, and every other
		// field it is drawn in ends as far in. A row whose number cannot be
		// made or measured shows nothing.
		const auto  percent = a_reading->percent;
		auto        number = a_asNumber && percent >= 0 ? NumberOf(a_movie, a_row) : Value{};
		const auto  widest = number.IsDisplayObject() ? Widest(number, field, last) : std::nullopt;
		const auto  narrows = percent >= 0 && (!a_asNumber || widest);
		// The name narrows by the CND and by what the room's end, or the
		// row's right edge, takes off the movie's own room.
		const auto  end = Settings::bQuickContainerCndAtRight.GetValue() && a_edge ? std::min(*a_edge, *last.room) : *last.room;
		const auto  room = METER_GAP + (a_asNumber ? widest.value_or(0.0) : METER_WIDTH);
		const auto  ownEnd = Flash::Number(field, "x"sv) + last.base + icons;
		const auto  wanted = narrows ? last.base - room - (ownEnd - end) : last.base;
		if (std::abs(width - wanted) >= SAME_WIDTH) {
			WriteWidth(a_movie, a_row, a_index, field, meter, wanted);
		}
		NarrowOthers(a_movie, a_row, a_index, narrows ? std::optional{ end - room - icons } : std::nullopt);
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
			ShowNumber(a_movie, a_row, a_index, number, field, *a_reading, icons, end, a_afterRedraw);
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

		// A row with no shown text to measure keeps the meter hidden.
		const auto nameEnd = Roles::Hud::TextEnd(a_movie, a_row);
		if (!nameEnd) {
			Roles::Noted(a_movie, "quick container name's end"sv, "that row shows no CND meter"sv);
			Hide(meter, last);
			return;
		}

		// Whole pixels keep the one pixel frame sharp.
		const auto spot = Place(a_movie, a_row, field, last, *nameEnd, icons, METER_WIDTH, end, a_afterRedraw);
		if (Show(meter, last, std::round(spot.x))) {
			TraceLog::Line("menu", "Quick container row {:d} CND meter at {:.0f}, after icons {:.0f} wide from the name's end at {:.1f}, {:s}", a_index, last.x, icons, *nameEnd,
				Standing(spot.edge));
		}
	}
}
