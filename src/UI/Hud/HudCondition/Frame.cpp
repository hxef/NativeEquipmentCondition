#include "UI/Hud/HudCondition/Frame.h"

#include "UI/Flash.h"
#include "UI/Hud/HudCondition/Listener.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>

namespace HudCondition
{
	using Scaleform::GFx::Value;

	namespace
	{
		// -------------------------------------------------------------------
		// The readout
		// -------------------------------------------------------------------

		// How long the readout fades on its own, about as long as the counter's
		// own fade.
		constexpr double FADE_SECONDS = 0.25;

		// The bar takes the colour the divider is drawn in, checked every
		// reading, since the HUD colour or a HUD mod can change it. In vanilla
		// the counter is a HUD part as the bar is, so both are expected to
		// carry the HUD colour and nothing is written. Alpha stays the bar's
		// own. A colour written again and again is traced once, by a_told.
		void Recolor(Value& a_readout, Value& a_counter, std::optional<Flash::Tint>& a_told)
		{
			const auto wanted = Roles::Hud::DividerTint(a_counter);
			const auto now = Flash::TintOf(a_readout);
			if (!wanted || !now || now->Near(*wanted) || !Flash::SetTint(a_readout, *wanted) || (a_told && a_told->Near(*wanted))) {
				return;
			}
			a_told = wanted;
			TraceLog::Line("menu", "HUD CND bar takes the colour the ammo divider is drawn in, times {:.3f} {:.3f} {:.3f} plus {:.0f} {:.0f} {:.0f}",
				wanted->red, wanted->green, wanted->blue, wanted->redAdd, wanted->greenAdd, wanted->blueAdd);
		}
	}

	// -------------------------------------------------------------------
	// The frame listener
	// -------------------------------------------------------------------

	void FrameListener::Call(const Params& a_params)
	{
		Value readout;
		if (a_params.argCount < 1 || !a_params.movie ||
			!a_params.args[0].GetMember("currentTarget"sv, &readout) || !readout.IsDisplayObject()) {
			return;
		}

		// The bar copies the counter's place, so it stays as last drawn
		// while the counter is gone or not in the bar's parent.
		auto  found = Roles::Hud::AmmoCounter(*a_params.movie, readout);
		Value meters;
		Value beside;
		if (!found || !readout.GetMember("parent"sv, &meters) || !meters.IsDisplayObject() ||
			!meters.Invoke("contains", &beside, &*found, 1) || !beside.IsBoolean() || !beside.GetBoolean()) {
			Roles::Noted(*a_params.movie, "ammo counter"sv, "the CND bar stays as last drawn"sv);
			return;
		}
		auto& counter = *found;

		const auto now = Clock::now();

		// The HUD menu is listed once it has finished being built, a
		// frame after its movie loads. The colour is copied from a
		// reading after the colour target is made, so the HUD colour
		// lands first.
		if (!colored) {
			if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
				HudParts::AddColorTarget(*menu, readout);
				recolorFrom = now + HudParts::Weapon::READING_EVERY;
				counterPart = HudParts::AmmoCounterPart(*menu);
				colored = true;
				TraceLog::Line("menu", "HUD CND is a HUD part of its own, in the gameplay HUD colour, {:s}",
					counterPart ? "and follows the ammo counter's HUD modes" : "but found no ammo counter to follow HUD modes by");
			}
		}

		const auto elapsed = lastFrame ? std::chrono::duration<double>(now - *lastFrame).count() : 0.0;
		lastFrame = now;

		const auto reading = !lastReading || now - *lastReading >= HudParts::Weapon::READING_EVERY;
		if (reading) {
			lastReading = now;
			HudParts::Weapon::Queue();
			// The letters are measured again each reading until a line
			// reads, so a field not laid out yet is not kept for good.
			if (!label.measured) {
				label = Label(readout, !labelled, laidOut ? Flash::Point{ laidOut->wordX, laidOut->wordY } : Flash::Point{});
				labelled = true;
			}
			const auto choice = Choose(*a_params.movie, counter, readout, ownVisible, label);
			Follow(choice);
			if (choice) {
				teller.Tell(*choice, now);
			}
			if (colored && now >= recolorFrom) {
				Recolor(readout, counter, toldTint);
			}
		}

		// The bar is drawn at the size the player set, laid out again
		// when the spot or that size changes.
		const auto size = SizeNow();
		const auto drawn = Sized(spot, size, label);
		if (drawn != laidOut) {
			LayOut(readout, drawn, size);
		}

		HudParts::Readout::FollowWordSwitch(readout, Settings::bHudBarShowsWord, wordShown, "HUD"sv);

		const auto percent = HudParts::Weapon::Percent(Settings::bHudCondition);
		if (percent != shownPercent) {
			HudParts::Readout::SetPercent(readout, drawn.line.width, drawn.deep, percent);
			shownPercent = percent;
			if (percent >= 0) {
				TraceLog::Line("menu", "HUD CND bar drawn at {:d}%", percent);
			} else {
				TraceLog::Line("menu", "HUD CND bar has no reading to draw");
			}
		}

		// A HUD mod or a power armor HUD can move the counter or a clip
		// it sits in, and the bar goes with it. The MCM page can move
		// the bar, and a new screen size can move the screen's edges.
		const auto shown = a_params.movie->GetVisibleFrameRect();
		const Basis basis{ TransformOf(counter), ParentsOf(meters), ShiftNow(), Flash::Box{ shown.x1, shown.y1, shown.x2, shown.y2 } };
		if (reading || basis != placedFrom) {
			placedFrom = basis;
			PlaceOnScreen(*a_params.movie, meters, counter, readout, basis, drawn, size);
		}

		// While the counter shows, the bar fades with it. A melee
		// weapon hides the counter, so the bar fades on its own, shown
		// while the weapon is out. When the HUD mode hides the counter,
		// in the Pip-Boy for example, the bar fades too, even with a
		// weapon out. A hidden counter with rounds to count keeps the
		// bar hidden while bHudBarHidesWithCounter is on. While it is
		// off, a bar that stays never dips as the counter fades out. A
		// bar that stands alone has no counter to fade with, so it
		// shows while the weapon is out, as a melee weapon's does.
		const auto hudAllows = !counterPart || counterPart->hudModes.canBeVisible;
		const auto alone = hudAllows && HudParts::Weapon::Drawn();
		const auto hidesWithCounter = !standsAlone && counterPart && Settings::bHudBarHidesWithCounter.GetValue() && HudParts::HasRounds(*counterPart);
		const auto counterOpacity = Flash::Opacity(counter);
		const auto counterShows = !standsAlone && counterOpacity > 0.0;
		const auto stays = alone && !hidesWithCounter;

		auto opacity = 0.0;
		if (percent < 0) {
			fade = 0.0;
		} else if (counterShows) {
			opacity = stays ? std::max(counterOpacity, fade) : counterOpacity;
			fade = opacity;
		} else {
			const auto target = stays ? 1.0 : 0.0;
			const auto step = elapsed / FADE_SECONDS;
			fade = target > fade ? std::min(target, fade + step) : std::max(target, fade - step);
			opacity = fade;
		}
		if (!placeable) {
			opacity = 0.0;
			fade = 0.0;
		}

		// The divider hides while the bar is drawn over it, checked
		// every frame, since a movie can show it again. With no
		// condition to show, or the bar off it, the counter keeps its
		// divider.
		const auto covered = counterShows && percent >= 0 && onSpot && spot.onDivider;
		Roles::Hud::Cover(counter, covered, ownVisible);
		if (covered != dividerHidden) {
			dividerHidden = covered;
			TraceLog::Line("menu", "HUD ammo divider {:s}, the bar moved by {:.0f},{:.0f}", covered ? "hides under the CND bar" : "shows", basis.shift.x, basis.shift.y);
		}

		if (opacity != shownOpacity) {
			if ((opacity > 0.0) != (shownOpacity > 0.0)) {
				TraceLog::Line("menu", "HUD CND {:s} {:s}, percent {}", opacity > 0.0 ? "shows" : "hides",
					!counterShows ? (alone && hidesWithCounter ? "with the ammo counter" : "on its own") : (spot.onDivider ? "in place of the ammo divider" : "beside the ammo counter"), percent);
			}
			readout.SetMember("alpha"sv, Value(opacity));
			readout.SetMember("visible"sv, Value(opacity > 0.0));
			shownOpacity = opacity;
		}
	}

	void FrameListener::Reset()
	{
		lastFrame.reset();
		lastReading.reset();
		shownPercent = NOT_DRAWN;
		shownOpacity = -1.0;
		fade = 0.0;
		spot = SHIPPED;
		label = HudParts::Readout::LabelPlace{};
		teller.Reset();
		placedFrom.reset();
		placedAt.reset();
		placeable = false;
		onSpot = false;
		held = false;
		standsAlone = false;
		aloneAt.reset();
		followed = false;
		laidOut.reset();
		laidOutSize = HudParts::Readout::Size{};
		labelled = false;
		wordShown.reset();
		colored = false;
		recolorFrom = {};
		toldTint.reset();
		dividerHidden = false;
		ownVisible.reset();
		counterPart = nullptr;
	}

	namespace
	{
		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;
	}

	Scaleform::GFx::FunctionHandler& Frames()
	{
		return g_frameListener;
	}

	void ResetFrames()
	{
		g_frameListener.Reset();
	}
}
