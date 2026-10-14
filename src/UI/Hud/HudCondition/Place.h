#pragma once

#include "UI/Hud/HudCondition/HudCondition.h"
#include "UI/Roles/Hud.h"

#include <array>
#include <optional>

// Where the readout sits in the counter's parent: on its spot, with the
// counter's place, scale and turn, moved by the player's shift and held whole
// on screen. And that place kept for the MCM page. Private to this folder.
namespace HudCondition
{
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

	// Where the readout goes in the counter's parent, and the box it fills
	// on its spot, before the shift, in HUD units. shortBy is how far the
	// screen's edge held it short of the shift, in the shift's units. onSpot
	// is true while the shift is 0 and 0 and nothing held it, held while the
	// edge held it.
	struct Placed
	{
		Transform  at;
		Flash::Box box;
		Shift      shortBy;
		bool       onSpot = false;
		bool       held = false;
	};

	// Where the readout's spot goes in a_meters, the counter's parent, with
	// the counter's place, scale and turn, so the spot turns with it, moved
	// by the shift along the screen and held so that a_filled, the box from
	// Filled, stays whole in the frame. Put writes it. Nothing when the movie
	// refuses a point.
	[[nodiscard]] std::optional<Placed> Place(Scaleform::GFx::Movie& a_movie, Roles::Value& a_meters, const Basis& a_basis,
		const Roles::Hud::Line& a_spot, const Flash::Box& a_filled);

	void Put(Roles::Value& a_readout, const Transform& a_at);

	// Keeps a_box, a Placed box, for the MCM page, see Measured. a_frame is
	// the visible frame it was placed in. The trace says it when a whole HUD
	// unit changes.
	void Keep(const Flash::Box& a_box, const Flash::Box& a_frame);

	// Forgets the place kept, for a new HUD movie.
	void Forget();
}
