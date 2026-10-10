#include "UI/Hud/PowerArmorCondition/PowerArmorCondition.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/PowerArmorCondition/Dash.h"
#include "UI/Hud/PowerArmorCondition/Layout.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace PowerArmorCondition
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The bar's name and pace
		// -------------------------------------------------------------------

		// The name the readout carries, so the frame listener can find it.
		constexpr const char* READOUT_NAME = "NEC_PACondition_mc";

		// How long the bar fades, about as quick as the rest of the HUD.
		constexpr double FADE_SECONDS = 0.25;

		// How often the weapon's condition is checked, a few times a second.
		// The dash is measured every frame, since it sways.
		constexpr std::uint32_t FRAMES_PER_READING = 10;

		// A value Percent never returns, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

		// -------------------------------------------------------------------
		// The frame listener
		// -------------------------------------------------------------------

		// Called by the HUD every frame through an enterFrame listener on the
		// bar.
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

				// The HUD menu is listed a frame after its movie loads.
				if (!colored) {
					if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
						HudParts::AddColorTarget(*menu, readout);
						colored = true;
						TraceLog::Line("menu", "Power armor CND is a HUD part of its own, in the gameplay HUD colour");
					}
				}

				const auto now = Clock::now();
				const auto elapsed = frames == 0 ? 0.0 : std::chrono::duration<double>(now - lastFrame).count();
				lastFrame = now;

				// The dash is measured every frame. The weapon's condition
				// costs more to check and changes slowly, so it is checked less
				// often.
				if (frames % FRAMES_PER_READING == 0) {
					HudParts::Weapon::Queue();
				}
				++frames;
				QueueAnchor();

				const auto anchor = CurrentAnchor();
				const auto measured = anchor.miss == Miss::kNone;
				const auto percent = HudParts::Weapon::Percent();
				const auto frame = FrameOf(*a_params.movie);
				auto       box = measured ? Place(frame, anchor) : Box{};

				// The MCM page can move the bar off the digits. It still sways
				// with the dash, and only its place changes, never its size.
				// The movie's y runs down, so above 0 is taken off to go up. A
				// nan typed into the ini counts as 0, as for the HUD's bar.
				const auto offset = [](float a_value) { return std::isfinite(a_value) ? a_value : 0.0F; };
				box.x += offset(Settings::fPowerArmorBarX.GetValue());
				box.y -= offset(Settings::fPowerArmorBarY.GetValue());

				// Laying out again means measuring text, so it waits for the
				// dash to be drawn at a different size. Moving and turning is 3
				// numbers, every frame.
				if (measured && !SameSize(box, laidOutFor)) {
					bar = Layout(readout, box);
					laidOutFor = box;
					shownPercent = NOT_DRAWN;
					TraceLog::Line("menu", "Power armor CND bar is {:.0f} across and {:.1f} deep, its word at size {:.1f} and ending {:.1f} past it of the {:.1f} the box leaves, a row of the dash being {:.1f}, turned {:.2f} degrees from level",
						bar.width, bar.deep, bar.size, bar.reach, box.room, box.row, box.turn * DEGREES);
				}

				// Everything has to hold: the dash on screen and measured, a
				// weapon that wears, and room in the movie for all 4 corners
				// around the bar and its word. A readout that would go past the
				// edge is not drawn.
				const auto length = bar.width + bar.reach;
				const auto half = bar.size / 2.0;
				const auto fits = measured &&
				                  frame.Holds(box.At(0.0, -half)) &&
				                  frame.Holds(box.At(0.0, half)) &&
				                  frame.Holds(box.At(length, -half)) &&
				                  frame.Holds(box.At(length, half));
				const auto wanted = InPowerArmor() && DashAllowed() && percent >= 0 && fits;

				if (fits) {
					// The readout's origin is the right end of its bar, so it
					// goes the bar's width along from the bar's left end,
					// turned the way the bar runs.
					const auto origin = box.At(bar.width, 0.0);
					readout.SetMember("x"sv, Value(origin.x));
					readout.SetMember("y"sv, Value(origin.y));
					readout.SetMember("rotation"sv, Value(box.turn * DEGREES));

					// The dash sizes the bar, so its scale stays 1. A HUD mod
					// that scales every clip on the root would size it twice. A
					// movie can scale it every frame, so the line is said once per
					// HUD load.
					if (Flash::Number(readout, "scaleX"sv) != 1.0 || Flash::Number(readout, "scaleY"sv) != 1.0) {
						readout.SetMember("scaleX"sv, Value(1.0));
						readout.SetMember("scaleY"sv, Value(1.0));
						if (!saidScaled) {
							saidScaled = true;
							TraceLog::Line("menu", "Power armor CND bar was scaled by the HUD movie, put back at scale 1");
						}
					}
				}

				// A measured dash whose readout does not fit looks the same as
				// no dash in game, so it is logged once.
				if (!fits && measured && !saidOffScreen) {
					saidOffScreen = true;
					TraceLog::Line("menu", "Power armor CND would fall outside the HUD, {:.0f} across and {:.0f} deep at {:.0f},{:.0f} in a movie {:.0f} by {:.0f} at {:.0f},{:.0f}",
						length, bar.size, box.x, box.y, frame.width, frame.height, frame.left, frame.top);
				}

				if (wanted && percent != shownPercent) {
					HudParts::Readout::SetPercent(readout, bar.width, bar.deep, percent);
					shownPercent = percent;
					TraceLog::Line("menu", "Power armor CND bar drawn at {:d}%", percent);
				}

				const auto target = wanted ? 1.0 : 0.0;
				const auto step = elapsed / FADE_SECONDS;
				fade = target > fade ? std::min(target, fade + step) : std::max(target, fade - step);

				if (fade != shownOpacity) {
					if ((fade > 0.0) != (shownOpacity > 0.0)) {
						TraceLog::Line("menu", "Power armor CND {:s}, percent {}", fade > 0.0 ? "shows between the dash's ammo digits" : "hides", percent);
					}
					readout.SetMember("alpha"sv, Value(fade));
					readout.SetMember("visible"sv, Value(fade > 0.0));
					shownOpacity = fade;
				}
			}

			void Reset()
			{
				frames = 0;
				shownPercent = NOT_DRAWN;
				shownOpacity = -1.0;
				fade = 0.0;
				laidOutFor = Box{};
				bar = Bar{};
				colored = false;
				saidOffScreen = false;
				saidScaled = false;
			}

		private:
			Clock::time_point lastFrame;
			std::uint32_t     frames = 0;
			std::int32_t      shownPercent = NOT_DRAWN;
			double            shownOpacity = -1.0;
			double            fade = 0.0;
			Box               laidOutFor;
			Bar               bar;
			bool              colored = false;
			bool              saidOffScreen = false;
			bool              saidScaled = false;
		};

		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;

		// -------------------------------------------------------------------
		// Stepping out and building the bar
		// -------------------------------------------------------------------

		// The game lets go of the dash's renderer on this same event, so the
		// trace line names the thread that happens on, see MeasureDash in
		// Dash.cpp.
		class StepOutSink : public RE::BSTEventSink<RE::ExitPowerArmor::Event>
		{
		public:
			F4_HEAP_REDEFINE_NEW(StepOutSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::ExitPowerArmor::Event&, RE::BSTEventSource<RE::ExitPowerArmor::Event>*) override
			{
				TraceLog::Line("menu", "Power armor step out, the game lets go of the dash");
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Adds the readout and its frame listener to the HUD movie.
		void Build(Scaleform::GFx::Movie& a_movie)
		{
			Value root;
			if (!a_movie.GetVariable(&root, "_root") || !root.IsDisplayObject()) {
				REX::WARN("The HUD movie has no _root, so power armor shows no CND.");
				return;
			}

			// One readout, the same kind the ammo counter uses, put straight on
			// the movie's root, since it is placed by the dash and a parent
			// clip's own position would only have to be undone. Nothing is
			// added to the HUD until the whole readout is built.
			auto readout = HudParts::Readout::Create(a_movie, READOUT_NAME);
			if (!readout.IsDisplayObject()) {
				REX::WARN("The HUD movie would not take a power armor CND bar, so power armor shows no CND.");
				return;
			}

			// The bar follows the dash every frame, so its word is drawn where
			// it is rather than snapped to pixels.
			HudParts::Readout::AntiAliasForAnimation(readout);

			if (!root.Invoke("addChild", std::array{ readout })) {
				REX::WARN("The HUD movie would not take the power armor CND bar on its root, so power armor shows no CND.");
				return;
			}

			// A new HUD movie starts with nothing drawn.
			g_frameListener.Reset();

			Value listener;
			a_movie.CreateFunction(&listener, &g_frameListener);
			if (!readout.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
				REX::WARN("The HUD refused the frame listener, so power armor never shows CND.");
				return;
			}

			HudParts::Weapon::Queue();
			QueueAnchor();
			TraceLog::Line("menu", "HUDMenu.swf loaded, power armor CND will follow the dash's ammo box");
		}

		HudParts::Waiter g_waiter{ Settings::bHudCondition, &Build };
	}

	void Load()
	{
		// Once, since the event source lasts as long as the game and a second
		// sink would write every trace line twice.
		static bool registered = false;
		if (registered) {
			return;
		}
		RE::ExitPowerArmor::GetEventSource()->RegisterSink(new StepOutSink());
		registered = true;
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			g_waiter.Watch(a_movie, "the power armor CND bar");
		}
	}
}
