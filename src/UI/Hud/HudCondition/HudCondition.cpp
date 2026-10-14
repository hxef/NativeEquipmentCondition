#include "UI/Hud/HudCondition/HudCondition.h"

#include "UI/Flash.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/Hud.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <chrono>
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

		// The name the readout carries, so the frame listener can find it.
		constexpr const char* READOUT_NAME = "NEC_Condition_mc";

		// Where the bar goes with no divider measured: 44 wide, with its
		// origin on its right end, as HUDMenu.swf ships the divider, and as
		// deep as on that divider, which is 2 thick.
		constexpr Spot SHIPPED{ { 0.0, 0.0, 44.0 }, DEEP_PER_THICK * 2.0, true };

		// The word's font size, which sets its gap and drop, see
		// HudParts::Readout.
		constexpr double TEXT_SIZE = 20.0;

		// How long the readout fades on its own, about as long as the counter's
		// own fade.
		constexpr double FADE_SECONDS = 0.25;

		// A check every 10 frames is several a second, which keeps up with a
		// weapon wearing hit by hit.
		constexpr std::uint32_t FRAMES_PER_READING = 10;

		// A value Percent never returns, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

		// Writes the word with a_first, once a movie since it never changes,
		// and returns where it sits from the bar's right end, a_moved taken
		// back, which is how far Follow moved it. Both have their origin on
		// their right end, so the bar runs left and the word follows past it.
		// The trace says the first try and the one that measures.
		HudParts::Readout::LabelPlace Label(Value& a_readout, bool a_first, const Flash::Point& a_moved)
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
				auto  found = Roles::Hud::AmmoCounter(*a_params.movie);
				Value meters;
				Value beside;
				if (!found || !readout.GetMember("parent"sv, &meters) || !meters.IsDisplayObject() ||
					!meters.Invoke("contains", &beside, &*found, 1) || !beside.IsBoolean() || !beside.GetBoolean()) {
					Roles::Noted(*a_params.movie, "ammo counter"sv, "the CND bar stays as last drawn"sv);
					return;
				}
				auto& counter = *found;

				// The HUD menu is listed once it has finished being built, a
				// frame after its movie loads. The colour is copied from a
				// reading after the colour target is made, so the game's own
				// colour lands first.
				if (!colored) {
					if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
						HudParts::AddColorTarget(*menu, readout);
						recolorFrom = frames + FRAMES_PER_READING;
						counterCanBeVisible = HudParts::ComponentCanBeVisible(*menu, RE::VTABLE::HUDAmmoCounter[0].address());
						colored = true;
						TraceLog::Line("menu", "HUD CND is a HUD part of its own, in the gameplay HUD colour, {:s}",
							counterCanBeVisible ? "and follows the ammo counter's HUD modes" : "but found no ammo counter to follow HUD modes by");
					}
				}

				const auto now = Clock::now();
				const auto elapsed = frames == 0 ? 0.0 : std::chrono::duration<double>(now - lastFrame).count();
				lastFrame = now;

				const auto reading = frames++ % FRAMES_PER_READING == 0;
				if (reading) {
					HudParts::Weapon::Queue();
					// The letters are measured again each reading until a line
					// reads, so a field not laid out yet is not kept for good.
					if (!label.measured) {
						label = Label(readout, !labelled, Flash::Point{ spot.wordX, spot.wordY });
						labelled = true;
					}
					const auto choice = Choose(*a_params.movie, counter, readout, ownVisible, label);
					Follow(choice, readout);
					if (choice) {
						teller.Tell(*choice, now);
					}
					if (colored && frames > recolorFrom) {
						Recolor(readout, counter, toldTint);
					}
				}

				const auto percent = HudParts::Weapon::Percent();
				if (percent != shownPercent) {
					HudParts::Readout::SetPercent(readout, spot.line.width, spot.deep, percent);
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
					PlaceOnScreen(*a_params.movie, meters, counter, readout, basis);
				}

				// A gun keeps the counter showing, and the bar fades with it. A
				// melee weapon hides the counter, so the bar fades on its own,
				// shown while the weapon is out. When the HUD mode hides the
				// counter, in the Pip-Boy for example, the bar fades too, even
				// with a weapon out.
				const auto hudAllows = !counterCanBeVisible || *counterCanBeVisible;
				const auto counterOpacity = Flash::Opacity(counter);
				const auto counterShows = counterOpacity > 0.0;

				auto opacity = 0.0;
				if (percent < 0) {
					fade = 0.0;
				} else if (counterShows) {
					opacity = counterOpacity;
					fade = counterOpacity;
				} else {
					const auto target = hudAllows && HudParts::Weapon::Drawn() ? 1.0 : 0.0;
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
							!counterShows ? "on its own" : (spot.onDivider ? "in place of the ammo divider" : "beside the ammo counter"), percent);
					}
					readout.SetMember("alpha"sv, Value(opacity));
					readout.SetMember("visible"sv, Value(opacity > 0.0));
					shownOpacity = opacity;
				}
			}

			void Reset()
			{
				frames = 0;
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
				laidOut = false;
				labelled = false;
				colored = false;
				recolorFrom = 0;
				toldTint.reset();
				dividerHidden = false;
				ownVisible.reset();
				counterCanBeVisible = nullptr;
			}

		private:
			// Places the bar on screen and keeps that place for the MCM page.
			// The bar hides while its counter is wholly off screen, which is
			// how HUD mods hide the counter, and while the movie refuses a
			// point, so it is never drawn where NEC cannot tell it is on
			// screen.
			void PlaceOnScreen(Scaleform::GFx::Movie& a_movie, Value& a_meters, Value& a_counter, Value& a_readout, const Basis& a_basis)
			{
				const auto counterIn = Roles::InFrame(a_movie, a_counter);
				const auto placed = counterIn ? Place(a_movie, a_meters, a_basis, spot.line, Filled(spot, label)) : std::nullopt;
				placeable = placed.has_value();
				onSpot = placed && placed->onSpot;
				if (!placed) {
					Roles::Noted(a_movie, counterIn ? "stage place for the CND bar"sv : "on screen ammo counter"sv,
						counterIn ? "the bar hides"sv : "the CND bar hides with it"sv);
					return;
				}
				if (placed->at != placedAt) {
					Put(a_readout, placed->at);
					placedAt = placed->at;
				}
				Keep(placed->box, a_basis.frame);
				if (placed->held != held) {
					held = placed->held;
					if (held) {
						TraceLog::Line("menu", "HUD CND bar held whole on screen, {:.0f},{:.0f} short of where fHudBarX and fHudBarY put it", placed->shortBy.x, placed->shortBy.y);
					} else {
						TraceLog::Line("menu", "HUD CND bar sits where fHudBarX and fHudBarY put it");
					}
				}
			}

			// The first reading lays the bar out at the spot chosen or the
			// shipped place. Later only a spot that differs lays it out,
			// draws and places it again.
			void Follow(const std::optional<Choice>& a_choice, Value& a_readout)
			{
				if (laidOut && (!a_choice || a_choice->spot == spot)) {
					return;
				}
				spot = a_choice ? a_choice->spot : spot;
				HudParts::Readout::SetTrack(a_readout, spot.line.width, spot.deep);
				HudParts::Readout::MoveLabel(a_readout, label.field.left + spot.wordX, label.field.top + spot.wordY);
				laidOut = true;
				shownPercent = NOT_DRAWN;
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

			Clock::time_point lastFrame;
			std::uint32_t     frames = 0;
			std::int32_t      shownPercent = NOT_DRAWN;
			double            shownOpacity = -1.0;
			double            fade = 0.0;
			Spot              spot = SHIPPED;
			Teller            teller;
			bool              laidOut = false;
			bool              labelled = false;
			bool              colored = false;
			bool              dividerHidden = false;

			// Where the word and its letters sit at its place past the bar.
			HudParts::Readout::LabelPlace label;

			// What the bar was last placed from and where it went. placeable
			// is false while its counter is off screen or the movie refused a
			// point, onSpot while it sits on its spot, and held while the
			// screen's edge holds it short of the shift.
			std::optional<Basis>     placedFrom;
			std::optional<Transform> placedAt;
			bool                     placeable = false;
			bool                     onSpot = false;
			bool                     held = false;

			// The first frame count a reading may copy the counter's colour
			// at, and the colour last traced.
			std::uint32_t              recolorFrom = 0;
			std::optional<Flash::Tint> toldTint;

			// The divider's own visible from before NEC's bar covered it, given
			// back when the bar moves off.
			std::optional<bool> ownVisible;

			// The ammo counter's canBeVisible, found along with the HUD menu.
			const bool* counterCanBeVisible = nullptr;
		};

		FrameListener g_frameListener;

		// -------------------------------------------------------------------
		// Building the bar
		// -------------------------------------------------------------------

		// Adds the readout and its frame listener to the HUD movie.
		void Build(Scaleform::GFx::Movie& a_movie)
		{
			auto  counter = Roles::Hud::AmmoCounter(a_movie);
			Value meters;
			if (!counter || !counter->GetMember("parent"sv, &meters) || !meters.IsDisplayObject()) {
				Roles::Missing(a_movie, "ammo counter"sv, "the HUD shows no CND bar outside power armor"sv);
				return;
			}

			// 1 readout beside the counter, in the same parent, so it stays
			// when the counter hides. The first frames measure the divider, lay
			// the parts out and colour them. Nothing is added to the HUD until
			// the whole readout is built, so a movie that refuses any part
			// keeps its own divider.
			auto readout = HudParts::Readout::Create(a_movie, READOUT_NAME);
			if (!readout.IsDisplayObject()) {
				REX::WARN("The HUD movie would not take a CND bar, so it shows no CND.");
				return;
			}
			meters.Invoke("addChild", std::array{ readout });

			// A new HUD movie starts with nothing drawn.
			g_frameListener.Reset();

			Value listener;
			a_movie.CreateFunction(&listener, &g_frameListener);
			if (!readout.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
				REX::WARN("The HUD refused the frame listener, so its CND bar never updates.");
				return;
			}

			HudParts::Weapon::Queue();
			TraceLog::Line("menu", "HUDMenu.swf loaded, CND goes by the ammo counter");
		}

		HudParts::Waiter g_waiter{ Settings::bHudCondition, &Build };
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			Forget();
			g_waiter.Watch(a_movie, "the CND bar");
		}
	}
}
