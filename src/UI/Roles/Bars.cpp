#include "UI/Roles/Bars.h"

#include "UI/Flash.h"
#include "UI/Roles/Conventions.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace Roles::Bars
{
	namespace
	{
		// -------------------------------------------------------------------
		// Reading a list of hints
		// -------------------------------------------------------------------

		// How deep under menuObj a bar of the movie's own is looked for.
		constexpr std::uint32_t MAX_DEPTH = 3;

		// A stop for the walk, in clips looked at. A menu movie has a few
		// dozen within 3 levels of menuObj.
		constexpr std::uint32_t MAX_CLIPS = 500;

		// How many hints a list holds, or nothing when it is no list.
		[[nodiscard]] std::optional<std::uint32_t> CountOf(const Value& a_list)
		{
			if (a_list.IsArray()) {
				return a_list.GetArraySize();
			}
			const auto length = Flash::Number(a_list, "length"sv, -1.0);
			if (!std::isfinite(length) || length < 0.0) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(length);
		}

		// Whether a_list holds a_hint, asked of the list itself, since a
		// Value cannot tell whether 2 values are the same object.
		[[nodiscard]] bool Holds(Value& a_list, const Value& a_hint)
		{
			Value at;
			return a_list.IsObject() && a_hint.IsObject() && a_list.Invoke("indexOf", &at, &a_hint, 1) &&
			       Flash::AsNumber(at) >= 0.0;
		}

		// The trace line for a bar, once per movie load: the engine's, or one
		// of the movie's own and the method it answered.
		void SayFound(Scaleform::GFx::Movie& a_movie, const Conventions::Bar* a_convention)
		{
			if (!a_convention) {
				Found(a_movie, "button bar"sv, "through the engine"sv);
				return;
			}
			Found(a_movie, "own button bar"sv, std::format("by its {:s} method", a_convention->hints));
		}

		// -------------------------------------------------------------------
		// The walk for bars of the movie's own
		// -------------------------------------------------------------------

		struct Own
		{
			Value                   list;
			Value                   bar;
			const Conventions::Bar* convention = nullptr;
			std::uint32_t           count = 0;
		};

		// The bar a_clip is, when it answers both methods of a row in
		// Conventions.h and hands back a list.
		[[nodiscard]] std::optional<Own> Answer(Value& a_clip)
		{
			for (const auto& convention : Conventions::BARS) {
				Value list;
				if (!a_clip.IsDisplayObject() || !a_clip.HasMember(convention.hints) || !a_clip.HasMember(convention.redraw) ||
					!a_clip.Invoke(convention.hints, &list)) {
					continue;
				}
				if (const auto count = CountOf(list)) {
					return Own{ list, a_clip, &convention, *count };
				}
			}
			return std::nullopt;
		}

		// Adds every bar of the movie's own under a_clip, a_depth levels deep
		// at most. A bar's own children are buttons, so the walk stops there.
		void Walk(Value& a_clip, std::uint32_t a_depth, std::uint32_t& a_budget, std::vector<Own>& a_bars)
		{
			const auto children = Flash::Number(a_clip, "numChildren"sv, 0.0);
			for (std::uint32_t i = 0; i < children && a_budget > 0; i++) {
				a_budget--;
				auto child = Flash::ChildAt(a_clip, i);
				if (!child.IsDisplayObject()) {
					continue;
				}
				if (auto own = Answer(child)) {
					a_bars.push_back(std::move(*own));
				} else if (a_depth > 1) {
					Walk(child, a_depth - 1, a_budget, a_bars);
				}
			}
		}

		[[nodiscard]] std::vector<Own> OwnBars(RE::GameMenuBase& a_menu)
		{
			std::vector<Own> bars;
			auto             budget = MAX_CLIPS;
			if (a_menu.menuObj.IsDisplayObject()) {
				Walk(a_menu.menuObj, MAX_DEPTH, budget, bars);
			}
			return bars;
		}
	}

	// -------------------------------------------------------------------
	// The list a hint belongs in
	// -------------------------------------------------------------------

	std::optional<Hints> InUse(RE::GameMenuBase& a_menu, const Value& a_ours)
	{
		if (!a_menu.uiMovie) {
			return std::nullopt;
		}
		auto& movie = *a_menu.uiMovie;

		// The engine's list first, so a movie that keeps it, as vanilla's
		// does, is answered without a walk once the hint is in.
		auto* const engine = a_menu.buttonHintBar.get();
		Value       list = engine ? engine->sourceButtons : Value{};
		const auto  held = CountOf(list);
		if (held && Holds(list, a_ours)) {
			return Hints{ list, *engine, nullptr, true };
		}

		auto bars = OwnBars(a_menu);
		for (auto& bar : bars) {
			if (Holds(bar.list, a_ours)) {
				return Hints{ bar.list, bar.bar, bar.convention, true };
			}
		}
		if (held && *held > 0) {
			SayFound(movie, nullptr);
			return Hints{ list, *engine };
		}

		// A list can hold only hidden hints while a box is up, and they
		// still count, so the main bar keeps REPAIR then.
		const Own* most = nullptr;
		bool       tie = false;
		for (const auto& bar : bars) {
			if (bar.count == 0 || (most && bar.count < most->count)) {
				continue;
			}
			tie = most && bar.count == most->count;
			most = &bar;
		}
		if (!most) {
			return std::nullopt;
		}
		if (tie) {
			Noted(movie, "button bar holding the most hints"sv, "REPAIR joins none of the 2 that tie"sv);
			return std::nullopt;
		}
		SayFound(movie, most->convention);
		return Hints{ most->list, most->bar, most->convention };
	}

	// -------------------------------------------------------------------
	// Adding a hint and visiting every hint
	// -------------------------------------------------------------------

	bool Add(Hints& a_hints, Value& a_hint)
	{
		Value pushed;
		if (!a_hints.list.IsObject() || !a_hints.list.Invoke("push", &pushed, &a_hint, 1)) {
			return false;
		}
		a_hints.ours = true;

		// Handing the list back is how the engine's bar hears of a change.
		if (a_hints.convention) {
			Flash::Call(a_hints.bar, a_hints.convention->redraw);
		} else {
			Flash::Call(a_hints.bar, "SetButtonHintData", std::span{ &a_hints.list, 1 });
		}
		return true;
	}

	void ForEach(RE::GameMenuBase& a_menu, Scaleform::GFx::FunctionHandler& a_visit)
	{
		if (!a_menu.uiMovie) {
			return;
		}
		auto& movie = *a_menu.uiMovie;

		Value visit;
		movie.CreateFunction(&visit, &a_visit);
		const std::span<const Value> args{ &visit, 1 };

		// A movie may leave the engine's list empty and draw its own bar.
		auto* const engine = a_menu.buttonHintBar.get();
		if (const auto held = engine ? CountOf(engine->sourceButtons) : std::nullopt; held && *held > 0) {
			SayFound(movie, nullptr);
			Flash::Call(engine->sourceButtons, "forEach", args);
		}
		for (auto& bar : OwnBars(a_menu)) {
			SayFound(movie, bar.convention);
			Flash::Call(bar.list, "forEach", args);
		}
	}
}
