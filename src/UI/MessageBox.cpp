#include "UI/MessageBox.h"

namespace MessageBox
{
	void Ask(const char* a_title, const char* a_body,
		const std::vector<std::string>& a_buttons, RE::IMessageBoxCallback* a_callback)
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
}
