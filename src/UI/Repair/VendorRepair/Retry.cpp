#include "UI/Repair/VendorRepair/Retry.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/Button.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Repair/VendorRepair/VendorRepair.h"
#include "UI/Roles/Roles.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace VendorRepair
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Clock = std::chrono::steady_clock;

		// -------------------------------------------------------------------
		// The wait
		// -------------------------------------------------------------------

		// How long a screen waits for a bar, from its first miss, long enough
		// for a UI replacer that lays its bars out late.
		constexpr auto WAIT = std::chrono::seconds(2);

		// How long a screen is watched, from REPAIR first joining a bar. A UI
		// replacer that reads its settings from files as the screen opens
		// hands its bar a fresh list a few frames later. 3 s leaves room for a
		// slow first read of the session, 1 s longer than WAIT.
		constexpr auto WATCH = std::chrono::seconds(3);

		// What NEC.log or the trace log says the player sees once a wait runs
		// out, see GiveUp. It never shows on screen.
		constexpr auto NO_BAR = "REPAIR shows on no bar, since this movie shows no button hints or draws them in a bar NEC does not know. C still repairs"sv;

		// Where the frame listener is kept on menuObj, the barter screen's
		// dynamic class, so a movie gets 1 however many screens it shows.
		constexpr std::string_view LISTENER_MEMBER = "NECRepairWait"sv;

		// The movie waiting and when it stops, and a movie given up on until
		// the next screen opens. The movies are compared, never read through,
		// since the screen can close inside the wait. Only the barter screen's
		// thread comes here.
		const Scaleform::GFx::Movie* g_movie = nullptr;
		std::optional<Clock::time_point> g_deadline;
		const Scaleform::GFx::Movie* g_gaveUp = nullptr;
		std::uint32_t                g_frames = 0;

		// The movie watched and when the watch stops, kept once it stops so a
		// screen is watched once. Compared the same way.
		const Scaleform::GFx::Movie* g_watched = nullptr;
		std::optional<Clock::time_point> g_watchEnd;
		std::uint32_t g_watchFrames = 0;

		// Ends the wait on a screen that never showed a bar REPAIR can join.
		// A screen hidden by its own movie all that while only gets a trace
		// line, since the player never saw a bar missing.
		void GiveUp(RE::BarterMenu& a_menu)
		{
			g_deadline.reset();
			g_gaveUp = g_movie;

			Value root;
			const bool shown = a_menu.menuObj.IsObject() && a_menu.menuObj.GetMember("root"sv, &root) &&
			                   Flash::Opacity(root) > 0.0;
			if (shown) {
				Roles::Missing(*a_menu.uiMovie, "button bar"sv, NO_BAR);
			} else {
				Roles::Noted(*a_menu.uiMovie, "button bar"sv, NO_BAR);
			}
		}

		// -------------------------------------------------------------------
		// The frame listener
		// -------------------------------------------------------------------

		// Runs the whole refresh each frame of a wait or a watch on its
		// movie, and returns at once on any other frame. It stays on the
		// movie after, kept in LISTENER_MEMBER, so later waits and watches on
		// that movie use it again.
		class Listener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const bool waiting = g_deadline && a_params.movie && a_params.movie == g_movie;
				const bool watching = g_watchEnd && a_params.movie && a_params.movie == g_watched;
				if (!waiting && !watching) {
					return;
				}
				auto* const menu = OpenBarter();
				if (!menu || menu->uiMovie.get() != a_params.movie || !Settings::bVendorRepair.GetValue() ||
					!HighlightLive()) {
					EndWait(false);
					g_watchEnd.reset();
					return;
				}
				g_frames++;
				g_watchFrames++;
				Refresh(menu);
				const auto now = Clock::now();
				if (g_deadline && g_movie == a_params.movie && now >= *g_deadline) {
					GiveUp(*menu);
				}
				if (g_watchEnd && g_watched == a_params.movie && now >= *g_watchEnd) {
					g_watchEnd.reset();
					TraceLog::Line("menu", "{:s}'s barter screen stops watching its bar after {:d} frames",
						Trader(menu), g_watchFrames);
				}
			}
		};

		// Lives as long as the plugin, see Flash.h.
		Listener g_listener;

		// Adds the listener to a_menu's movie unless it has one. False when
		// the movie refuses it.
		[[nodiscard]] bool Listen(RE::BarterMenu& a_menu)
		{
			Value kept;
			if (a_menu.menuObj.IsObject() && a_menu.menuObj.GetMember(LISTENER_MEMBER, &kept) && !kept.IsUndefined()) {
				return true;
			}
			Value listener;
			a_menu.uiMovie->CreateFunction(&listener, &g_listener);
			const std::array args{ Value("enterFrame"), listener };
			return Flash::Call(a_menu.menuObj, "addEventListener", args) &&
			       Flash::Set(a_menu.menuObj, LISTENER_MEMBER, listener);
		}
	}

	// -------------------------------------------------------------------
	// Starting and ending a wait
	// -------------------------------------------------------------------

	void WaitForBar(RE::BarterMenu& a_menu)
	{
		const auto* movie = a_menu.uiMovie.get();
		if (!movie || movie == g_gaveUp || (g_deadline && movie == g_movie)) {
			return;
		}

		g_movie = movie;
		g_frames = 0;
		if (!Listen(a_menu)) {
			TraceLog::Line("menu", "{:s}'s barter screen refused a frame listener, so REPAIR cannot wait for a bar",
				Trader(&a_menu));
			GiveUp(a_menu);
			return;
		}
		g_deadline = Clock::now() + WAIT;
		TraceLog::Line("menu", "{:s}'s barter screen has no bar REPAIR can join yet, so it looks again each frame for {:d} s",
			Trader(&a_menu), WAIT.count());
	}

	void EndWait(bool a_found)
	{
		if (!g_deadline) {
			return;
		}
		g_deadline.reset();
		if (a_found) {
			TraceLog::Line("menu", "REPAIR joined a bar {:d} frames into its wait", g_frames);
		}
	}

	// -------------------------------------------------------------------
	// Watching a bar REPAIR joined
	// -------------------------------------------------------------------

	void WatchBar(RE::BarterMenu& a_menu)
	{
		const auto* movie = a_menu.uiMovie.get();
		if (!movie || movie == g_watched) {
			return;
		}

		g_watched = movie;
		g_watchFrames = 0;
		if (!Listen(a_menu)) {
			TraceLog::Line("menu", "{:s}'s barter screen refused a frame listener, so REPAIR cannot watch its bar",
				Trader(&a_menu));
			return;
		}
		g_watchEnd = Clock::now() + WATCH;
		TraceLog::Line("menu", "{:s}'s barter screen watches its bar each frame for {:d} s, in case it gets a fresh list",
			Trader(&a_menu), WATCH.count());
	}

	void ForgetWait()
	{
		g_movie = nullptr;
		g_deadline.reset();
		g_gaveUp = nullptr;
		g_watched = nullptr;
		g_watchEnd.reset();
	}
}
