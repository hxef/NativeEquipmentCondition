#pragma once

#include "UI/Hud/HudCondition/HudCondition.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Roles/Hud.h"

#include <array>
#include <optional>

// Where the readout sits in the counter's parent: on its spot, with the
// counter's place, scale and turn, moved by the player's shift and held whole
// on screen. And that place kept for the MCM page. Private to this folder.
namespace HudCondition
{
	// The size fHudBarLength and fHudBarThickness ask for now.
	[[nodiscard]] HudParts::Readout::Size SizeNow();

	// Where a clip sits, how big it is drawn and how far it is turned.
	struct Transform
	{
		double x = 0.0;
		double y = 0.0;
		double scaleX = 1.0;
		double scaleY = 1.0;
		double rotation = 0.0;

		bool operator==(const Transform&) const = default;
	};

	[[nodiscard]] Transform TransformOf(const Roles::Value& a_clip);

	// The counter's parent and up to 3 clips above it, a default Transform
	// past the top. A HUD mod can move any of them, which moves the counter
	// on screen while its own place stays the same.
	using Parents = std::array<Transform, 4>;

	[[nodiscard]] Parents ParentsOf(const Roles::Value& a_meters);

	// What a place is worked out from beside the spot: the counter, the
	// clips above it, the shift and the visible frame. The frame listener
	// places the bar again when one changes, and on every reading, which may
	// change the spot.
	struct Basis
	{
		Transform  counter;
		Parents    parents;
		Shift      shift;
		Flash::Box frame;

		bool operator==(const Basis&) const = default;
	};

	// How the readout's units land on the stage at its spot, worked out from
	// 3 corners of a box staged there. The counter and the clips above it
	// only move, scale and turn it, so every other point lands the same way.
	struct Staging
	{
		Flash::Point origin;  // where the readout's origin lands
		Flash::Point across;  // how far 1 unit to the right moves on the stage
		Flash::Point down;    // how far 1 unit down moves on the stage

		// The box on the stage that holds a_box's 4 corners.
		[[nodiscard]] Flash::Box Holding(const Flash::Box& a_box) const;
	};

	// Where the readout goes in the counter's parent, and the box it fills
	// on its spot, before the shift, in HUD units, with the staging that box
	// came from, nothing for a box with no width or height. shortBy is how
	// far the screen's edge held it short of the shift, in the shift's units.
	// onSpot is true while the shift is 0 and 0 and nothing held it, held
	// while the edge held it, and alone while it stands alone, see
	// PlaceAlone.
	struct Placed
	{
		Transform              at;
		Flash::Box             box;
		std::optional<Staging> staging;
		Shift                  shortBy;
		bool                   onSpot = false;
		bool                   held = false;
		bool                   alone = false;
	};

	// Where the readout's spot goes in a_meters, the counter's parent, with
	// the counter's place, scale and turn, so the spot turns with it, moved
	// by the shift along the screen and held so that a_filled, the box from
	// Filled, stays whole in the frame. Put writes it. Nothing when the movie
	// refuses a point.
	[[nodiscard]] std::optional<Placed> Place(Scaleform::GFx::Movie& a_movie, Roles::Value& a_meters, const Basis& a_basis,
		const Roles::Hud::Line& a_spot, const Flash::Box& a_filled);

	// Where the readout goes in a_meters with no counter to follow: upright
	// at its own size, on a spot that puts a_filled's top left corner on the
	// visible frame's, then moved by the shift, so fHudBarY below 0 moves it
	// down, and held whole in the frame. A new length or thickness keeps
	// that corner. Nothing when the movie refuses a point.
	[[nodiscard]] std::optional<Placed> PlaceAlone(Scaleform::GFx::Movie& a_movie, Roles::Value& a_meters, const Basis& a_basis, const Flash::Box& a_filled);

	void Put(Roles::Value& a_readout, const Transform& a_at);

	// Keeps a_placed's box for the MCM page, see Measured, with what it was
	// worked out from: a_spot as chosen, a_size, a_label and a_frame, the
	// visible frame. The trace says the box when a whole HUD unit changes.
	void Keep(const Placed& a_placed, const Spot& a_spot, const HudParts::Readout::Size& a_size, const HudParts::Readout::LabelPlace& a_label,
		const Flash::Box& a_frame);

	// Forgets the place kept, for a new HUD movie.
	void Forget();
}
