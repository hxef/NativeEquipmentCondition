#include "UI/Hud/HudCondition/Listener.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Roles/Roles.h"

#include <array>
#include <cmath>
#include <optional>
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
		const auto filled = Filled(a_drawn, label);
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
		Keep(*placed, spot, a_size, label, a_basis.frame);
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
	// bar out there.
	void FrameListener::Follow(const std::optional<Choice>& a_choice)
	{
		if (followed && (!a_choice || a_choice->spot == spot)) {
			return;
		}
		spot = a_choice ? a_choice->spot : spot;
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
	// a_size, then draws the level and places the bar again.
	void FrameListener::LayOut(Value& a_readout, const Spot& a_drawn, const HudParts::Readout::Size& a_size)
	{
		HudParts::Readout::SetTrack(a_readout, a_drawn.line.width, a_drawn.deep);
		HudParts::Readout::MoveLabel(a_readout, label.field.left + a_drawn.wordX, label.field.top + a_drawn.wordY);
		laidOut = a_drawn;
		shownPercent = NOT_DRAWN;
		placedFrom.reset();
		if (a_size != laidOutSize) {
			laidOutSize = a_size;
			TraceLog::Line("menu", "HUD CND bar sized to {:g} times its length and {:g} times its thickness, {:.0f} across and {:.1f} deep",
				a_size.length, a_size.thickness, a_drawn.line.width, a_drawn.deep);
		}
	}
}
