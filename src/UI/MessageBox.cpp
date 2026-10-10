#include "UI/MessageBox.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/Boxes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <optional>

namespace MessageBox
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// NEC's boxes
		// -------------------------------------------------------------------

		// A callback NEC handed the game, and the number its box goes by in
		// the trace log, from 1.
		struct Asked
		{
			const RE::IMessageBoxCallback* callback = nullptr;
			std::uint64_t                  number = 0;
		};

		// Every callback of NEC's that still lives. They are compared, never
		// read through. The game frees one once its box is done, on whichever
		// thread that is, so a lock guards the list.
		std::mutex          g_askedLock;
		std::vector<Asked>  g_asked;
		std::uint64_t       g_lastNumber = 0;

		[[nodiscard]] bool AnyAsked()
		{
			const std::scoped_lock l{ g_askedLock };
			return !g_asked.empty();
		}

		// The number of the box a_callback belongs to, or nothing for the
		// game's own box or another mod's.
		[[nodiscard]] std::optional<std::uint64_t> NumberOf(const RE::IMessageBoxCallback* a_callback)
		{
			const std::scoped_lock l{ g_askedLock };
			for (const auto& asked : g_asked) {
				if (asked.callback == a_callback) {
					return asked.number;
				}
			}
			return std::nullopt;
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

		// Called by the message box movie every frame. Only NEC's own box is
		// looked at, so while NEC has no box up or waiting, a frame costs a
		// check of an empty list.
		//
		// The message box mod puts a background of its own in the game's
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
				// clears or replaces this pointer in one step, under the lock this
				// listener runs under, so it never points at a freed box.
				const auto* ui = RE::UI::GetSingleton();
				const auto  menu = ui ? ui->GetMenu<RE::MessageBoxMenu>() : nullptr;
				if (!menu || menu->uiMovie.get() != a_params.movie || !menu->currentMessage) {
					return;
				}
				const auto number = NumberOf(menu->currentMessage->callback.get());
				if (!number || *number == refused) {
					return;
				}

				// The game's own background has no style, and a style without
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
			// The last box each line was said for, and the last box whose style
			// refused, by number. Only the message box's own thread comes here.
			std::uint64_t kept = 0;
			std::uint64_t lifted = 0;
			std::uint64_t refused = 0;
		};

		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;
	}

	// -------------------------------------------------------------------
	// Callbacks
	// -------------------------------------------------------------------

	Callback::Callback()
	{
		const std::scoped_lock l{ g_askedLock };
		g_asked.push_back({ this, ++g_lastNumber });
	}

	Callback::~Callback()
	{
		const std::scoped_lock l{ g_askedLock };
		std::erase_if(g_asked, [this](const Asked& a_asked) { return a_asked.callback == this; });
	}

	// -------------------------------------------------------------------
	// Asking
	// -------------------------------------------------------------------

	void Ask(const char* a_title, const char* a_body,
		const std::vector<std::string>& a_buttons, Callback* a_callback)
	{
		if (a_buttons.empty()) {
			delete a_callback;
			return;
		}

		// A box that never opens returns the cancel button, so whatever waits
		// on the answer can carry on.
		auto* messages = RE::MessageMenuManager::GetSingleton();
		if (!messages) {
			(*a_callback)(static_cast<std::uint8_t>(a_buttons.size() - 1));
			delete a_callback;
			return;
		}

		RE::BSScrapArray<RE::BSString> buttons;
		for (const auto& button : a_buttons) {
			buttons.emplace_back().Set(button.c_str(), 0);
		}

		// The Cancel key presses the last button. Most of the game's own boxes
		// give it no button, -1.
		const auto wayOut = static_cast<std::int32_t>(a_buttons.size() - 1);

		// MessageMenuManager::Create takes 4 labels and is the only source of
		// the 4 button limit. QueueMessage counts nothing. The box is modal and
		// shows even when the box before it said the same.
		messages->QueueMessage(a_title, a_body, a_callback, RE::WARNING_TYPES::kDefault, &buttons,
			true, false, wayOut);
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
