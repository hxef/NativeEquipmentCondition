#include "UI/MessageBoxAsked.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/Boxes.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// What NEC does in the message box movie: the row its box starts on, its
// background kept solid under a message box mod, the room its rows have, and
// a guard that lets a press answer its box only once the box is drawn, all by
// 1 frame listener.
namespace MessageBox
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The row NEC's box starts on
		// -------------------------------------------------------------------

		// The list's own number for no row, which its scroll arrows also
		// leave. From there Down highlights the first row and Up nothing.
		constexpr double NO_SELECTION = -1.0;

		// Puts the highlight on a_asked's starting row, or takes it off for
		// NO_ROW, once the movie shows the box with its buttons, which puts
		// the highlight on the first. Tried on that 1 frame only, so a player
		// who comes back to the first row keeps it. A read that fails gives
		// NaN, which matches no row. True once tried.
		[[nodiscard]] bool MoveHighlight(RE::MessageBoxMenu& a_menu, const Asked& a_asked)
		{
			auto       list = Roles::Boxes::MessageList(a_menu);
			const auto rows = Flash::Member(list, "entryList"sv);
			if (!Flash::Bool(a_menu.menuObj, "visible"sv) || !rows.IsArray() || rows.GetArraySize() != a_asked.buttons) {
				return false;
			}
			const auto none = a_asked.start == NO_ROW;
			const auto to = none ? NO_SELECTION : static_cast<double>(a_asked.start);
			constexpr auto UNREAD = std::numeric_limits<double>::quiet_NaN();
			if (Flash::Number(list, "selectedIndex"sv, UNREAD) != 0.0 ||
				!Flash::Set(list, "selectedIndex"sv, Value(to)) ||
				Flash::Number(list, "selectedIndex"sv, UNREAD) != to) {
				TraceLog::Line("menu", "NEC's box {:d} keeps its highlight on its first row", a_asked.number);
				return true;
			}
			if (none) {
				TraceLog::Line("menu", "NEC's box {:d} took its highlight off every row", a_asked.number);
			} else {
				TraceLog::Line("menu", "NEC's box {:d} put its highlight on row {:d}", a_asked.number, a_asked.start);
			}
			return true;
		}

		// -------------------------------------------------------------------
		// Presses before NEC's box is drawn
		// -------------------------------------------------------------------

		// The movie answers its box by calling this function of its code
		// object by name, on a click, on Enter and on a message box mod's own
		// keys. The guard takes that name and keeps the game's function under
		// the second one.
		constexpr auto CODE_OBJECT = "BGSCodeObj"sv;
		constexpr auto PRESS = "onButtonPress"sv;
		constexpr auto GAME_PRESS = "NEC_onButtonPress"sv;

		// Answers NEC's box only once the box has been drawn. No hand can
		// press a box it never saw, so a press before that was meant for the
		// box before it. The message box mod answers Escape as the key is let
		// go, after NEC's own answer to the same key, and that press would
		// land on the box NEC's answer opened. Every other box, and every
		// press on a drawn box, goes to the game's function as before. NEC's
		// own Tab and Escape do not come through here, so they still close a
		// box that is never drawn.
		class PressGuard final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				// The guard lives only in the message box movie, which the
				// menu owns, so the menu is there whenever the movie calls it.
				const auto* ui = RE::UI::GetSingleton();
				const auto  menu = ui ? ui->GetMenu<RE::MessageBoxMenu>() : nullptr;
				if (!menu || menu->uiMovie.get() != a_params.movie) {
					return;
				}
				const auto* message = menu->GetRuntimeData().currentMessage;
				const auto  asked = message ? Find(message->callback.get()) : std::nullopt;
				if (asked && !asked->drawn) {
					const auto* row = a_params.argCount > 0 ? a_params.args : nullptr;
					TraceLog::Line("menu", "NEC's box {:d} drops a press on row {:s} made before the box was drawn", asked->number,
						row && Flash::IsAnyNumber(*row) ? std::format("{:.0f}", Flash::AsNumber(*row)) : std::string{ "(none)" });
					return;
				}
				auto code = Flash::Member(menu->menuObj, CODE_OBJECT);
				if (code.IsObject()) {
					code.Invoke(GAME_PRESS.data(), a_params.retVal, a_params.args, a_params.argCount);
				}
			}
		};

		// Lives as long as the plugin, see Flash.h.
		PressGuard g_pressGuard;

		// The movie that refused the guard, so that is said once. Only the
		// message box's own thread comes here.
		const Scaleform::GFx::Movie* g_unguarded = nullptr;

		// Puts the guard in the game function's place, once per movie.
		void Guard(RE::MessageBoxMenu& a_menu, Scaleform::GFx::Movie& a_movie)
		{
			auto code = Flash::Member(a_menu.menuObj, CODE_OBJECT);
			if (!code.IsObject() || code.HasMember(GAME_PRESS) || &a_movie == g_unguarded) {
				return;
			}
			const auto press = Flash::Member(code, PRESS);
			if (press.IsUndefined()) {
				return;
			}
			Value guard;
			a_movie.CreateFunction(&guard, &g_pressGuard);
			if (!Flash::Set(code, GAME_PRESS, press) || !Flash::Set(code, PRESS, guard)) {
				g_unguarded = &a_movie;
				TraceLog::Line("menu", "The message box movie refused NEC's press guard, so a press made before NEC's box is drawn can answer it");
				return;
			}
			TraceLog::Line("menu", "NEC guards the message box movie's presses, so a press made before NEC's box is drawn answers nothing");
		}

		// -------------------------------------------------------------------
		// Keeping NEC's box solid
		// -------------------------------------------------------------------

		// Below this the message box mod draws a background see-through, in
		// the style's own colour.
		constexpr double SEE_THROUGH = 0.075;

		// How solid NEC's box is drawn, the message box mod's own default.
		constexpr double SOLID = 0.9;

		// A colour with its red, green and blue each at a tenth, rounded down,
		// as the message box mod darkens the fill of a solid box. A number that
		// is no colour counts as black.
		[[nodiscard]] std::int32_t Darkened(double a_color)
		{
			const auto color = std::isfinite(a_color) ? static_cast<std::uint32_t>(std::clamp(a_color, 0.0, 16777215.0)) : 0u;
			const auto red = static_cast<std::uint32_t>(((color >> 16) & 0xFF) * (1.0 - 0.9));
			const auto green = static_cast<std::uint32_t>(((color >> 8) & 0xFF) * (1.0 - 0.9));
			const auto blue = static_cast<std::uint32_t>((color & 0xFF) * (1.0 - 0.9));
			return static_cast<std::int32_t>((red << 16) | (green << 8) | blue);
		}

		// Vanilla's row field, and the 2 px Flash keeps free on each side of
		// a field's text.
		constexpr double VANILLA_ROW_FIELD = 450.0;
		constexpr double GUTTERS = 4.0;

		// A row passes its field when its text is wider than the room by more
		// than SLACK, or when it wraps. NEC's rows are a paragraph, which a
		// field counts as 2 lines while it draws 1.
		constexpr double SLACK = 1.0;
		constexpr double ROW_LINES = 2.0;

		// What RowRoom gives. The frame listener writes it on the message
		// box's thread and a repair box reads it before its box goes up.
		std::atomic<double> g_rowRoom{ VANILLA_ROW_FIELD - GUTTERS };

		// Narrows the room to a row field's when one of a_box's rows passes
		// its field, so the boxes after it take the short words, draw their
		// cells smaller or cut a name to fit. A message box mod that wraps a
		// row measures it by its first line, so a field after a wrap can be
		// narrower than the mod's real limit.
		void Fitted(std::uint64_t a_box, const std::vector<Roles::Boxes::RowField>& a_rows)
		{
			TraceLog::Line("menu", "NEC's box {:d} draws its rows {:.0f} px wide", a_box, a_rows.front().width);
			for (const auto& row : a_rows) {
				const auto room = row.width - GUTTERS;
				if ((row.textWidth > room + SLACK || row.lines > ROW_LINES) && room < g_rowRoom) {
					g_rowRoom = room;
					TraceLog::Line("menu", "NEC's box {:d} has a row of {:.0f} px on {:.0f} lines that its field {:.0f} px wide cannot hold, so rows are laid for {:.0f} px from here",
						a_box, row.textWidth, row.lines, row.width, room);
				}
			}
		}

		// Called by the message box movie every frame. Only NEC's own box is
		// looked at, so while NEC has no box up or waiting, a frame costs a
		// check of an empty list. The press guard joins the movie on the first
		// frame NEC has a box up or waiting. A box with a starting row gets
		// its highlight there first.
		//
		// The message box mod puts a background of its own in the vanilla
		// background's place and styles it as each box shows, a few frames
		// late for the first box of a session, while its settings load. So a
		// see-through background is made solid on whichever frame it is
		// found, as often as it comes back. The next box gets its own style
		// from the message box mod, which sets the background and its colour
		// again for every box.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (!a_params.movie || !AnyAsked()) {
					return;
				}

				// The box on screen is the one the menu shows, and its
				// callback says whose it is. The game frees an answered box and
				// clears or replaces this pointer in one step, on the thread
				// that hands the menus their input and then moves their movies
				// on, so never while this listener runs, and it never points at
				// a freed box.
				const auto* ui = RE::UI::GetSingleton();
				const auto  menu = ui ? ui->GetMenu<RE::MessageBoxMenu>() : nullptr;
				if (!menu || menu->uiMovie.get() != a_params.movie) {
					return;
				}
				Guard(*menu, *a_params.movie);
				if (!menu->GetRuntimeData().currentMessage) {
					return;
				}
				const auto* callback = menu->GetRuntimeData().currentMessage->callback.get();
				const auto  asked = Find(callback);

				// The first frame the box shows on. A hand needs many frames
				// to answer a box it sees, so its press, however quick, comes
				// after this and answers the box.
				if (asked && !asked->drawn && Flash::Opacity(menu->menuObj) > 0.0) {
					Drawn(callback);
				}
				if (asked && asked->start > 0 && MoveHighlight(*menu, *asked)) {
					StartAt(callback, 0, 0);
				}
				const auto number = asked ? std::optional{ asked->number } : std::nullopt;
				if (number && *number != measured && Flash::Opacity(menu->menuObj) > 0.0) {
					if (const auto rows = Roles::Boxes::MessageRows(*menu); !rows.empty()) {
						measured = *number;
						Fitted(*number, rows);
					}
				}
				if (!number || *number == refused) {
					return;
				}

				// The vanilla background has no style, and a style without
				// both numbers is not one this knows.
				auto       background = Roles::Boxes::MessageBackground(*menu);
				auto       style = Flash::Member(background, "style"sv);
				const auto alpha = Flash::Member(style, "fBackgroundAlpha"sv);
				const auto color = Flash::Member(style, "iBackgroundColor"sv);
				if (!Flash::IsAnyNumber(alpha) || !Flash::IsAnyNumber(color)) {
					return;
				}

				// A background solid enough is left as it is. Said once the
				// box shows, since the first box of a session is hidden until
				// the message box mod has styled it.
				const auto was = Flash::AsNumber(alpha);
				if (!(was < SEE_THROUGH)) {
					if (*number != kept && *number != lifted && Flash::Opacity(menu->menuObj) > 0.0) {
						kept = *number;
						TraceLog::Line("menu", "NEC's box {:d} keeps its message box style's background at {:.2f}", *number, was);
					}
					return;
				}

				// The background is read back, so a style that keeps its own
				// number is never darkened twice.
				if (!Flash::Set(style, "iBackgroundColor"sv, Value(Darkened(Flash::AsNumber(color)))) ||
					!Flash::Set(style, "fBackgroundAlpha"sv, Value(SOLID)) ||
					!(Flash::Number(style, "fBackgroundAlpha"sv) >= SEE_THROUGH) ||
					!Flash::Call(background, "redraw")) {
					refused = *number;
					TraceLog::Line("menu", "NEC's box {:d} stays at {:.2f}, its message box style refused a solid background", *number, was);
					return;
				}
				if (*number != lifted) {
					lifted = *number;
					TraceLog::Line("menu", "NEC's box {:d} was see-through at {:.2f} in its message box style, so it is drawn at {:.2f}", *number, was, SOLID);
				}
			}

		private:
			// The last box each line was said for, the last box whose style
			// refused, and the last whose rows were measured, by number. Only
			// the message box's own thread comes here.
			std::uint64_t kept = 0;
			std::uint64_t lifted = 0;
			std::uint64_t refused = 0;
			std::uint64_t measured = 0;
		};

		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;
	}

	double RowRoom()
	{
		return g_rowRoom;
	}

	// -------------------------------------------------------------------
	// Joining the message box movie
	// -------------------------------------------------------------------

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!MenuMovies::IsMovie(a_file, "MessageBoxMenu.swf"sv)) {
			return;
		}

		// A movie loaded again is a new movie with a stage of its own, so
		// each stage gets 1 listener.
		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The message box has no stage to listen on, so a message box mod can draw NEC's repair question see-through.");
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The message box refused the frame listener, so a message box mod can draw NEC's repair question see-through.");
		}
	}
}
