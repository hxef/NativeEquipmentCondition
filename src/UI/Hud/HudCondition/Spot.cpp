#include "UI/Hud/HudCondition/Spot.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Hud/HudCondition/Boxes.h"

#include <algorithm>
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

		// The word's font size, which sets its gap and drop, see
		// HudParts::Readout.
		constexpr double TEXT_SIZE = 20.0;

		// A word this close past a side of the bar sits by that side, so
		// rounding never moves it into the bar.
		constexpr double SIDE_LEEWAY = 0.01;

		// A flipped word moves left from its mirror by at most 2 of its gaps
		// from the bar, 1 to clear a number field that reaches into its
		// letters and 1 to keep its gap. A field that asks more reaches far
		// into the bar's row and covers the word, which then takes the
		// nearest free spot beside the bar rather than sit apart from it.
		constexpr double MOST_CLEAR_PER_GAP = 2.0;

		// How long a choice holds before NEC.log says it. A HUD mod seen
		// moved its parts for 0.3 seconds after the HUD loaded.
		constexpr std::chrono::seconds SETTLE{ 1 };

		// The keys NEC.log has said this game start.
		std::set<std::string, std::less<>> g_told;

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

		// How much further left a flipped word whose letters sit at a_word
		// moves to keep a_gap from every shown number right of it in its
		// rows, the gap it keeps from the bar, and the number that asks the
		// most. 0 and null when none comes that close.
		struct Clearing
		{
			double      by = 0.0;
			const Part* from = nullptr;
		};

		[[nodiscard]] Clearing Clear(const Box& a_word, double a_gap, std::span<const Part> a_numbers)
		{
			Clearing clearing;
			for (const auto& number : a_numbers) {
				const auto& box = number.box;
				const auto  rows = box.top < a_word.bottom && a_word.top < box.bottom;
				const auto  by = a_word.right + a_gap - box.left;
				if (number.shown && rows && box.right > a_word.right && by > clearing.by) {
					clearing = { by, &number };
				}
			}
			return clearing;
		}

		// a_own, then a_word after a_and, or a_word alone when a_own is empty.
		[[nodiscard]] std::string Joined(const std::string& a_own, std::string_view a_and, const std::string& a_word)
		{
			return a_own.empty() ? a_word : std::format("{:s}{:s}{:s}", a_own, a_and, a_word);
		}

		// The bar at a_line, on the divider's place, and its word at its own
		// place, else in the nearest free spot beside the bar, a gap as wide
		// as the one past the end away. With none free the word stays at its
		// own place. a_said and a_key say why the bar sits there, empty for a
		// bar that fits. A flipped key ends in flipped, so NEC.log says the
		// flipped wording once too. A flipped word's own place is as far left
		// as Clear asks, so it never crowds the numbers it sits beside, up to
		// MOST_CLEAR_PER_GAP. Past that, the number covers the word at its
		// mirror. Clear is worked out unflipped too, for Turned.
		[[nodiscard]] Choice WithWord(const Around& a_around, const Roles::Hud::Line& a_line, std::span<const Part> a_keepOff, const std::string& a_said,
			const std::string& a_key)
		{
			const auto  mirror = Home(true, a_line.width, a_around.word);
			const auto  gap = std::max(a_around.word.left, 0.0);
			const auto  clear = Clear(a_around.word.Moved(a_line.x + mirror, a_line.y), gap, a_around.numbers);
			const auto  fits = clear.by <= MOST_CLEAR_PER_GAP * gap;
			const auto  cleared = fits ? clear.by : 0.0;
			const auto  home = a_around.flipped ? mirror - cleared : 0.0;
			const Spot  own{ a_line, a_around.deep, true, home, 0.0, a_around.flipped, cleared };
			const auto  word = a_around.word.Moved(a_line.x + home, a_line.y);
			const auto* over = a_around.flipped && !fits ? clear.from : Covered(word, a_keepOff);
			const auto  shown = WordOnScreen(a_around, a_line.x + home, a_line.y);
			if (shown && !over && a_around.flipped && cleared > 0.0) {
				return Choice{ own, Joined(a_said, ", and its "sv, std::format("word sits {:.1f} further left, to keep its gap from {:s}", clear.by, clear.from->name)),
					Joined(a_key, ", "sv, "word further left of " + clear.from->name + ", flipped") };
			}
			if (shown && !over) {
				return Choice{ own, a_said, a_key };
			}

			const auto reason = shown ? "cover " + over->name : std::string{ "run off screen" };
			const auto kind = std::format("{:s}{:s}", shown ? "covers " + over->name : std::string{ "off screen" }, a_around.flipped ? ", flipped" : "");
			const auto from = a_around.flipped ? "before the bar's start"sv : "past the bar's end"sv;
			const auto bar = BarAt(a_line.x, a_line.y, a_line.width, a_around.deep);
			for (const auto& at : Tries(bar, a_around.word, std::max(a_around.word.left, 0.0), Point{ a_line.x + home, a_line.y }, a_keepOff)) {
				const auto moved = a_around.word.Moved(at.x, at.y);
				if (WordOnScreen(a_around, at.x, at.y) && !Covered(moved, a_keepOff)) {
					auto spot = own;
					spot.wordX = at.x - a_line.x;
					spot.wordY = at.y - a_line.y;
					return Choice{ spot, Joined(a_said, ", and its "sv, std::format("word sits {:s} its bar, since {:s} it would {:s}", at.side, from, reason)),
						Joined(a_key, ", "sv, std::format("word {:s}, {:s}", at.side, kind)) };
				}
			}
			return Choice{ own,
				Joined(a_said, ", and its "sv,
					std::format("word stays {:s} its bar {:s}, since no spot beside the bar is free", a_around.flipped ? "before" : "past", shown ? "over " + over->name : "partly off screen")),
				Joined(a_key, ", "sv, "word stays, " + kind) };
		}

		// -------------------------------------------------------------------
		// Off the divider
		// -------------------------------------------------------------------

		// The nearest free spot for the bar with its word at its own place,
		// or nothing. The bar keeps the divider's length, or 4 times its
		// depth if that is longer, and its distance is from its middle to the
		// divider's.
		[[nodiscard]] std::optional<Choice> Nearest(const Around& a_around, const Why& a_why, std::span<const Part> a_others)
		{
			const auto  length = std::max(a_around.divider.length, MIN_LENGTH_PER_DEEP * a_around.deep);
			const auto& divider = a_around.divider.box;
			const auto  home = Home(a_around.flipped, length, a_around.word);
			const auto  word = a_around.word.Moved(home, 0.0);

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
			for (const auto& at : Tries(drawn, bar.With(word), a_around.deep, aim, keepOff)) {
				const auto movedBar = bar.Moved(at.x, at.y);
				const auto movedWord = word.Moved(at.x, at.y);
				if (!OnScreen(a_around, movedBar) || !OnScreen(a_around, movedWord) || Covered(movedBar, keepOff) || Covered(movedWord, keepOff)) {
					continue;
				}
				if (WordOnScreen(a_around, at.x + home, at.y)) {
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
			return Choice{ Spot{ Roles::Hud::Line{ best->x, best->y, length }, a_around.deep, false, home, 0.0, a_around.flipped },
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

	double Home(bool a_flipped, double a_length, const Box& a_letters)
	{
		return a_flipped ? -a_length - a_letters.left - a_letters.right : 0.0;
	}

	Spot Turned(const Spot& a_spot, bool a_flipped, const Box& a_letters)
	{
		auto turned = a_spot;
		if (a_spot.flipped == a_flipped) {
			return turned;
		}
		turned.flipped = a_flipped;
		const auto width = a_spot.line.width;
		const auto own = a_spot.wordY == 0.0 && (a_spot.flipped ? a_spot.wordX <= Home(true, width, a_letters) : a_spot.wordX == 0.0);
		if (own) {
			turned.wordX = Home(a_flipped, width, a_letters) - (a_flipped ? a_spot.clear : 0.0);
		}
		return turned;
	}

	HudParts::Readout::LabelPlace Label(Roles::Value& a_readout, bool a_first, const Flash::Point& a_moved)
	{
		if (a_first) {
			HudParts::Readout::SetLabel(a_readout, { .size = TEXT_SIZE, .gap = HudParts::Readout::LABEL_GAP * TEXT_SIZE, .drop = HudParts::Readout::LABEL_DROP * TEXT_SIZE });
		}
		auto label = HudParts::Readout::LabelExtent(a_readout);
		label.field = label.field.Moved(-a_moved.x, -a_moved.y);
		label.letters = label.letters.Moved(-a_moved.x, -a_moved.y);
		const auto& field = label.field;
		const auto& letters = label.letters;
		if (a_first || label.measured) {
			TraceLog::Line("menu", "HUD CND word's letters fill {:.1f},{:.1f} to {:.1f},{:.1f} from the bar's end, its field {:.1f},{:.1f} to {:.1f},{:.1f}{:s}",
				letters.left, letters.top, letters.right, letters.bottom, field.left, field.top, field.right, field.bottom, label.measured ? "" : ", not measured yet");
		}
		return label;
	}

	std::optional<Choice> Choose(Scaleform::GFx::Movie& a_movie, Roles::Value& a_counter, const Roles::Value& a_readout,
		std::optional<bool> a_ownVisible, const HudParts::Readout::LabelPlace& a_label, bool a_flipped)
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
			DEEP_PER_THICK * shape->thick, frame, a_flipped };

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

	Spot Sized(const Spot& a_spot, const HudParts::Readout::Size& a_size, const HudParts::Readout::LabelPlace& a_label)
	{
		auto sized = a_spot;
		sized.line.width *= a_size.length;
		sized.deep *= a_size.thickness;

		// The side is told by the word's letters, as WithWord placed them.
		const auto own = BarAt(0.0, 0.0, a_spot.line.width, a_spot.deep);
		const auto grown = BarAt(0.0, 0.0, sized.line.width, sized.deep);
		const auto word = a_label.letters.Moved(a_spot.wordX, a_spot.wordY);
		const auto below = word.top >= own.bottom - SIDE_LEEWAY;
		const auto above = !below && word.bottom <= own.top + SIDE_LEEWAY;
		if (below) {
			sized.wordY += grown.bottom - own.bottom;
		} else if (above) {
			sized.wordY += grown.top - own.top;
		} else if (word.right <= own.left + SIDE_LEEWAY) {
			sized.wordX += grown.left - own.left;
		}

		// A flipped bar's right end, the readout's origin, moves by what the
		// bar grew. A word above or below is moved back by as much, and a
		// word left of the bar already is, so only a word right of the bar
		// moves with that end.
		if (a_spot.flipped) {
			const auto grownBy = sized.line.width - a_spot.line.width;
			sized.line.x += grownBy;
			if (below || above) {
				sized.wordX -= grownBy;
			}
		}
		return sized;
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
