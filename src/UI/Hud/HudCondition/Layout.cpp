#include "UI/Hud/HudCondition/Listener.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Roles.h"

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

namespace HudCondition
{
	using Scaleform::GFx::Value;

	// -------------------------------------------------------------------
	// Laying the bar out and placing it
	// -------------------------------------------------------------------

	// Places the bar on screen, as drawn at a_size, and keeps that
	// place for the MCM page. While its counter is wholly off screen,
	// which is how HUD mods hide the counter, the bar hides, or with
	// bHudBarStandsAlone on stands alone. It hides while the movie
	// refuses a point, so it is never drawn where NEC cannot tell it
	// is on screen.
	void FrameListener::PlaceOnScreen(Scaleform::GFx::Movie& a_movie, Value& a_meters, Value& a_counter, Value& a_readout, const Basis& a_basis, const Spot& a_drawn,
		const HudParts::Readout::Size& a_size)
	{
		const auto counterIn = Roles::InFrame(a_movie, a_counter);
		const auto standing = !counterIn && Settings::bHudBarStandsAlone.GetValue();
		const auto filled = Filled(a_drawn, Words(), !tabled);
		std::optional<Placed> placed;
		if (counterIn) {
			placed = Place(a_movie, a_meters, a_basis, a_drawn.line, filled);
		} else if (standing) {
			placed = PlaceAlone(a_movie, a_meters, a_basis, filled);
		}
		placeable = placed.has_value();
		onSpot = placed && placed->onSpot;
		standsAlone = placed && placed->alone;
		TellAlone(placed, a_basis);
		if (!placed) {
			Roles::Noted(a_movie, counterIn || standing ? "stage place for the CND bar"sv : "on screen ammo counter"sv,
				counterIn || standing ? "the bar hides"sv : "the CND bar hides with it"sv);
			return;
		}
		if (placed->at != placedAt) {
			Put(a_readout, placed->at);
			placedAt = placed->at;
		}
		Keep(*placed, spot, a_size, Words(), a_basis.frame, tabled);
		if (placed->held != held) {
			held = placed->held;
			if (held) {
				TraceLog::Line("menu", "HUD CND bar held whole on screen, {:.0f},{:.0f} short of where fHudBarX and fHudBarY put it", placed->shortBy.x, placed->shortBy.y);
			} else {
				TraceLog::Line("menu", "HUD CND bar sits where fHudBarX and fHudBarY put it");
			}
		}
	}

	// Says where a bar that stands alone starts, from the screen's top
	// left in HUD units as the MCM page shows it, each time a whole
	// unit changes.
	void FrameListener::TellAlone(const std::optional<Placed>& a_placed, const Basis& a_basis)
	{
		std::optional<std::array<long, 2>> at;
		if (a_placed && a_placed->alone) {
			at = std::array{ std::lround(a_basis.shift.x - a_placed->shortBy.x), std::lround(a_placed->shortBy.y - a_basis.shift.y) };
		}
		if (at == aloneAt) {
			return;
		}
		aloneAt = at;
		if (at) {
			TraceLog::Line("menu", "HUD CND bar stands alone {:d},{:d} from the screen's top left in HUD units, with no ammo counter on screen to follow", (*at)[0], (*at)[1]);
		}
	}

	// The first reading takes the spot chosen or the shipped place.
	// Later only a spot that differs is taken. The frame then lays the
	// bar out there. A spot that differs by clear and turn alone is taken
	// with nothing in NEC.log, since the bar and its word do not move.
	void FrameListener::Follow(const std::optional<Choice>& a_choice)
	{
		const bool same = followed && (!a_choice || a_choice->spot.SitsAs(spot));
		if (a_choice) {
			spot = a_choice->spot;
		}
		if (same) {
			return;
		}
		followed = true;
		const auto said = a_choice ? std::string_view{ a_choice->said } : std::string_view{};
		if (!spot.onDivider) {
			TraceLog::Line("menu", "HUD CND {:s}, at {:.0f},{:.0f}, {:.0f} across and {:.1f} deep", said, spot.line.x, spot.line.y, spot.line.width, spot.deep);
			return;
		}
		TraceLog::Line("menu", "HUD CND bar takes the ammo divider's place at {:.0f},{:.0f}, {:.0f} across and {:.1f} deep",
			spot.line.x, spot.line.y, spot.line.width, spot.deep);
		if (!said.empty()) {
			TraceLog::Line("menu", "HUD CND {:s}, the word moved by {:.0f},{:.0f}", said, spot.wordX, spot.wordY);
		}
	}

	// Draws the track and moves the word for a_drawn, the spot at
	// a_size, then draws the level and places the bar again. At Percent
	// it moves the 1 line or the 2 lines instead, see Table.h.
	void FrameListener::LayOut(Value& a_readout, const Spot& a_drawn, const HudParts::Readout::Size& a_size)
	{
		HudParts::Readout::SetTrack(a_readout, a_drawn.line.width, a_drawn.deep);
		if (tabled && table) {
			PutTable(a_readout, label, *table, a_drawn.wordX, a_drawn.wordY, One());
			laidRise = table->rise;
			lined.reset();
		} else {
			HudParts::Readout::MoveLabel(a_readout, label.field.left + a_drawn.wordX, label.field.top + a_drawn.wordY);
			laidRise = 0.0;
		}
		laidOut = a_drawn;
		shownPercent = NOT_DRAWN;
		shownMark = HudParts::Readout::Mark{};
		placedFrom.reset();
		if (a_size != laidOutSize) {
			laidOutSize = a_size;
			TraceLog::Line("menu", "HUD CND bar sized to {:g} times its length and {:g} times its thickness, {:.0f} across and {:.1f} deep",
				a_size.length, a_size.thickness, a_drawn.line.width, a_drawn.deep);
		}
	}

	// -------------------------------------------------------------------
	// Bar or percent, 1 line or 2
	// -------------------------------------------------------------------

	// A change of form shows the bar's boxes, or hides them and shows the
	// word's field, which holds the percent, and forgets everything laid
	// out, so this frame reads and lays the new form out. A bar takes the
	// word back from the 1 line, and a percent starts in 2 lines until told.
	void FrameListener::SwitchForm(Value& a_readout, bool a_number)
	{
		tabled = a_number;
		HudParts::Readout::ShowBar(a_readout, !a_number);
		if (a_number) {
			HudParts::Readout::ShowLabel(a_readout, true);
		} else {
			HideTable(a_readout);
		}
		if (oneLine) {
			WriteWord(a_readout);
		}
		oneLine = false;
		saidLines = {};
		wordShown.reset();
		laidOut.reset();
		shownPercent = NOT_DRAWN;
		shownMark = HudParts::Readout::Mark{};
		table.reset();
		shownNumber.clear();
		lined.reset();
		lineAlpha = -1.0;
		placedFrom.reset();
		lastReading.reset();
	}

	// Tells the percent's form every frame from the facts now, see Lines in
	// Table.h, the divider's from the last measure. A change of form lays
	// the percent out again and reads the HUD again next frame, so its spot
	// is chosen for the new box. The 1 line going to the ammo line or away
	// from it is laid out again too, since its size may change. The trace
	// says the form and why on every change of either.
	void FrameListener::FollowLines(Value& a_readout)
	{
		const LinesFacts facts{ .moved = ShiftNow() != Shift{},
			.alone = standsAlone,
			.noCounter = counterPart && !HudParts::HasRounds(*counterPart),
			.between = between,
			.fits = fits,
			.asked = Settings::bHudBarPercentOneLine.GetValue(),
			.wordless = !Settings::bHudBarShowsWord.GetValue() };
		const auto lines = LinesFor(facts);
		const auto moves = lines.one != oneLine || (lines.one && lines.atLine != atLine);
		atLine = lines.atLine;
		if (lines.why != saidLines) {
			saidLines = lines.why;
			TraceLog::Line("menu", "HUD CND percent in {:s}, {:s}", lines.one ? "1 line" : "2 lines", lines.why);
		}
		if (!moves) {
			return;
		}
		oneLine = lines.one;
		if (!oneLine) {
			WriteWord(a_readout);
		}
		laidOut.reset();
		shownNumber.clear();
		lined.reset();
		placedFrom.reset();
		lastReading.reset();
	}

	// Writes the percent under the word when it changes, as the weapon's
	// card writes it, or the 1 line in the word's field.
	void FrameListener::DrawNumber(Value& a_readout)
	{
		const auto tenths = HudParts::Weapon::Tenths(&Settings::iWeaponCnd);
		const auto number = tenths && table ? HudParts::Readout::PercentText(*tenths) : std::string{};
		if (number == shownNumber) {
			return;
		}
		shownNumber = number;
		lined.reset();
		if (number.empty()) {
			return;
		}
		if (const auto* one = One()) {
			WriteOneLine(a_readout, *tenths, one->scale);
		} else {
			WriteNumber(a_readout, number);
		}
		const auto word = Flash::String(HudParts::Readout::LabelOf(a_readout), "text"sv);
		TraceLog::Line("menu", "HUD CND drawn as {:s}", oneLine ? word : word + " " + number);
	}
}
