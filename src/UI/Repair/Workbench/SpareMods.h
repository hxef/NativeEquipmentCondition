#pragma once

#include "Core/Plugin.h"

#include "Condition/Materials/Loose.h"
#include "UI/Repair/Workbench/Spares.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// What a spare repair does with the spare's mods, iKeepSpareMods. Private to
// this folder.
//
// The spare's frame, its own scrap recipe, is always scrapped, and so is
// every mod no workbench takes off. Each other mod is either scrapped or
// comes back as the loose mod a mod swap returns, see Materials/Loose.h. A
// plan names the mods that come back, and the spare worked for it, see
// SpareOn, pays for the repair with the rest. Off keeps nothing, so the spare
// repairs and salvages exactly as when it is scrapped whole. The 2 boxes
// that pick the mods are in SpareModBox.h.
namespace Workbench
{
	// The settings of iKeepSpareMods, by their number.
	enum class KeepMods : std::int32_t
	{
		kOff = 0,
		// Scrap the mods the repair needs, the cheapest first.
		kCheapest = 1,
		// The same, the priciest first.
		kPriciest = 2,
		// A box picks the mods to scrap.
		kChooseUse = 3,
		// A box picks the mods to keep.
		kChooseKeep = 4
	};

	// iKeepSpareMods as it stands. Any other number reads as Off.
	[[nodiscard]] KeepMods KeepModsNow();

	// Reads into a_spare the mods of a_extra that come off, in the order the
	// bench lists mods: by value, the cheapest first, then by name and form
	// ID. Then those of a_kept, 1 mod for each form ID. Returns their form
	// IDs, the plan as Materials::Yield takes it.
	std::vector<std::uint32_t> ReadMods(Spare& a_spare, const RE::ExtraDataList* a_extra, std::span<const std::uint32_t> a_kept);

	// The condition a_spare brings a_item to, in tenths, what the repair
	// writes, and full once that reaches FreeFromTenths, which sets a_mended
	// when given. At CND decimals Off the card shows whole percents, so the
	// item lands on the whole steps the spare pays: from 60% with a gain of 10
	// on 70%. Else on the tenth its whole worth reaches on the curve in
	// Repair.h, rounded down and less than 1 step past those: from 60.4% on
	// 70.4% to 71.3%, and 99.9% at most short of full.
	[[nodiscard]] std::uint32_t LandingOf(const Spare& a_spare, const Selection& a_item, const Priced& a_priced, bool* a_mended);

	// What a_worth, paying a_gain whole steps, restores to a_item in tenths,
	// as the box of spares writes it: the landing less the card's number now,
	// +10.4% from 60.4% to 70.8%. At CND decimals Off 10 times a_gain, which
	// leaves a free mend out.
	[[nodiscard]] std::uint32_t RestoredTenths(const Selection& a_item, const Priced& a_priced, std::uint32_t a_worth, std::uint32_t a_gain);

	// The number the 2 boxes of Keep spare mods show for a_spare, in tenths:
	// LandingOf, so the title is the card after the box. At CND decimals Off
	// the tenth the curve says the worth reaches, rounded down, 605 for 60.5%,
	// 1000 at full, while the card gets the whole part. That whole part is
	// always the steps the worth pays, so it never shows 64.0 where the card
	// writes 63.
	[[nodiscard]] std::int32_t ShownTenths(const Spare& a_spare, const Selection& a_item, const Priced& a_priced);

	// The plan of Cheapest and Priciest used first for a_whole, the spare
	// worked with nothing kept: the frame first, then 1 mod at a time, the
	// cheapest or the priciest first, until a_item reaches the condition its
	// card reads after a_whole, a tenth at CND decimals Fractions only and
	// Always. A mod that adds no worth is passed over, so it comes back.
	// Every mod not scrapped is kept. Nothing for the other settings.
	// Says it in the trace.
	[[nodiscard]] std::vector<std::uint32_t> AutoKept(const Selection& a_item, const Priced& a_priced, const Spare& a_whole,
		KeepMods a_setting);

	// The trace's lines for a pick: which mods of a_whole can come off, and
	// the loose mods a_used handed back, nothing for none.
	void TraceSplit(const Spare& a_whole);
	void TraceHandedBack(const Spare& a_used);

	// "Kept Long Ported Barrel, Suppressor and Standard Sights.", the corner
	// line for the mods a spare handed back, the priciest first, or empty for
	// none.
	[[nodiscard]] std::string KeptLine(std::span<const Materials::LooseMod> a_kept);
}
