#pragma once

#include "Core/Plugin.h"

// A worn item is worth less, everywhere the game shows a price. The engine
// reads the condition when it prices an item and raises anything below full
// back to full an instruction later, so a broken item sold for the price of a
// new one. Install puts the condition back into the formula that value goes to,
// which covers the Pip-Boy, the item card, vendors and containers. The curve is
// Fallout 3's, condition to the power of 1.5: half condition gets 35% of the
// price.
namespace ItemValue
{
	// Patches the pair of calls every price in the game comes through.
	void Install();

	// Whether that pair is in, so a worn item is worth less.
	[[nodiscard]] bool Works();

	// Prices items as sound, meaning without wear and without the barter
	// markup, while one of these exists on this thread. Only a repair priced in
	// caps needs it: a broken item priced against its own broken value would
	// cost almost nothing to fix, and a repair bought in steps would cost more
	// than one bought in one go. See VendorRepair.h. Dividing the wear back out
	// afterwards fails at the broken end, where the engine rounds a price up to
	// 3 caps and dividing by 0.01 gives 300. A barter screen's Charisma markup
	// is in the same formula and depends on what the screen priced last, so it
	// is left out too.
	class ScopedSoundPrice
	{
	public:
		ScopedSoundPrice();
		~ScopedSoundPrice();

		ScopedSoundPrice(const ScopedSoundPrice&) = delete;
		ScopedSoundPrice(ScopedSoundPrice&&) = delete;
		ScopedSoundPrice& operator=(const ScopedSoundPrice&) = delete;
		ScopedSoundPrice& operator=(ScopedSoundPrice&&) = delete;

	private:
		bool _was;
	};
}
