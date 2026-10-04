#include "UI/Hud/HudCondition.h"

#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/MenuMovies.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace HudCondition
{
	namespace
	{
		using Scaleform::GFx::Value;

		constexpr const char* METERS_PATH = "_root.RightMeters_mc";
		constexpr const char* COUNTER_NAME = "AmmoCount_mc";

		// The divider the counter draws between its 2 numbers. The bar replaces
		// it.
		constexpr const char* DIVIDER_NAME = "AmmoLineInstance";

		// The name the readout carries, so the frame listener can find it.
		constexpr const char* READOUT_NAME = "NEC_Condition_mc";

		// How wide the divider is when there is none to measure: 44, with its
		// origin on its right end, as HUDMenu.swf ships it.
		constexpr double DIVIDER_WIDTH = 44.0;

		// The word's font size, which sets every other size, see
		// HudParts::Readout. That makes the bar 6 deep, where the divider is 2
		// and the gap between the numbers about 12.
		constexpr double TEXT_SIZE = 20.0;
		constexpr double BAR_DEEP = HudParts::Readout::BAR_DEEP * TEXT_SIZE;

		// How long the readout fades on its own, about as long as the counter's
		// own fade.
		constexpr double FADE_SECONDS = 0.25;

		// A check every 10 frames is several a second, which keeps up with a
		// weapon wearing hit by hit.
		constexpr std::uint32_t FRAMES_PER_READING = 10;

		// A value Percent never returns, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

		// Where the counter draws its divider, and so where the bar goes.
		// Measured from the HUD, so a HUD mod that moves the counter's parts
		// moves the bar with them.
		struct Divider
		{
			double x = 0.0;
			double y = 0.0;
			double width = DIVIDER_WIDTH;
		};

		// Where a clip sits and how big it is drawn.
		struct Transform
		{
			double x = 0.0;
			double y = 0.0;
			double scaleX = 1.0;
			double scaleY = 1.0;

			bool operator==(const Transform&) const = default;
		};

		// How far the player moved the bar from the divider, from fHudBarX and
		// fHudBarY, above 0 to the right and up. 0 and 0 is the divider's own
		// place.
		struct Shift
		{
			double x = 0.0;
			double y = 0.0;

			bool operator==(const Shift&) const = default;
		};

		Shift ShiftNow()
		{
			const Shift shift{ Settings::fHudBarX.GetValue(), Settings::fHudBarY.GetValue() };

			// A nan typed into the ini never equals itself, so the bar would be
			// placed again every frame. It counts as 0 and 0.
			return std::isfinite(shift.x) && std::isfinite(shift.y) ? shift : Shift{};
		}

		Transform TransformOf(const Value& a_clip)
		{
			return Transform{ Flash::Number(a_clip, "x"sv), Flash::Number(a_clip, "y"sv),
				Flash::Number(a_clip, "scaleX"sv), Flash::Number(a_clip, "scaleY"sv) };
		}

		// Reads where the counter draws its divider. The divider's origin is
		// its right end, which is where the bar's right end goes. A counter
		// without one gets what HUDMenu.swf ships.
		Divider MeasureDivider(const Value& a_counter)
		{
			Divider divider;

			Value drawn;
			if (!a_counter.GetMember(DIVIDER_NAME, &drawn) || !drawn.IsDisplayObject()) {
				return divider;
			}

			const auto width = Flash::Number(drawn, "width"sv);
			divider.x = Flash::Number(drawn, "x"sv);
			divider.y = Flash::Number(drawn, "y"sv);
			if (width > 0.0) {
				divider.width = width;
			}
			return divider;
		}

		// Lays the readout out along the divider it replaces. Both have their
		// origin on their right end, so the bar runs left over the divider's
		// length and the word follows past the end.
		void Layout(Value& a_readout, const Divider& a_divider)
		{
			HudParts::Readout::SetLabel(a_readout, { .size = TEXT_SIZE, .gap = HudParts::Readout::LABEL_GAP * TEXT_SIZE, .drop = HudParts::Readout::LABEL_DROP * TEXT_SIZE });
			HudParts::Readout::SetTrack(a_readout, a_divider.width, BAR_DEEP);
		}

		// Puts the readout where the divider is, moved by the player's shift.
		// It sits beside the counter in the same parent, so the counter's
		// position and scale are copied onto it. The HUD's y runs down, so the
		// shift's y is taken off.
		void Place(Value& a_readout, const Transform& a_counter, const Divider& a_divider, const Shift& a_shift)
		{
			a_readout.SetMember("x"sv, Value(a_counter.x + a_divider.x * a_counter.scaleX + a_shift.x));
			a_readout.SetMember("y"sv, Value(a_counter.y + a_divider.y * a_counter.scaleY - a_shift.y));
			a_readout.SetMember("scaleX"sv, Value(a_counter.scaleX));
			a_readout.SetMember("scaleY"sv, Value(a_counter.scaleY));
		}

		// Called by the HUD every frame through an enterFrame listener on the
		// readout. The event's currentTarget is the readout.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			using Clock = std::chrono::steady_clock;

			void Call(const Params& a_params) override
			{
				Value readout;
				Value meters;
				Value counter;
				if (a_params.argCount < 1 || !a_params.movie ||
					!a_params.args[0].GetMember("currentTarget"sv, &readout) || !readout.IsDisplayObject() ||
					!readout.GetMember("parent"sv, &meters) || !meters.IsDisplayObject() ||
					!meters.GetMember(COUNTER_NAME, &counter) || !counter.IsDisplayObject()) {
					return;
				}

				// The HUD menu is listed once it has finished being built, a
				// frame after its movie loads.
				if (!colored) {
					if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
						HudParts::AddColorTarget(*menu, readout);
						counterCanBeVisible = HudParts::ComponentCanBeVisible(*menu, RE::VTABLE::HUDAmmoCounter[0].address());
						colored = true;
						TraceLog::Line("menu", "HUD CND is a HUD part of its own, in the gameplay HUD colour, {:s}",
							counterCanBeVisible ? "and follows the ammo counter's HUD modes" : "but found no ammo counter to follow HUD modes by");
					}
				}

				const auto now = Clock::now();
				const auto elapsed = frames == 0 ? 0.0 : std::chrono::duration<double>(now - lastFrame).count();
				lastFrame = now;

				if (frames++ % FRAMES_PER_READING == 0) {
					HudParts::Weapon::Queue();
				}

				// The counter's parts are in place once the movie has run a
				// frame.
				if (!laidOut) {
					divider = MeasureDivider(counter);
					Layout(readout, divider);
					laidOut = true;
					TraceLog::Line("menu", "HUD CND bar takes the ammo divider's place at {:.0f},{:.0f}, {:.0f} across and {:.0f} deep",
						divider.x, divider.y, divider.width, BAR_DEEP);
				}

				const auto percent = HudParts::Weapon::Percent();
				if (percent != shownPercent) {
					HudParts::Readout::SetPercent(readout, divider.width, BAR_DEEP, percent);
					shownPercent = percent;
					if (percent >= 0) {
						TraceLog::Line("menu", "HUD CND bar drawn at {:d}%", percent);
					} else {
						TraceLog::Line("menu", "HUD CND bar has no reading to draw");
					}
				}

				// A HUD mod or a power armor HUD can move the counter, and the
				// bar goes with it. The MCM page can move the bar.
				const auto shape = TransformOf(counter);
				const auto shift = ShiftNow();
				if (!placed || shape != placedAgainst || shift != placedShift) {
					Place(readout, shape, divider, shift);
					placedAgainst = shape;
					placedShift = shift;
					placed = true;
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

				// The divider hides while the bar is drawn over it. With no
				// condition to show, or the bar moved off it, the counter keeps
				// its divider.
				const auto covered = counterShows && percent >= 0 && shift == Shift{};
				if (covered != dividerHidden) {
					Value drawn;
					if (counter.GetMember(DIVIDER_NAME, &drawn) && drawn.IsDisplayObject()) {
						drawn.SetMember("visible"sv, Value(!covered));
					}
					dividerHidden = covered;
					TraceLog::Line("menu", "HUD ammo divider {:s}, the bar moved by {:.0f},{:.0f}", covered ? "hides under the CND bar" : "shows", shift.x, shift.y);
				}

				if (opacity != shownOpacity) {
					if ((opacity > 0.0) != (shownOpacity > 0.0)) {
						TraceLog::Line("menu", "HUD CND {:s} {:s}, percent {}", opacity > 0.0 ? "shows" : "hides",
							counterShows ? "in place of the ammo divider" : "on its own", percent);
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
				divider = Divider{};
				placedAgainst = Transform{};
				placedShift = Shift{};
				placed = false;
				laidOut = false;
				colored = false;
				dividerHidden = false;
				counterCanBeVisible = nullptr;
			}

		private:
			Clock::time_point lastFrame;
			std::uint32_t     frames = 0;
			std::int32_t      shownPercent = NOT_DRAWN;
			double            shownOpacity = -1.0;
			double            fade = 0.0;
			Divider           divider;
			Transform         placedAgainst;
			Shift             placedShift;
			bool              placed = false;
			bool              laidOut = false;
			bool              colored = false;
			bool              dividerHidden = false;

			// The ammo counter's canBeVisible, found along with the HUD menu.
			const bool* counterCanBeVisible = nullptr;
		};

		// Lives as long as the plugin, the same way as the item card listener.
		FrameListener g_frameListener;

		// Adds the readout and its frame listener to the HUD movie.
		void Build(Scaleform::GFx::Movie& a_movie)
		{
			Value meters;
			Value counter;
			if (!a_movie.GetVariable(&meters, METERS_PATH) || !meters.IsDisplayObject() ||
				!meters.GetMember(COUNTER_NAME, &counter) || !counter.IsDisplayObject()) {
				REX::WARN("The HUD has no ammo counter at {:s}.{:s}, so it shows no CND. A HUD replacer may have moved it.", METERS_PATH, COUNTER_NAME);
				return;
			}

			// One readout beside the counter, in the same parent, so it stays
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
			TraceLog::Line("menu", "HUDMenu.swf loaded, CND takes the divider's place between the ammo numbers");
		}

		HudParts::Waiter g_waiter{ Settings::bHudCondition, &Build };
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			g_waiter.Watch(a_movie, "the CND bar");
		}
	}
}
