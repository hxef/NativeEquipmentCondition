#pragma once

#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

// The boxes Spot.cpp's search for the CND bar's spot works with: what the HUD
// shows around the ammo counter, the bar's box, what a box covers and the
// origins to try beside a box. Private to this folder.
namespace HudCondition
{
	// What the HUD shows around the counter now, in the counter's units.
	// inside holds the counter's other children but a backdrop, beside the
	// parts next to the counter. word is where the word's letters sit from
	// the bar's right end and line the same down to the bottom of their
	// line, deep how deep the bar is, and frame the screen, or nothing
	// while it cannot be told. flipped is on while bCndBarsFlipped is, so
	// the word's own place is before the bar's start, see Spot.
	struct Around
	{
		Roles::Hud::DividerShape      divider;
		std::vector<Roles::Hud::Part> numbers;
		std::vector<Roles::Hud::Part> inside;
		std::vector<Roles::Hud::Part> beside;
		Roles::Box                    word;
		Roles::Box                    line;
		double                        deep = 0.0;
		std::optional<Roles::Box>     frame;
		bool                          flipped = false;
	};

	// Why the bar does not fit on its divider, in the words NEC.log uses,
	// and the same without its numbers.
	struct Why
	{
		std::string said;
		std::string kind;
	};

	// An origin to try for a box, and the side of what it stands beside,
	// in the words NEC.log uses.
	struct Try
	{
		double      x = 0.0;
		double      y = 0.0;
		const char* side = "";
	};

	// The box of a bar a_length long and a_deep deep, with its right end
	// and middle at a_x, a_y.
	[[nodiscard]] Roles::Box BarAt(double a_x, double a_y, double a_length, double a_deep);

	// The middle of a box across and down.
	[[nodiscard]] double MiddleAcross(const Roles::Box& a_box);
	[[nodiscard]] double MiddleDown(const Roles::Box& a_box);

	// Whether a box is on screen. While the screen cannot be told every
	// box is.
	[[nodiscard]] bool OnScreen(const Around& a_around, const Roles::Box& a_box);

	// Whether the word's whole line, the box Filled holds, is on screen
	// with the bar's right end at a_x, a_y.
	[[nodiscard]] bool WordOnScreen(const Around& a_around, double a_x, double a_y);

	// The word's letters down to the bottom of their line.
	[[nodiscard]] Roles::Box WholeLine(const HudParts::Readout::LabelPlace& a_label);

	// The first shown part a box would cover, or null.
	[[nodiscard]] const Roles::Hud::Part* Covered(const Roles::Box& a_box, std::span<const Roles::Hud::Part> a_parts);

	// Whether the divider runs between the numbers, with the middle of
	// one above its own and the middle of another below. Numbers shown
	// or not count, so one that drops out for a moment moves nothing.
	[[nodiscard]] bool Between(const Roles::Box& a_divider, std::span<const Roles::Hud::Part> a_numbers);

	// Origins that put a_box, drawn from its own origin, beside a_anchor
	// and a_gap away from it, nearest a_aim first. On each side the box
	// lines up with a_aim, with the anchor's ends and middle, and with
	// each part's edges a_gap away, and part of it stays level with the
	// anchor.
	[[nodiscard]] std::vector<Try> Tries(const Roles::Box& a_anchor, const Roles::Box& a_box, double a_gap, const Flash::Point& a_aim,
		std::span<const Roles::Hud::Part> a_parts);
}
