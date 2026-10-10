#include "UI/Roles/Hud.h"

#include "UI/Flash.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Roles::Hud
{
	namespace
	{
		// -------------------------------------------------------------------
		// Vanilla's places
		// -------------------------------------------------------------------

		// The path the engine binds its ammo counter to as the HUD loads. The
		// counter reads its 2 numbers off the clip there.
		constexpr const char* COUNTER_PATH = "_root.RightMeters_mc.AmmoCount_mc";

		// The "---" the counter draws between its 2 numbers, a child of the
		// counter.
		constexpr const char* DIVIDER_NAME = "AmmoLineInstance";

		// The field a quick container row draws its name in.
		constexpr const char* NAME_FIELD = "ItemName_tf";

		// The counter's divider, or a value that is not a display object.
		[[nodiscard]] Value DividerOf(Value& a_counter)
		{
			Value divider;
			if (a_counter.IsDisplayObject()) {
				a_counter.GetMember(DIVIDER_NAME, &divider);
			}
			return divider;
		}

		// A clip's own visible, or nothing when it reads as neither true nor
		// false.
		[[nodiscard]] std::optional<bool> VisibleOf(Value& a_clip)
		{
			Value visible;
			if (!a_clip.IsDisplayObject() || !a_clip.GetMember("visible"sv, &visible) || !visible.IsBoolean()) {
				return std::nullopt;
			}
			return visible.GetBoolean();
		}
	}

	// -------------------------------------------------------------------
	// The ammo counter
	// -------------------------------------------------------------------

	std::optional<Value> AmmoCounter(Scaleform::GFx::Movie& a_movie)
	{
		// A counter taken off the HUD keeps its path but has no parent.
		Value counter;
		Value parent;
		if (!a_movie.GetVariable(&counter, COUNTER_PATH) || !counter.IsDisplayObject() ||
			!counter.GetMember("parent"sv, &parent) || !parent.IsDisplayObject()) {
			return std::nullopt;
		}
		Found(a_movie, "ammo counter"sv, "where the engine binds it"sv);
		return counter;
	}

	// -------------------------------------------------------------------
	// The divider
	// -------------------------------------------------------------------

	std::optional<Line> Divider(Scaleform::GFx::Movie& a_movie, Value& a_counter, std::optional<bool> a_ownVisible)
	{
		auto divider = DividerOf(a_counter);
		if (!divider.IsDisplayObject()) {
			return std::nullopt;
		}

		// NEC writes true only to give back the movie's own, so a true is the
		// movie's own.
		const auto shown = VisibleOf(divider).value_or(true) || a_ownVisible.value_or(false);
		const auto alpha = Flash::Number(divider, "alpha"sv);
		const auto rotation = Flash::Number(divider, "rotation"sv);
		if (!shown || !(alpha > 0.0) || rotation != 0.0 || !InFrame(a_movie, divider)) {
			return std::nullopt;
		}

		// x, y and width are in the counter's units, width the bounds the
		// divider takes there.
		const Line line{ Flash::Number(divider, "x"sv), Flash::Number(divider, "y"sv), Flash::Number(divider, "width"sv) };
		if (!std::isfinite(line.x) || !std::isfinite(line.y) || !std::isfinite(line.width) || line.width <= 0.0) {
			return std::nullopt;
		}
		Found(a_movie, "ammo divider"sv, "at vanilla's place"sv);
		return line;
	}

	void Cover(Value& a_counter, bool a_covered, std::optional<bool>& a_ownVisible)
	{
		auto       divider = DividerOf(a_counter);
		const auto shown = VisibleOf(divider);
		if (!shown) {
			a_ownVisible.reset();
			return;
		}

		// A true while covered was written by the movie since, so it is the
		// movie's own and is given back later.
		if (a_covered) {
			if (!a_ownVisible || *shown) {
				a_ownVisible = *shown;
			}
			if (*shown) {
				Flash::Set(divider, "visible"sv, Value(false));
			}
			return;
		}
		if (a_ownVisible) {
			if (*shown != *a_ownVisible) {
				Flash::Set(divider, "visible"sv, Value(*a_ownVisible));
			}
			a_ownVisible.reset();
		}
	}

	// -------------------------------------------------------------------
	// A quick container row's text
	// -------------------------------------------------------------------

	namespace
	{
		// How close a part's right edge sits to the name's start to count as
		// placed against it.
		constexpr double AGAINST = 4.0;

		// Where a text field's first line starts and ends, in its parent's
		// units. Nothing for a clip that is no text field, or that shows no
		// text unless a_empty.
		struct Span
		{
			double start = 0.0;
			double end = 0.0;
		};

		[[nodiscard]] std::optional<Span> SpanOf(Value& a_field, bool a_empty = false)
		{
			Value       text;
			Value       metrics;
			const Value line{ 0 };
			if (!a_field.IsDisplayObject() || !a_field.GetMember("text"sv, &text) || !text.IsString() || (!a_empty && !*text.GetString()) ||
				!a_field.Invoke("getLineMetrics", &metrics, &line, 1) || !metrics.IsObject()) {
				return std::nullopt;
			}
			const auto start = Flash::Number(a_field, "x"sv) + Flash::Number(metrics, "x"sv);
			const auto end = start + Flash::Number(metrics, "width"sv);
			if (!std::isfinite(start) || !std::isfinite(end)) {
				return std::nullopt;
			}
			return Span{ start, end };
		}

		// The children of a clip, from the bottom up.
		[[nodiscard]] std::vector<Value> ChildrenOf(Value& a_clip)
		{
			std::vector<Value> children;
			const auto         count = Flash::Number(a_clip, "numChildren"sv);
			for (std::uint32_t i = 0; i < count; i++) {
				if (auto child = Flash::ChildAt(a_clip, i); child.IsDisplayObject()) {
					children.push_back(child);
				}
			}
			return children;
		}

		[[nodiscard]] bool Shows(const Value& a_clip)
		{
			return a_clip.IsDisplayObject() && Flash::Opacity(a_clip) > 0.0;
		}

		// A clip's right edge in a_space's units. A clip holding others counts
		// by its shown children only, since it may keep hidden ones of other
		// widths.
		[[nodiscard]] std::optional<double> RightEdge(Value& a_clip, Value& a_space)
		{
			if (Flash::Number(a_clip, "numChildren"sv) > 0.0) {
				std::optional<double> right;
				for (auto& child : ChildrenOf(a_clip)) {
					const auto edge = Shows(child) ? RightEdge(child, a_space) : std::nullopt;
					if (edge) {
						right = std::max(right.value_or(*edge), *edge);
					}
				}
				return right;
			}
			Value bounds;
			if (!a_clip.IsDisplayObject() || !a_clip.Invoke("getBounds", &bounds, &a_space, 1) || !bounds.IsObject()) {
				return std::nullopt;
			}
			const auto right = Flash::Number(bounds, "x"sv) + Flash::Number(bounds, "width"sv);
			return std::isfinite(right) ? std::optional{ right } : std::nullopt;
		}
	}

	std::optional<double> TextEnd(Scaleform::GFx::Movie& a_movie, Value& a_row)
	{
		std::optional<double> end;
		auto                  others = false;
		for (auto& child : ChildrenOf(a_row)) {
			const auto span = SpanOf(child);
			if (span && OnScreen(a_movie, child)) {
				end = std::max(end.value_or(span->end), span->end);
				others = others || Flash::String(child, "name"sv) != NAME_FIELD;
			}
		}

		// An item with an empty name still ends where vanilla's name field
		// puts its empty line.
		if (!end) {
			auto       field = Flash::Child(a_row, NAME_FIELD);
			const auto span = SpanOf(field, true);
			if (span && OnScreen(a_movie, field)) {
				end = span->end;
			}
		}
		if (end) {
			if (others) {
				Found(a_movie, "quick container name in other text fields"sv, "by a walk over the row"sv);
			} else {
				Found(a_movie, "quick container name's end"sv, "at vanilla's place"sv);
			}
		}
		return end;
	}

	std::optional<Start> TextStart(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_name, const Value& a_ours)
	{
		const auto span = SpanOf(a_name);
		if (!span || !OnScreen(a_movie, a_name)) {
			return std::nullopt;
		}

		// Children are told apart by name, since a Value cannot say whether
		// it holds the same clip as another.
		const auto name = Flash::String(a_name, "name"sv);
		const auto ours = Flash::String(a_ours, "name"sv);
		Start      start{ span->start, {} };
		for (auto& child : ChildrenOf(a_row)) {
			const auto own = Flash::String(child, "name"sv);
			if (!Shows(child) || own == name || own == ours) {
				continue;
			}
			const auto right = RightEdge(child, a_row);
			if (right && std::abs(*right - start.x) <= AGAINST) {
				start.against.push_back(child);
			}
		}
		if (!start.against.empty()) {
			Found(a_movie, "part placed against a quick container name"sv, "by its right edge"sv);
		}
		return start;
	}
}
