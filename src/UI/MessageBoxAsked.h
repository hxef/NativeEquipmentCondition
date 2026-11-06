#pragma once

#include "UI/MessageBox.h"

#include <cstdint>
#include <optional>

// The list of NEC's boxes, which MessageBox.cpp keeps and MessageBoxMovie.cpp
// reads. Private to those 2 files.
namespace MessageBox
{
	// A callback NEC handed the game, and the number its box goes by in
	// the trace log, from 1.
	struct Asked
	{
		const RE::IMessageBoxCallback* callback = nullptr;
		std::uint64_t                  number = 0;
		// The row the highlight is still to be put on, NO_ROW to take it
		// off, and how many buttons the box has, 0 once it is done or for
		// none.
		std::uint32_t start = 0;
		std::uint32_t buttons = 0;
		// The number of its last button, its cancel.
		std::uint8_t last = 0;
	};

	// Whether NEC has a box up or waiting, the box a_callback belongs to, and
	// setting the row it starts on, in MessageBox.cpp.
	[[nodiscard]] bool                 AnyAsked();
	[[nodiscard]] std::optional<Asked> Find(const RE::IMessageBoxCallback* a_callback);
	void                               StartAt(const RE::IMessageBoxCallback* a_callback, std::uint32_t a_start, std::uint32_t a_buttons);
}
