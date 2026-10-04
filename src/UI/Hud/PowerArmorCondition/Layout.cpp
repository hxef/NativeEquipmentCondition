#include "UI/Hud/PowerArmorCondition/Layout.h"

#include "UI/Hud/HudParts/HudParts.h"

#include <cmath>

namespace PowerArmorCondition
{
	namespace
	{
		using Scaleform::GFx::Value;
	}

	Frame FrameOf(Scaleform::GFx::Movie& a_movie)
	{
		const auto rect = a_movie.GetVisibleFrameRect();

		Frame frame;
		frame.left = rect.x1;
		frame.top = rect.y1;
		frame.width = rect.x2 - rect.x1;
		frame.height = rect.y2 - rect.y1;

		// The same 2 numbers the game works the dash's distance out from, so
		// the bar follows the dash onto an ultrawide screen.
		const auto* state = RE::BSGraphics::State::GetSingleton();
		if (state && state->backBufferHeight > 0) {
			frame.aspect = static_cast<double>(state->backBufferWidth) / state->backBufferHeight;
		}
		return frame;
	}

	Box Place(const Frame& a_frame, const Anchor& a_anchor)
	{
		// Every measure is taken between places already put into the movie, so
		// whatever the frame does to a place it does to the size and the turn.
		const auto start = a_frame.Stage(a_anchor.start);
		const auto end = a_frame.Stage(a_anchor.end);
		const auto word = a_frame.Stage(a_anchor.word);
		const auto limit = a_frame.Stage(a_anchor.limit);
		const auto first = a_frame.Stage(a_anchor.first);
		const auto above = a_frame.Stage(a_anchor.above);

		Box box;
		box.x = start.x;
		box.y = start.y;
		box.width = std::hypot(end.x - start.x, end.y - start.y);
		box.gap = std::hypot(word.x - end.x, word.y - end.y);
		box.room = std::hypot(limit.x - end.x, limit.y - end.y);
		box.row = std::hypot(first.x - above.x, first.y - above.y);
		box.turn = std::atan2(end.y - start.y, end.x - start.x);
		return box;
	}

	bool SameSize(const Box& a_box, const Box& a_other)
	{
		constexpr double CLOSE_ENOUGH = 0.25;

		return std::abs(a_box.width - a_other.width) < CLOSE_ENOUGH &&
		       std::abs(a_box.gap - a_other.gap) < CLOSE_ENOUGH &&
		       std::abs(a_box.room - a_other.room) < CLOSE_ENOUGH &&
		       std::abs(a_box.row - a_other.row) < CLOSE_ENOUGH;
	}

	Bar Layout(Value& a_readout, const Box& a_box)
	{
		Bar bar;
		bar.width = a_box.width;
		bar.deep = a_box.row * BAR_DEEP;

		// The word is written at its largest, then again smaller when it runs
		// past the room in the box. Its field is scaled to its size, so its
		// width grows in step with it and 1 measure is enough, and a longer
		// word such as COND comes out smaller.
		HudParts::Readout::Label label{ .size = a_box.row * LABEL_SIZE, .gap = a_box.gap, .wide = LABEL_WIDE };
		bar.reach = HudParts::Readout::SetLabel(a_readout, label);
		if (bar.reach > a_box.room) {
			label.size *= (a_box.room - a_box.gap) / (bar.reach - a_box.gap);
			bar.reach = HudParts::Readout::SetLabel(a_readout, label);
		}
		bar.size = label.size;

		HudParts::Readout::SetTrack(a_readout, bar.width, bar.deep);
		return bar;
	}
}
