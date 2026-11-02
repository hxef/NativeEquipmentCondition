#include "UI/Hud/HudCondition/Frame.h"

#include "UI/Flash.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace HudCondition
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The readout
		// -------------------------------------------------------------------

		// Where the bar goes with no divider measured: 44 wide, with its
		// origin on its right end, as HUDMenu.swf ships the divider, and as
		// deep as on that divider, which is 2 thick.
		constexpr Spot SHIPPED{ { 0.0, 0.0, 44.0 }, DEEP_PER_THICK * 2.0, true };

		// How long the readout fades on its own, about as long as the counter's
		// own fade.
		constexpr double FADE_SECONDS = 0.25;

		// A value Percent never returns, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

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

		// -------------------------------------------------------------------
		// The frame listener
		// -------------------------------------------------------------------

		// Called by the HUD every frame through an enterFrame listener on the
		// readout. The event's currentTarget is the readout.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			using Clock = std::chrono::steady_clock;

			void Call(const Params& a_params) override
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

			void Reset()
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

		private:
			// Places the bar on screen, as drawn at a_size, and keeps that
			// place for the MCM page. While its counter is wholly off screen,
			// which is how HUD mods hide the counter, the bar hides, or with
			// bHudBarStandsAlone on stands alone. It hides while the movie
			// refuses a point, so it is never drawn where NEC cannot tell it
			// is on screen.
			void PlaceOnScreen(Scaleform::GFx::Movie& a_movie, Value& a_meters, Value& a_counter, Value& a_readout, const Basis& a_basis, const Spot& a_drawn,
				const HudParts::Readout::Size& a_size)
			{
				const auto counterIn = Roles::InFrame(a_movie, a_counter);
				const auto standing = !counterIn && Settings::bHudBarStandsAlone.GetValue();
				const auto filled = Filled(a_drawn, label);
				std::optional<Placed> placed;
				if (counterIn) {
					placed = Place(a_movie, a_meters, a_basis, a_drawn.line, filled);
				} else if (standing) {
					placed = PlaceAlone(a_movie, a_meters, a_basis, filled);
				}
				placeable = placed.has_value();
				onSpot = placed && placed->onSpot;
				standsAlone = placed && placed->alone;
				TellAlone(placed, a_basis);
				if (!placed) {
					Roles::Noted(a_movie, counterIn || standing ? "stage place for the CND bar"sv : "on screen ammo counter"sv,
						counterIn || standing ? "the bar hides"sv : "the CND bar hides with it"sv);
					return;
				}
				if (placed->at != placedAt) {
					Put(a_readout, placed->at);
					placedAt = placed->at;
				}
				Keep(*placed, spot, a_size, label, a_basis.frame);
				if (placed->held != held) {
					held = placed->held;
					if (held) {
						TraceLog::Line("menu", "HUD CND bar held whole on screen, {:.0f},{:.0f} short of where fHudBarX and fHudBarY put it", placed->shortBy.x, placed->shortBy.y);
					} else {
						TraceLog::Line("menu", "HUD CND bar sits where fHudBarX and fHudBarY put it");
					}
				}
			}

			// Says where a bar that stands alone starts, from the screen's top
			// left in HUD units as the MCM page shows it, each time a whole
			// unit changes.
			void TellAlone(const std::optional<Placed>& a_placed, const Basis& a_basis)
			{
				std::optional<std::array<long, 2>> at;
				if (a_placed && a_placed->alone) {
					at = std::array{ std::lround(a_basis.shift.x - a_placed->shortBy.x), std::lround(a_placed->shortBy.y - a_basis.shift.y) };
				}
				if (at == aloneAt) {
					return;
				}
				aloneAt = at;
				if (at) {
					TraceLog::Line("menu", "HUD CND bar stands alone {:d},{:d} from the screen's top left in HUD units, with no ammo counter on screen to follow", (*at)[0], (*at)[1]);
				}
			}

			// The first reading takes the spot chosen or the shipped place.
			// Later only a spot that differs is taken. The frame then lays the
			// bar out there.
			void Follow(const std::optional<Choice>& a_choice)
			{
				if (followed && (!a_choice || a_choice->spot == spot)) {
					return;
				}
				spot = a_choice ? a_choice->spot : spot;
				followed = true;
				const auto said = a_choice ? std::string_view{ a_choice->said } : std::string_view{};
				if (!spot.onDivider) {
					TraceLog::Line("menu", "HUD CND {:s}, at {:.0f},{:.0f}, {:.0f} across and {:.1f} deep", said, spot.line.x, spot.line.y, spot.line.width, spot.deep);
					return;
				}
				TraceLog::Line("menu", "HUD CND bar takes the ammo divider's place at {:.0f},{:.0f}, {:.0f} across and {:.1f} deep",
					spot.line.x, spot.line.y, spot.line.width, spot.deep);
				if (!said.empty()) {
					TraceLog::Line("menu", "HUD CND {:s}, the word moved by {:.0f},{:.0f}", said, spot.wordX, spot.wordY);
				}
			}

			// Draws the track and moves the word for a_drawn, the spot at
			// a_size, then draws the level and places the bar again.
			void LayOut(Value& a_readout, const Spot& a_drawn, const HudParts::Readout::Size& a_size)
			{
				HudParts::Readout::SetTrack(a_readout, a_drawn.line.width, a_drawn.deep);
				HudParts::Readout::MoveLabel(a_readout, label.field.left + a_drawn.wordX, label.field.top + a_drawn.wordY);
				laidOut = a_drawn;
				shownPercent = NOT_DRAWN;
				placedFrom.reset();
				if (a_size != laidOutSize) {
					laidOutSize = a_size;
					TraceLog::Line("menu", "HUD CND bar sized to {:g} times its length and {:g} times its thickness, {:.0f} across and {:.1f} deep",
						a_size.length, a_size.thickness, a_drawn.line.width, a_drawn.deep);
				}
			}

			std::optional<Clock::time_point> lastFrame;
			std::optional<Clock::time_point> lastReading;
			std::int32_t                     shownPercent = NOT_DRAWN;
			double                           shownOpacity = -1.0;
			double                           fade = 0.0;
			Spot                             spot = SHIPPED;
			Teller                           teller;
			bool                             followed = false;
			bool                             labelled = false;
			bool                             colored = false;
			bool                             dividerHidden = false;

			// Where the word and its letters sit at its place past the bar,
			// and whether it was last shown, see FollowWordSwitch.
			HudParts::Readout::LabelPlace label;
			std::optional<bool>           wordShown;

			// The spot as last laid out, at the size it was laid out at.
			std::optional<Spot>     laidOut;
			HudParts::Readout::Size laidOutSize;

			// What the bar was last placed from and where it went. placeable
			// is false while it has no place, its counter off screen with no
			// standing alone or the movie refusing a point, onSpot while it
			// sits on its spot, held while the screen's edge holds it short of
			// the shift, and standsAlone while it stands alone, at aloneAt as
			// last traced.
			std::optional<Basis>               placedFrom;
			std::optional<Transform>           placedAt;
			bool                               placeable = false;
			bool                               onSpot = false;
			bool                               held = false;
			bool                               standsAlone = false;
			std::optional<std::array<long, 2>> aloneAt;

			// When a reading may first copy the counter's colour, and the
			// colour last traced.
			Clock::time_point          recolorFrom;
			std::optional<Flash::Tint> toldTint;

			// The divider's own visible from before NEC's bar covered it, given
			// back when the bar moves off.
			std::optional<bool> ownVisible;

			// Found along with the HUD menu, see HudParts::AmmoCounterPart.
			const RE::HUDAmmoCounter* counterPart = nullptr;
		};

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
