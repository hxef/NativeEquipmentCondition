#include "UI/Hud/HudCondition/Boxes.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace HudCondition
{
	namespace
	{
		using Flash::Point;
		using Roles::Box;
		using Roles::Hud::Part;
	}

	// -------------------------------------------------------------------
	// Boxes
	// -------------------------------------------------------------------

	Box BarAt(double a_x, double a_y, double a_length, double a_deep)
	{
		return Box{ a_x - a_length, a_y - (a_deep / 2.0), a_x, a_y + (a_deep / 2.0) };
	}

	double MiddleAcross(const Box& a_box)
	{
		return (a_box.left + a_box.right) / 2.0;
	}

	double MiddleDown(const Box& a_box)
	{
		return (a_box.top + a_box.bottom) / 2.0;
	}

	bool OnScreen(const Around& a_around, const Box& a_box)
	{
		return !a_around.frame || a_around.frame->Holds(a_box);
	}

	bool WordOnScreen(const Around& a_around, double a_x, double a_y)
	{
		return OnScreen(a_around, a_around.line.Moved(a_x, a_y));
	}

	Box WholeLine(const HudParts::Readout::LabelPlace& a_label)
	{
		return Box{ a_label.letters.left, a_label.letters.top, a_label.letters.right, a_label.letters.bottom + a_label.descent };
	}

	const Part* Covered(const Box& a_box, std::span<const Part> a_parts)
	{
		const auto it = std::ranges::find_if(a_parts, [&](const Part& a_part) { return a_part.shown && a_part.box.Meets(a_box); });
		return it != a_parts.end() ? &*it : nullptr;
	}

	bool Between(const Box& a_divider, std::span<const Part> a_numbers)
	{
		const auto line = MiddleDown(a_divider);
		const auto above = std::ranges::any_of(a_numbers, [&](const Part& a_number) { return MiddleDown(a_number.box) < line; });
		const auto below = std::ranges::any_of(a_numbers, [&](const Part& a_number) { return MiddleDown(a_number.box) > line; });
		return above && below;
	}

	std::vector<Try> Tries(const Box& a_anchor, const Box& a_box, double a_gap, const Point& a_aim, std::span<const Part> a_parts)
	{
		std::vector<double> across{ a_aim.x, a_anchor.right - a_box.right, MiddleAcross(a_anchor) - MiddleAcross(a_box), a_anchor.left - a_box.left };
		std::vector<double> down{ a_aim.y, MiddleDown(a_anchor) - MiddleDown(a_box), a_anchor.bottom - a_box.bottom, a_anchor.top - a_box.top };
		for (const auto& part : a_parts) {
			across.insert(across.end(), { part.box.left - a_gap - a_box.right, part.box.right + a_gap - a_box.left });
			down.insert(down.end(), { part.box.top - a_gap - a_box.bottom, part.box.bottom + a_gap - a_box.top });
		}

		std::vector<Try> tries;
		for (const auto& [y, side] : { std::pair{ a_anchor.bottom + a_gap - a_box.top, "below" }, std::pair{ a_anchor.top - a_gap - a_box.bottom, "above" } }) {
			for (const auto x : across) {
				if (a_box.left + x < a_anchor.right && a_anchor.left < a_box.right + x) {
					tries.push_back(Try{ x, y, side });
				}
			}
		}
		for (const auto& [x, side] : { std::pair{ a_anchor.left - a_gap - a_box.right, "left of" }, std::pair{ a_anchor.right + a_gap - a_box.left, "right of" } }) {
			for (const auto y : down) {
				if (a_box.top + y < a_anchor.bottom && a_anchor.top < a_box.bottom + y) {
					tries.push_back(Try{ x, y, side });
				}
			}
		}
		std::ranges::stable_sort(tries, {}, [&](const Try& a_try) { return std::hypot(a_try.x - a_aim.x, a_try.y - a_aim.y); });
		return tries;
	}
}
