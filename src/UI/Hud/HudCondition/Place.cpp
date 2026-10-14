#include "UI/Hud/HudCondition/Place.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <cmath>
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

		// The place kept for the MCM page, and the whole HUD units the trace
		// last said it in.
		std::mutex                         g_lock;
		std::optional<Measure>             g_measured;
		std::optional<std::array<long, 8>> g_told;

		[[nodiscard]] std::array<long, 8> Whole(const Measure& a_measure)
		{
			const auto& box = a_measure.box;
			const auto& frame = a_measure.frame;
			return { std::lround(box.left), std::lround(box.top), std::lround(box.right), std::lround(box.bottom),
				std::lround(frame.left), std::lround(frame.top), std::lround(frame.right), std::lround(frame.bottom) };
		}
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
		for (const auto& corner : corners) {
			const auto staged = Roles::ToStage(a_movie, a_meters, InParent(counter, Point{ a_spot.x + corner.x, a_spot.y + corner.y }));
			if (!staged) {
				return std::nullopt;
			}
			const Flash::Box dot{ staged->x, staged->y, staged->x, staged->y };
			box = box ? box->With(dot) : dot;
		}

		// The stage's y runs down, so the shift's y is taken off.
		const Point want{ shift.x, -shift.y };
		const Point move{ Held(want.x, box->left, box->right, frame.left, frame.right), Held(want.y, box->top, box->bottom, frame.top, frame.bottom) };
		const auto  held = std::abs(move.x - want.x) > LEEWAY || std::abs(move.y - want.y) > LEEWAY;

		const auto spot = InParent(counter, Point{ a_spot.x, a_spot.y });
		Placed     placed{ Transform{ spot.x, spot.y, counter.scaleX, counter.scaleY, counter.rotation }, *box,
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

	void Keep(const Flash::Box& a_box, const Flash::Box& a_frame)
	{
		const Measure now{ a_box, a_frame };
		const auto    whole = Whole(now);
		bool          tell = false;
		{
			const std::scoped_lock lock{ g_lock };
			g_measured = now;
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
		g_measured.reset();
		g_told.reset();
	}

	std::optional<Measure> Measured()
	{
		const std::scoped_lock lock{ g_lock };
		return g_measured;
	}
}
