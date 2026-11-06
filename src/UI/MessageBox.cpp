#include "UI/MessageBox.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/TraceLog.h"
#include "UI/MessageBoxAsked.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace MessageBox
{
	namespace
	{
		// -------------------------------------------------------------------
		// NEC's boxes
		// -------------------------------------------------------------------

		// Every callback of NEC's that still lives. They are compared, never
		// read through. The game frees one once its box is done, on whichever
		// thread that is, so a lock guards the list.
		std::mutex          g_askedLock;
		std::vector<Asked>  g_asked;
		std::uint64_t       g_lastNumber = 0;
	}

	[[nodiscard]] bool AnyAsked()
	{
		const std::scoped_lock l{ g_askedLock };
		return !g_asked.empty();
	}

	// The box a_callback belongs to, or nothing for a vanilla box or
	// another mod's.
	[[nodiscard]] std::optional<Asked> Find(const RE::IMessageBoxCallback* a_callback)
	{
		const std::scoped_lock l{ g_askedLock };
		for (const auto& asked : g_asked) {
			if (asked.callback == a_callback) {
				return asked;
			}
		}
		return std::nullopt;
	}

	// Sets the row a_callback's box starts on, 0 once it is there.
	void StartAt(const RE::IMessageBoxCallback* a_callback, std::uint32_t a_start, std::uint32_t a_buttons)
	{
		const std::scoped_lock l{ g_askedLock };
		for (auto& asked : g_asked) {
			if (asked.callback == a_callback) {
				asked.start = a_start;
				asked.buttons = a_buttons;
			}
		}
	}

	namespace
	{
		// Notes that a_callback's box cancels by button a_last.
		void CancelsBy(const RE::IMessageBoxCallback* a_callback, std::uint8_t a_last)
		{
			const std::scoped_lock l{ g_askedLock };
			for (auto& asked : g_asked) {
				if (asked.callback == a_callback) {
					asked.last = a_last;
				}
			}
		}

		// NEC's box a_menu shows, or nothing for a vanilla box, another mod's
		// or none.
		[[nodiscard]] std::optional<Asked> Shown(const RE::MessageBoxMenu& a_menu)
		{
			const auto* message = a_menu.GetRuntimeData().currentMessage;
			return message ? Find(message->callback.get()) : std::nullopt;
		}

		// -------------------------------------------------------------------
		// Tab and Escape
		// -------------------------------------------------------------------

		// What the game does before NEC: the message box's answer to a key let
		// go, which is none, and whether the pause key's handler takes an
		// input. That handler takes Escape before any menu hears it and only
		// stops it while a message box is up.
		REL::Relocation<bool (*)(RE::IMenu*, const RE::BSFixedString&)>         _OnButtonEventRelease;
		REL::Relocation<bool (*)(RE::BSInputEventUser*, const RE::InputEvent*)> _PauseShouldHandleEvent;

		// Whether each place runs, see CallPatch::PatchSlot. The pause hook
		// asks the keys place too, so Escape stays the handler's while no hook
		// presses for it.
		CallPatch::LinkBase g_keysLink;
		CallPatch::LinkBase g_pauseLink;

		// What menu mode calls the Cancel key, Tab or B, and Escape, which
		// keeps its name from play.
		constexpr auto CANCEL_KEY = "Cancel"sv;
		constexpr auto PAUSE_KEY = "Pause"sv;

		// On NEC's box the Cancel key and Escape press its last button as they
		// are let go, as a click does, and go no further. So on AE the box's
		// own Cancel press, which the box was handed the same button for,
		// never answers it twice. On OG it has no such press. Every other key
		// and every other box go as the game has them.
		bool OnButtonEventReleaseHk(RE::IMenu* a_this, const RE::BSFixedString& a_key)
		{
			if (g_keysLink.Live() && (a_key == CANCEL_KEY || a_key == PAUSE_KEY)) {
				auto* menu = static_cast<RE::MessageBoxMenu*>(a_this);
				if (const auto asked = Shown(*menu)) {
					TraceLog::Line("menu", "NEC's box {:d} heard {:s} let go and presses its last button", asked->number, a_key.c_str());
					menu->OnButtonPress(asked->last);
					return true;
				}
			}
			return _OnButtonEventRelease(a_this, a_key);
		}

		// Escape passes the pause key's handler while NEC's box is on screen,
		// so the box hears it.
		bool PauseShouldHandleEventHk(RE::BSInputEventUser* a_this, const RE::InputEvent* a_event)
		{
			const auto* button = a_event->As<RE::ButtonEvent>();
			if (g_pauseLink.Live() && g_keysLink.Live() && button && button->QUserEvent() == PAUSE_KEY && AnyAsked()) {
				const auto* ui = RE::UI::GetSingleton();
				const auto  menu = ui ? ui->GetMenu<RE::MessageBoxMenu>() : nullptr;
				if (menu && Shown(*menu)) {
					return false;
				}
			}
			return _PauseShouldHandleEvent(a_this, a_event);
		}
	}

	// -------------------------------------------------------------------
	// Install
	// -------------------------------------------------------------------

	void Install()
	{
		const auto keys = CallPatch::PatchSlot(RE::MessageBoxMenu::VTABLE[0], { 0x0F, 0x0F }, OnButtonEventReleaseHk, "box keys",
			Part::kNone, true, &g_keysLink);
		const auto pause = CallPatch::PatchSlot(RE::VTABLE::MenuOpenHandler[0], { 0x01, 0x01 }, PauseShouldHandleEventHk, "box pause key",
			Part::kNone, true, &g_pauseLink);
		_OnButtonEventRelease = keys.value_or(0);
		_PauseShouldHandleEvent = pause.value_or(0);
		if (keys && pause) {
			REX::INFO("Tab and Escape close NEC's repair boxes as their CANCEL button does.");
		} else {
			REX::WARN("Tab and Escape may leave NEC's repair boxes open, so close them with their CANCEL button.");
		}
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
		const std::vector<std::string>& a_buttons, Callback* a_callback, std::uint32_t a_start)
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

		// The Cancel key presses the last button. Most vanilla boxes
		// give it no button, -1.
		const auto wayOut = static_cast<std::int32_t>(a_buttons.size() - 1);
		CancelsBy(a_callback, static_cast<std::uint8_t>(wayOut));
		if (a_start == NO_ROW || (a_start > 0 && a_start < a_buttons.size())) {
			StartAt(a_callback, a_start, static_cast<std::uint32_t>(a_buttons.size()));
		}

		// MessageMenuManager::Create takes 4 labels and is the only source of
		// the 4 button limit. QueueMessage counts nothing. The box is modal and
		// shows even when the box before it said the same.
		messages->QueueMessage(a_title, a_body, a_callback, RE::WARNING_TYPES::kDefault, &buttons,
			true, false, wayOut);
	}
}
