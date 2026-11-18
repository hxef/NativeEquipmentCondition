#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/FridgeIcon.h"
#include "UI/Hud/HudParts/Readout.h"
#include "UI/Inventory/ItemCard/WearRateForms.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>

// At Item card CND Bar the CND row keeps its label and its wear rate, and a
// framed meter stands where the number stood, in the quick container
// meters' proportions: a thin frame in the letters' colour, a dark inside,
// a gap and the fill in the same colour, the penalty mark a dark gap
// through the fill or a faint line past it. Its height is a share of the
// number's line, which Item card CND bar thickness scales, and every size
// comes from the row as the open movie draws it, so a UI replacer's card
// gets a meter of its own size. It is not the HUD's bar: only the penalty
// point, the mark's width and the flip rule are shared.
namespace ItemCard
{
	namespace
	{
		using HudParts::Readout::Mark;
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// Measuring the row
		// -------------------------------------------------------------------

		// The widest rate the bar keeps room for while the rate shares the
		// number's field, so every item's bar on 1 card has 1 length.
		constexpr const char* WIDEST_RATE = "(x99.9)";

		// A bar shorter than this many font sizes leaves the number.
		constexpr double LEAST_LENGTH_PER_SIZE = 2.0;

		// The fewest stage pixels a meter takes, so its frame, gap and fill
		// always stay apart.
		constexpr double LEAST_PIXELS = 6.0;

		// Where the bar goes, in the row's units.
		struct Room
		{
			double left = 0.0;
			double right = 0.0;
			double middle = 0.0;  // the number's line's middle, which the bar is centred on
			double height = 0.0;
			double pixel = 0.0;  // 1 pixel of the stage, in the row's units
			double size = 0.0;   // the number's font size
			bool   besideRate = false;
		};

		// The room for a bar on a_row, from a_field, its number's field, and
		// a_line, the number's first line, while the number still shows. The
		// bar starts past the label's letters, or inside the number's field
		// where the row draws something between them, such as the frame of a
		// field that is a cell of its own. It ends at the field's right edge,
		// less the widest rate and a space while the rate shares the field.
		// It is as tall as the number's line, the reach of the rate's
		// brackets, and no taller than the field. Nothing when the row cannot
		// be measured.
		std::optional<Room> Measure(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_field, const std::string& a_text,
			const FridgeIcon::Line& a_line)
		{
			auto       label = Roles::Card::LabelField(a_row);
			const auto labelLine = label.IsDisplayObject() ? FridgeIcon::LineOf(label) : std::nullopt;
			const auto font = FontSize(a_field);
			const auto own = Roles::BoundsIn(a_row, a_row);
			auto       stage = Flash::StageBounds(a_row);
			if (!labelLine || !font || !own || !stage) {
				return std::nullopt;
			}

			const auto nan = std::numeric_limits<double>::quiet_NaN();
			const auto x = Flash::Number(a_field, "x"sv, nan);
			const auto width = Flash::Number(a_field, "width"sv, nan);
			const auto height = Flash::Number(a_field, "height"sv, nan);
			const auto across = Flash::Number(a_field, "scaleX"sv, 1.0);
			const auto down = Flash::Number(a_field, "scaleY"sv, 1.0);
			const auto margin = Flash::FIELD_MARGIN;

			Room room;
			room.size = *font * down;
			room.right = x + width - margin * across;
			const auto at = a_text.find(WEAR_RATE_START);
			room.besideRate = at != std::string::npos;
			if (room.besideRate) {
				// A rate wider than the widest kept for still fits. A space is
				// the step it adds between 2 letters.
				const auto widest = NaturalWidth(a_movie, a_field, WIDEST_RATE);
				const auto shown = NaturalWidth(a_movie, a_field, a_text.substr(at + 1));
				const auto spaced = NaturalWidth(a_movie, a_field, "0 0");
				const auto packed = NaturalWidth(a_movie, a_field, "00");
				if (!widest || !shown || !spaced || !packed) {
					return std::nullopt;
				}
				room.right -= (std::max(*widest, *shown) + *spaced - *packed) * across;
			}

			const auto inside = x + margin * across;
			room.left = labelLine->right + LABEL_GAP;
			if (room.left < inside && Roles::Card::DrawsAcross(a_row, room.left, inside)) {
				room.left = inside;
			}

			room.middle = a_line.middle;
			room.height = std::min(a_line.height, height - 2.0 * margin * down);

			// 1 pixel of the stage, the least any part of the meter is.
			const auto pixels = Flash::Number(*stage, "width"sv, nan) / (own->right - own->left);
			room.pixel = 1.0 / pixels;

			const std::array numbers{ room.left, room.right, room.middle, room.height, room.pixel, room.size };
			if (!std::ranges::all_of(numbers, [](double a_number) { return std::isfinite(a_number); }) || !(pixels > 0.0) ||
				!(room.height >= LEAST_PIXELS * room.pixel)) {
				return std::nullopt;
			}
			return room;
		}

		// -------------------------------------------------------------------
		// Drawing the meter
		// -------------------------------------------------------------------

		// The meter's lit shapes are white, for its tint to colour.
		constexpr std::uint32_t WHITE = 0xFFFFFF;

		// The dark inside stays black under the tint, as the notches of
		// HudParts' AddNotches do.
		constexpr std::uint32_t BLACK = 0x000000;

		// The meter's height at thickness 1, as a share of the number's line.
		constexpr double METER_SHARE = 0.65;

		// fItemCardBarThickness is held to these, as the MCM slider allows.
		constexpr double LEAST_THICKNESS = 0.7;
		constexpr double MOST_THICKNESS = 1.5;

		// The meter is 14 parts high: frame 2, gap 1, fill 8, gap 1, frame 2.
		constexpr double METER_PARTS = 14.0;
		constexpr double FRAME_PARTS = 2.0;
		constexpr double GAP_PARTS = 1.0;

		// The card shows through the dark inside at 35%.
		constexpr double DARK_ALPHA = 0.65;

		// The penalty mark's line where the fill stops short of it.
		constexpr double FAINT_ALPHA = 0.6;

		// The bar a_row holds, or a new one, or a value that is not a display
		// object when the movie will not make one.
		Value BarOf(Scaleform::GFx::Movie& a_movie, Value& a_row)
		{
			if (auto bar = Flash::Child(a_row, CONDITION_BAR_NAME); bar.IsDisplayObject()) {
				return bar;
			}
			Value bar;
			a_movie.CreateObject(&bar, "flash.display.Sprite");
			if (!bar.IsDisplayObject() || !Flash::Set(bar, "name"sv, Value(CONDITION_BAR_NAME)) || !a_row.Invoke("addChild", std::array{ bar })) {
				return {};
			}
			return Flash::Child(a_row, CONDITION_BAR_NAME);
		}

		// Where the meter stands across its row's line, and its frame and gap.
		struct Meter
		{
			double top = 0.0;
			double bottom = 0.0;
			double frame = 0.0;
			double gap = 0.0;
			double part = 0.0;  // a 14th of its height. The penalty mark is 2 parts wide at Penalty mark width 1.
		};

		// The meter for a_room at the thickness the slider asks for now.
		Meter Shape(const Room& a_room)
		{
			const auto thickness = HudParts::Readout::Held(Settings::fItemCardBarThickness, LEAST_THICKNESS, MOST_THICKNESS);
			const auto high = std::min(std::max(METER_SHARE * a_room.height * thickness, LEAST_PIXELS * a_room.pixel), a_room.height);
			const auto part = high / METER_PARTS;
			const auto top = a_room.middle - high / 2.0;
			return Meter{ top, top + high, std::max(FRAME_PARTS * part, a_room.pixel), std::max(GAP_PARTS * part, a_room.pixel), part };
		}

		// Draws a_meter in a_room in 4 fills. Shapes that overlap in 1 fill
		// cut holes in each other. First the frame, the outer box with the
		// inside cut out of it, then the dark inside. Then the fill from the
		// left end, or the right while a_fromRight, with a hole at the mark
		// once the fill reaches it, so the dark inside shows through. Last the
		// faint line at the mark while the fill stops short of it. Near the
		// point the fill runs at least 1 hole's width past the hole, or stops
		// at least 1 faint line's width before the line, so the mark reads on
		// either side.
		bool Draw(Value& a_bar, const Room& a_room, const Meter& a_meter, std::uint32_t a_tenths, const Mark& a_mark, bool a_fromRight)
		{
			Value      graphics;
			const auto rect = [&](double a_left, double a_top, double a_right, double a_bottom) {
				return graphics.Invoke("drawRect", std::array{ Value(a_left), Value(a_top), Value(a_right - a_left), Value(a_bottom - a_top) });
			};
			const auto begin = [&](std::uint32_t a_colour, double a_alpha) { return graphics.Invoke("beginFill", std::array{ Value(a_colour), Value(a_alpha) }); };

			const auto innerLeft = a_room.left + a_meter.frame;
			const auto innerRight = a_room.right - a_meter.frame;
			const auto innerTop = a_meter.top + a_meter.frame;
			const auto innerBottom = a_meter.bottom - a_meter.frame;
			const auto fillLeft = innerLeft + a_meter.gap;
			const auto fillRight = innerRight - a_meter.gap;
			const auto fillTop = innerTop + a_meter.gap;
			const auto fillBottom = innerBottom - a_meter.gap;
			const auto length = fillRight - fillLeft;

			// Lengths along the fill's room from the end the fill starts at. A
			// gun shown at the point counts as at it. The hole is the mark's
			// width and the faint line half of it, 1 stage pixel at least.
			auto filled = length * static_cast<double>(std::min(a_tenths, Condition::FULL_TENTHS)) / static_cast<double>(Condition::FULL_TENTHS);
			const auto reached = a_mark.at != 0 && a_tenths >= static_cast<std::uint32_t>(a_mark.at) * Condition::TENTHS_PER_PERCENT;
			const auto wide = reached ? std::max(a_mark.wide, a_room.pixel) : std::max(a_mark.wide / 2.0, a_room.pixel);
			const auto point = length * static_cast<double>(a_mark.at) / 100.0;
			const auto nearEdge = std::max(point - wide / 2.0, 0.0);
			const auto farEdge = std::min(point + wide / 2.0, length);
			if (a_mark.at != 0) {
				filled = reached ? std::max(filled, std::min(farEdge + wide, length)) : std::min(filled, std::max(nearEdge - wide, 0.0));
			}
			const auto stretch = [&](double a_from, double a_to) {
				return a_fromRight ? std::pair{ fillRight - a_to, fillRight - a_from } : std::pair{ fillLeft + a_from, fillLeft + a_to };
			};

			if (!a_bar.GetMember("graphics"sv, &graphics) || !graphics.IsObject() || !graphics.Invoke("clear")) {
				return false;
			}
			if (!begin(WHITE, 1.0) || !rect(a_room.left, a_meter.top, a_room.right, a_meter.bottom) || !rect(innerLeft, innerTop, innerRight, innerBottom) ||
				!graphics.Invoke("endFill")) {
				return false;
			}
			if (!begin(BLACK, DARK_ALPHA) || !rect(innerLeft, innerTop, innerRight, innerBottom) || !graphics.Invoke("endFill") || !begin(WHITE, 1.0)) {
				return false;
			}
			if (filled > 0.0) {
				const auto [left, right] = stretch(0.0, filled);
				if (!rect(left, fillTop, right, fillBottom)) {
					return false;
				}
			}
			if (reached && farEdge > nearEdge) {
				const auto [left, right] = stretch(nearEdge, farEdge);
				if (!rect(left, fillTop, right, fillBottom)) {
					return false;
				}
			}
			if (!graphics.Invoke("endFill")) {
				return false;
			}
			if (a_mark.at == 0 || reached || !(farEdge > nearEdge)) {
				return true;
			}
			const auto [left, right] = stretch(nearEdge, farEdge);
			return begin(WHITE, FAINT_ALPHA) && rect(left, fillTop, right, fillBottom) && graphics.Invoke("endFill");
		}
	}

	bool DrawConditionBar(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Value& a_row, const BarReading& a_reading)
	{
		const auto movie = MovieName(a_movieFile);
		auto       field = Roles::Card::NumberField(a_row);
		const auto shown = Flash::Member(field, "text"sv);
		const auto line = field.IsDisplayObject() && shown.IsString() ? FridgeIcon::LineOf(field) : std::nullopt;
		const auto text = line ? std::string{ shown.GetString() } : std::string{};
		auto       room = line ? Measure(a_movie, a_row, field, text, *line) : std::nullopt;
		if (room) {
			room->left = MarkFridgeAtBar(a_movie, a_movieFile, a_row, a_reading.fridge, room->left, *line, field);
		}
		const auto length = room ? room->right - room->left : std::numeric_limits<double>::quiet_NaN();
		if (!room || !(length >= LEAST_LENGTH_PER_SIZE * room->size)) {
			TraceLog::First("menu", "{:s} CND row has no room for a bar, {:.1f} across, and shows its number", movie, length);
			return false;
		}

		// The tint is read while the number shows, so the bar takes its
		// letters' colour. The number goes with its space, and the rate
		// stays where it was, or the field empties where the rate has a
		// cell of its own.
		const auto tint = Flash::LetterTint(field);
		const auto still = HudParts::Readout::StillEnd(HudParts::Readout::Still::kLeftEnd, Settings::bItemCardBarFlipped.GetValue());
		const auto fromRight = still == HudParts::Readout::Still::kRightEnd;
		const auto meter = Shape(*room);
		const auto mark = a_reading.kind ? HudParts::Readout::PenaltyMark(*a_reading.kind, meter.bottom - meter.top, 2.0 * meter.part) : Mark{};
		const auto rest = room->besideRate ? text.substr(text.find(WEAR_RATE_START) + 1) : std::string{};
		auto       bar = BarOf(a_movie, a_row);
		if (!bar.IsDisplayObject() || !Draw(bar, *room, meter, a_reading.tenths, mark, fromRight) || !WriteKeepingLook(field, rest)) {
			if (bar.IsDisplayObject()) {
				Flash::Set(bar, "visible"sv, Value(false));
			}
			Roles::Noted(a_movie, "CND row's bar"sv, "CND shows its number"sv);
			return false;
		}
		Flash::SetTint(bar, tint);
		Flash::Set(bar, "visible"sv, Value(true));

		const auto percent = a_reading.tenths / Condition::TENTHS_PER_PERCENT;
		TraceLog::First("menu", "{:s} CND row drawn as a bar {:.1f} across at {:d}%{:s}, {:.1f} high with a frame of {:.1f}, filled from the {:s}{:s}", movie,
			length, percent, room->besideRate ? " beside the rate"sv : ""sv, meter.bottom - meter.top, meter.frame, fromRight ? "right"sv : "left"sv,
			mark.at != 0 ? std::format(", marked at {:d}%", mark.at) : std::string{});
		return true;
	}
}
