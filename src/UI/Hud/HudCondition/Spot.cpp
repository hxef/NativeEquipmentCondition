#include "UI/Hud/HudCondition/Spot.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace HudCondition
{
	namespace
	{
		using Flash::Point;
		using Roles::Box;
		using Roles::Hud::Part;

		// A bar shorter than 4 times its depth reads as a dot, not a level.
		constexpr double MIN_LENGTH_PER_DEEP = 4.0;

		// How long a choice holds before NEC.log says it. A HUD mod seen
		// moved its parts for 0.3 seconds after the HUD loaded.
		constexpr std::chrono::seconds SETTLE{ 1 };

		// The keys NEC.log has said this game start.
		std::set<std::string, std::less<>> g_told;

		// What the HUD shows around the counter now, in the counter's units.
		// inside holds the counter's other children but a backdrop, beside the
		// parts next to the counter. word is where the word's letters sit from
		// the bar's right end and line the same down to the bottom of their
		// line, deep how deep the bar is, and frame the screen, or nothing
		// while it cannot be told.
		struct Around
		{
			Roles::Hud::DividerShape divider;
			std::vector<Part>        numbers;
			std::vector<Part>        inside;
			std::vector<Part>        beside;
			Box                      word;
			Box                      line;
			double                   deep = 0.0;
			std::optional<Box>       frame;
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

		// -------------------------------------------------------------------
		// Boxes
		// -------------------------------------------------------------------

		[[nodiscard]] Box BarAt(double a_x, double a_y, double a_length, double a_deep)
		{
			return Box{ a_x - a_length, a_y - (a_deep / 2.0), a_x, a_y + (a_deep / 2.0) };
		}

		[[nodiscard]] double MiddleAcross(const Box& a_box)
		{
			return (a_box.left + a_box.right) / 2.0;
		}

		[[nodiscard]] double MiddleDown(const Box& a_box)
		{
			return (a_box.top + a_box.bottom) / 2.0;
		}

		// Whether a box is on screen. While the screen cannot be told every
		// box is.
		[[nodiscard]] bool OnScreen(const Around& a_around, const Box& a_box)
		{
			return !a_around.frame || a_around.frame->Holds(a_box);
		}

		// Whether the word's whole line, the box Filled holds, is on screen
		// with the bar's right end at a_x, a_y.
		[[nodiscard]] bool WordOnScreen(const Around& a_around, double a_x, double a_y)
		{
			return OnScreen(a_around, a_around.line.Moved(a_x, a_y));
		}

		// The word's letters down to the bottom of their line.
		[[nodiscard]] Box WholeLine(const HudParts::Readout::LabelPlace& a_label)
		{
			return Box{ a_label.letters.left, a_label.letters.top, a_label.letters.right, a_label.letters.bottom + a_label.descent };
		}

		// The first shown part a box would cover, or null.
		[[nodiscard]] const Part* Covered(const Box& a_box, std::span<const Part> a_parts)
		{
			const auto it = std::ranges::find_if(a_parts, [&](const Part& a_part) { return a_part.shown && a_part.box.Meets(a_box); });
			return it != a_parts.end() ? &*it : nullptr;
		}

		// Whether the divider runs between the numbers, with the middle of
		// one above its own and the middle of another below. Numbers shown
		// or not count, so one that drops out for a moment moves nothing.
		[[nodiscard]] bool Between(const Box& a_divider, std::span<const Part> a_numbers)
		{
			const auto line = MiddleDown(a_divider);
			const auto above = std::ranges::any_of(a_numbers, [&](const Part& a_number) { return MiddleDown(a_number.box) < line; });
			const auto below = std::ranges::any_of(a_numbers, [&](const Part& a_number) { return MiddleDown(a_number.box) > line; });
			return above && below;
		}

		// Origins that put a_box, drawn from its own origin, beside a_anchor
		// and a_gap away from it, nearest a_aim first. On each side the box
		// lines up with a_aim, with the anchor's ends and middle, and with
		// each part's edges a_gap away, and part of it stays level with the
		// anchor.
		[[nodiscard]] std::vector<Try> Tries(const Box& a_anchor, const Box& a_box, double a_gap, const Point& a_aim, std::span<const Part> a_parts)
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

		// -------------------------------------------------------------------
		// On the divider
		// -------------------------------------------------------------------

		// Why a bar on the divider's own place does not fit, or nothing.
		[[nodiscard]] std::optional<Why> Unfit(const Around& a_around, std::span<const Part> a_others)
		{
			const auto& divider = a_around.divider;
			if (divider.turn != 0.0) {
				return Why{ std::format("its divider is turned {:.0f} degrees", divider.turn), "turned" };
			}

			const auto& place = divider.place;
			if (place.width < MIN_LENGTH_PER_DEEP * a_around.deep) {
				return Why{ std::format("its divider is {:.1f} across, too short for a bar {:.1f} deep", place.width, a_around.deep), "short" };
			}

			const auto  bar = BarAt(place.x, place.y, place.width, a_around.deep);
			const auto* covered = Between(divider.box, a_around.numbers) ? nullptr : Covered(bar, a_around.numbers);
			if (!covered) {
				covered = Covered(bar, a_others);
			}
			if (covered) {
				return Why{ std::format("a bar on its divider would cover {:s}", covered->name), "covers " + covered->name };
			}
			if (!OnScreen(a_around, bar)) {
				return Why{ "a bar on its divider would run off screen", "off screen" };
			}
			return std::nullopt;
		}

		// a_own, then a_word after a_and, or a_word alone when a_own is empty.
		[[nodiscard]] std::string Joined(const std::string& a_own, std::string_view a_and, const std::string& a_word)
		{
			return a_own.empty() ? a_word : std::format("{:s}{:s}{:s}", a_own, a_and, a_word);
		}

		// The bar at a_line, on the divider's place, and its word past its
		// end, else in the nearest free spot beside the bar, a gap as wide as
		// the one past the end away. With none free the word stays past the
		// end. a_said and a_key say why the bar sits there, empty for a bar
		// that fits.
		[[nodiscard]] Choice WithWord(const Around& a_around, const Roles::Hud::Line& a_line, std::span<const Part> a_keepOff, const std::string& a_said,
			const std::string& a_key)
		{
			Spot        spot{ a_line, a_around.deep, true };
			const auto  word = a_around.word.Moved(a_line.x, a_line.y);
			const auto* over = Covered(word, a_keepOff);
			const auto  shown = WordOnScreen(a_around, a_line.x, a_line.y);
			if (shown && !over) {
				return Choice{ spot, a_said, a_key };
			}

			const auto reason = shown ? "cover " + over->name : std::string{ "run off screen" };
			const auto kind = shown ? "covers " + over->name : std::string{ "off screen" };
			const auto bar = BarAt(a_line.x, a_line.y, a_line.width, a_around.deep);
			for (const auto& at : Tries(bar, a_around.word, std::max(a_around.word.left, 0.0), Point{ a_line.x, a_line.y }, a_keepOff)) {
				const auto moved = a_around.word.Moved(at.x, at.y);
				if (WordOnScreen(a_around, at.x, at.y) && !Covered(moved, a_keepOff)) {
					spot.wordX = at.x - a_line.x;
					spot.wordY = at.y - a_line.y;
					return Choice{ spot, Joined(a_said, ", and its "sv, std::format("word sits {:s} its bar, since past the bar's end it would {:s}", at.side, reason)),
						Joined(a_key, ", "sv, std::format("word {:s}, {:s}", at.side, kind)) };
				}
			}
			return Choice{ spot,
				Joined(a_said, ", and its "sv, std::format("word stays past its bar {:s}, since no spot beside the bar is free", shown ? "over " + over->name : "partly off screen")),
				Joined(a_key, ", "sv, "word stays, " + kind) };
		}

		// -------------------------------------------------------------------
		// Off the divider
		// -------------------------------------------------------------------

		// The nearest free spot for the bar with its word past its end, or
		// nothing. The bar keeps the divider's length, or 4 times its depth if
		// that is longer, and its distance is from its middle to the
		// divider's.
		[[nodiscard]] std::optional<Choice> Nearest(const Around& a_around, const Why& a_why, std::span<const Part> a_others)
		{
			const auto  length = std::max(a_around.divider.length, MIN_LENGTH_PER_DEEP * a_around.deep);
			const auto& divider = a_around.divider.box;

			// What the counter draws: its divider, its shown numbers and its
			// other children.
			auto drawn = divider;
			for (const auto& part : a_around.numbers) {
				drawn = part.shown ? drawn.With(part.box) : drawn;
			}
			for (const auto& part : a_around.inside) {
				drawn = drawn.With(part.box);
			}

			// The divider shows again once the bar is off it, so the bar keeps
			// off it too.
			std::vector<Part> keepOff{ a_around.numbers };
			keepOff.insert(keepOff.end(), a_others.begin(), a_others.end());
			keepOff.push_back(Part{ "its divider", divider });

			// With no try that fits the word's whole line, the first whose
			// letters fit still beats the divider's place. The HUD then lifts
			// it by less than the line reaches below the letters.
			const auto         bar = BarAt(0.0, 0.0, length, a_around.deep);
			const Point        aim{ MiddleAcross(divider) + (length / 2.0), MiddleDown(divider) };
			std::optional<Try> best;
			for (const auto& at : Tries(drawn, bar.With(a_around.word), a_around.deep, aim, keepOff)) {
				const auto movedBar = bar.Moved(at.x, at.y);
				const auto movedWord = a_around.word.Moved(at.x, at.y);
				if (!OnScreen(a_around, movedBar) || !OnScreen(a_around, movedWord) || Covered(movedBar, keepOff) || Covered(movedWord, keepOff)) {
					continue;
				}
				if (WordOnScreen(a_around, at.x, at.y)) {
					best = at;
					break;
				}
				if (!best) {
					best = at;
				}
			}
			if (!best) {
				return std::nullopt;
			}
			return Choice{ Spot{ Roles::Hud::Line{ best->x, best->y, length }, a_around.deep, false },
				std::format("bar sits {:s} the ammo counter, since {:s}", best->side, a_why.said), std::format("bar {:s}, {:s}", best->side, a_why.kind) };
		}

		// With no free spot the bar keeps the divider's place, as before NEC
		// looked for a spot, and its word takes a free spot as on a divider
		// that fits. A turned divider counts by its place before the turn.
		[[nodiscard]] Choice Kept(const Around& a_around, const Why& a_why, std::span<const Part> a_keepOff)
		{
			const auto&            divider = a_around.divider;
			const Roles::Hud::Line line{ divider.place.x, divider.place.y, divider.turn == 0.0 ? divider.place.width : divider.length };
			return WithWord(a_around, line, a_keepOff, std::format("bar keeps the ammo divider's place, since {:s} and no spot by the ammo counter is free", a_why.said),
				"divider, " + a_why.kind);
		}
	}

	std::optional<Choice> Choose(Scaleform::GFx::Movie& a_movie, Roles::Value& a_counter, const Roles::Value& a_readout,
		std::optional<bool> a_ownVisible, const HudParts::Readout::LabelPlace& a_label)
	{
		const auto shape = Roles::Hud::Divider(a_movie, a_counter, a_ownVisible);
		if (!shape) {
			Roles::Noted(a_movie, "ammo divider"sv, "the CND bar keeps its last place"sv);
			return std::nullopt;
		}

		// The screen counts only while the counter is on it, since a HUD mod
		// may hide the counter by moving it off. A child whose box holds every
		// number is a backdrop behind them.
		const auto frame = Roles::InFrame(a_movie, a_counter) ? Roles::FrameIn(a_movie, a_counter) : std::nullopt;
		auto       numbers = Roles::Hud::Numbers(a_counter);
		auto       inside = Roles::Hud::Inside(a_counter, frame);
		std::erase_if(inside, [&](const Part& a_part) {
			return !numbers.empty() && std::ranges::all_of(numbers, [&](const Part& a_number) { return a_part.box.Holds(a_number.box); });
		});
		const Around around{ *shape, std::move(numbers), std::move(inside), Roles::Hud::Beside(a_counter, a_readout, frame), a_label.letters, WholeLine(a_label),
			DEEP_PER_THICK * shape->thick, frame };

		std::vector<Part> others{ around.inside };
		others.insert(others.end(), around.beside.begin(), around.beside.end());
		std::vector<Part> keepOff{ around.numbers };
		keepOff.insert(keepOff.end(), others.begin(), others.end());
		const auto why = Unfit(around, others);
		if (!why) {
			return WithWord(around, around.divider.place, keepOff, {}, {});
		}
		if (auto nearest = Nearest(around, *why, others)) {
			return nearest;
		}
		return Kept(around, *why, keepOff);
	}

	Box Filled(const Spot& a_spot, const HudParts::Readout::LabelPlace& a_label)
	{
		return BarAt(0.0, 0.0, a_spot.line.width, a_spot.deep).With(WholeLine(a_label).Moved(a_spot.wordX, a_spot.wordY));
	}

	void Teller::Tell(const Choice& a_choice, Clock::time_point a_now)
	{
		if (a_choice.key != held) {
			held = a_choice.key;
			since = a_now;
			return;
		}
		if (!held.empty() && a_now - since >= SETTLE && g_told.insert(held).second) {
			REX::INFO("The HUD's CND {:s}.", a_choice.said);
		}
	}

	void Teller::Reset()
	{
		held.clear();
		since = {};
	}
}
