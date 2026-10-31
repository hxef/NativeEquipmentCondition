#include "UI/Hud/HudCondition/Place.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <numbers>

namespace HudCondition
{
	namespace
	{
		using Flash::Point;
		using Scaleform::GFx::Value;

		// A hold under a 100th of a HUD unit counts as none, so a spot
		// Spot.cpp judged on screen keeps the divider hidden through
		// rounding.
		constexpr double LEEWAY = 0.01;

		// A point in the readout's units at its spot, in the parent's units.
		[[nodiscard]] Point InParent(const Transform& a_counter, const Point& a_point)
		{
			const auto turn = a_counter.rotation * std::numbers::pi / 180.0;
			const auto across = a_point.x * a_counter.scaleX;
			const auto down = a_point.y * a_counter.scaleY;
			return Point{ a_counter.x + (across * std::cos(turn)) - (down * std::sin(turn)), a_counter.y + (across * std::sin(turn)) + (down * std::cos(turn)) };
		}

		// What the place kept for the MCM page was worked out from, so the
		// box can be worked out again at another size.
		struct Kept
		{
			Measure                       measure;
			std::optional<Staging>        staging;
			Spot                          spot;
			HudParts::Readout::Size       size;
			HudParts::Readout::LabelPlace label;
		};

		// The place kept for the MCM page, and the whole HUD units the trace
		// last said it in.
		std::mutex                         g_lock;
		std::optional<Kept>                g_kept;
		std::optional<std::array<long, 8>> g_told;

		[[nodiscard]] std::array<long, 8> Whole(const Measure& a_measure)
		{
			const auto& box = a_measure.box;
			const auto& frame = a_measure.frame;
			return { std::lround(box.left), std::lround(box.top), std::lround(box.right), std::lround(box.bottom),
				std::lround(frame.left), std::lround(frame.top), std::lround(frame.right), std::lround(frame.bottom) };
		}

		// The staging of a box from its corners as Place stages them: top
		// left, top right, bottom left, bottom right.
		[[nodiscard]] std::optional<Staging> StagingOf(const Flash::Box& a_box, const std::array<Point, 4>& a_staged)
		{
			const auto wide = a_box.right - a_box.left;
			const auto high = a_box.bottom - a_box.top;
			if (!(wide > 0.0) || !(high > 0.0)) {
				return std::nullopt;
			}
			const Point across{ (a_staged[1].x - a_staged[0].x) / wide, (a_staged[1].y - a_staged[0].y) / wide };
			const Point down{ (a_staged[2].x - a_staged[0].x) / high, (a_staged[2].y - a_staged[0].y) / high };
			const Point origin{ a_staged[0].x - (across.x * a_box.left) - (down.x * a_box.top), a_staged[0].y - (across.y * a_box.left) - (down.y * a_box.top) };
			return Staging{ origin, across, down };
		}
	}

	Flash::Box Staging::Holding(const Flash::Box& a_box) const
	{
		std::optional<Flash::Box> held;
		for (const auto& corner : { Point{ a_box.left, a_box.top }, Point{ a_box.right, a_box.top }, Point{ a_box.left, a_box.bottom }, Point{ a_box.right, a_box.bottom } }) {
			const auto x = origin.x + (across.x * corner.x) + (down.x * corner.y);
			const auto y = origin.y + (across.y * corner.x) + (down.y * corner.y);
			const Flash::Box dot{ x, y, x, y };
			held = held ? held->With(dot) : dot;
		}
		return *held;
	}

	// -------------------------------------------------------------------
	// Placing the readout
	// -------------------------------------------------------------------

	double Held(double a_want, double a_from, double a_to, double a_low, double a_high)
	{
		return std::max(a_low - a_from, std::min(a_want, a_high - a_to));
	}

	double Finite(double a_value)
	{
		return std::isfinite(a_value) ? a_value : 0.0;
	}

	// A nan typed into the ini would otherwise place the bar again every
	// frame, since it never equals itself.
	Shift ShiftNow()
	{
		return Shift{ Finite(Settings::fHudBarX.GetValue()), Finite(Settings::fHudBarY.GetValue()) };
	}

	HudParts::Readout::Size SizeNow()
	{
		return HudParts::Readout::SizeFrom(Settings::fHudBarLength, Settings::fHudBarThickness);
	}

	Transform TransformOf(const Value& a_clip)
	{
		return Transform{ Flash::Number(a_clip, "x"sv), Flash::Number(a_clip, "y"sv),
			Flash::Number(a_clip, "scaleX"sv), Flash::Number(a_clip, "scaleY"sv), Flash::Number(a_clip, "rotation"sv) };
	}

	Parents ParentsOf(const Value& a_meters)
	{
		Parents parents{};
		Value   clip = a_meters;
		for (auto& parent : parents) {
			parent = TransformOf(clip);
			Value above;
			if (!clip.GetMember("parent"sv, &above) || !above.IsDisplayObject()) {
				break;
			}
			clip = above;
		}
		return parents;
	}

	std::optional<Placed> Place(Scaleform::GFx::Movie& a_movie, Value& a_meters, const Basis& a_basis, const Roles::Hud::Line& a_spot, const Flash::Box& a_filled)
	{
		const auto& counter = a_basis.counter;
		const auto& shift = a_basis.shift;
		const auto& frame = a_basis.frame;

		// The box's 4 corners go to the stage, and the box holding them is
		// held, so a turned counter is held too.
		std::optional<Flash::Box> box;
		const std::array corners{ Point{ a_filled.left, a_filled.top }, Point{ a_filled.right, a_filled.top }, Point{ a_filled.left, a_filled.bottom },
			Point{ a_filled.right, a_filled.bottom } };
		std::array<Point, 4> staged{};
		for (std::size_t i = 0; i < corners.size(); i++) {
			const auto at = Roles::ToStage(a_movie, a_meters, InParent(counter, Point{ a_spot.x + corners[i].x, a_spot.y + corners[i].y }));
			if (!at) {
				return std::nullopt;
			}
			staged[i] = *at;
			const Flash::Box dot{ at->x, at->y, at->x, at->y };
			box = box ? box->With(dot) : dot;
		}

		// The stage's y runs down, so the shift's y is taken off.
		const Point want{ shift.x, -shift.y };
		const Point move{ Held(want.x, box->left, box->right, frame.left, frame.right), Held(want.y, box->top, box->bottom, frame.top, frame.bottom) };
		const auto  held = std::abs(move.x - want.x) > LEEWAY || std::abs(move.y - want.y) > LEEWAY;

		const auto spot = InParent(counter, Point{ a_spot.x, a_spot.y });
		Placed     placed{ Transform{ spot.x, spot.y, counter.scaleX, counter.scaleY, counter.rotation }, *box, StagingOf(a_filled, staged),
			Shift{ want.x - move.x, move.y - want.y }, shift == Shift{} && !held, held };

		// With no move the readout sits on the spot itself, with no trip
		// through the stage to round it.
		if (move.x == 0.0 && move.y == 0.0) {
			return placed;
		}
		const auto from = Roles::ToStage(a_movie, a_meters, spot);
		const auto to = from ? Roles::FromStage(a_movie, a_meters, Point{ from->x + move.x, from->y + move.y }) : std::nullopt;
		if (!to) {
			return std::nullopt;
		}
		placed.at.x = to->x;
		placed.at.y = to->y;
		return placed;
	}

	void Put(Value& a_readout, const Transform& a_at)
	{
		a_readout.SetMember("x"sv, Value(a_at.x));
		a_readout.SetMember("y"sv, Value(a_at.y));
		a_readout.SetMember("scaleX"sv, Value(a_at.scaleX));
		a_readout.SetMember("scaleY"sv, Value(a_at.scaleY));
		a_readout.SetMember("rotation"sv, Value(a_at.rotation));
	}

	// -------------------------------------------------------------------
	// The MCM page's numbers
	// -------------------------------------------------------------------

	void Keep(const Placed& a_placed, const Spot& a_spot, const HudParts::Readout::Size& a_size, const HudParts::Readout::LabelPlace& a_label,
		const Flash::Box& a_frame)
	{
		const Measure now{ a_placed.box, a_frame };
		const auto    whole = Whole(now);
		bool          tell = false;
		{
			const std::scoped_lock lock{ g_lock };
			g_kept = Kept{ now, a_placed.staging, a_spot, a_size, a_label };
			tell = whole != g_told;
			g_told = whole;
		}
		if (tell) {
			TraceLog::Line("menu", "HUD CND bar and word fill {:.1f},{:.1f} to {:.1f},{:.1f} on their spot, the screen showing {:.1f},{:.1f} to {:.1f},{:.1f} of the HUD",
				now.box.left, now.box.top, now.box.right, now.box.bottom, now.frame.left, now.frame.top, now.frame.right, now.frame.bottom);
		}
	}

	void Forget()
	{
		const std::scoped_lock lock{ g_lock };
		g_kept.reset();
		g_told.reset();
	}

	std::optional<Measure> Measured()
	{
		std::optional<Kept> kept;
		{
			const std::scoped_lock lock{ g_lock };
			kept = g_kept;
		}
		if (!kept) {
			return std::nullopt;
		}
		const auto size = SizeNow();
		if (size == kept->size || !kept->staging) {
			return kept->measure;
		}
		const auto& label = kept->label;
		return Measure{ kept->staging->Holding(Filled(Sized(kept->spot, size, label), label)), kept->measure.frame };
	}
}
