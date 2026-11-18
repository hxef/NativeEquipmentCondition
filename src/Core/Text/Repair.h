#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// The sentences of the repair question and the bench, its spare box and the
// box of a spare's mods, which Repair.cpp says. Text.h includes this file, so
// a caller includes Text.h.
namespace Text
{
	// "Repair Combat Rifle from 62.4% condition to:", see Condition::CardText.
	[[nodiscard]] std::string RepairQuestion(std::string_view a_name, std::string_view a_condition);

	// "Repair 10mm Pistol to 70%?", the title of the bench's repair box.
	[[nodiscard]] std::string RepairBoxTitle(std::string_view a_name, std::uint32_t a_percent);

	// "Gun Nut 4 reduces the components required for this repair by 50%.", said
	// over the question above when the player holds a rank of the perk pricing
	// the item.
	[[nodiscard]] std::string RepairDiscount(std::string_view a_perk, std::uint32_t a_rank,
		std::uint32_t a_percent);

	// "Condition must be at least 80% to modify.", or, where the floor is at
	// full, "Condition must be full to modify."
	[[nodiscard]] std::string TooDamaged(std::uint32_t a_floor);

	// "MEND", the bench's REPAIR button over an item worn so little that
	// repairing it is free. See Workbench/Bench.h.
	[[nodiscard]] std::string MendButton();

	// "Weapon mended.", or "Armor mended." with a_armor, said when MEND repairs
	// an item on the spot for free.
	[[nodiscard]] std::string Mended(bool a_armor);

	// "USE A SPARE", the top row of the bench's repair box while the player
	// carries a spare, see Workbench/Spares.h.
	[[nodiscard]] std::string SpareButton();

	// "Scrapper 2 raises how much condition a spare restores.", said over the
	// question below while the player's rank raises what a listed spare
	// restores. It names 1 perk rank, Scrapper's or the pricing perk's.
	[[nodiscard]] std::string SparePerk(std::string_view a_perk, std::uint32_t a_rank);

	// "Gun Nut 2 and Scrapper 1 raise how much condition a spare restores.",
	// said in its place while both ranks raise what a listed spare restores.
	[[nodiscard]] std::string SparePerks(std::string_view a_pricing, std::uint32_t a_pricingRank,
		std::string_view a_scrapper, std::uint32_t a_scrapperRank);

	// "Use up a spare to repair Hunting Rifle from 85.4% condition:"
	[[nodiscard]] std::string SpareQuestion(std::string_view a_name, std::string_view a_condition);

	// A spare's 1 mark, the first of equipped, favorite and legendary that
	// applies, a word in brackets, since no language's font for the box has
	// a star or a heart: "(equipped)" for an equipped legendary spare,
	// "(legendary)", or nothing. The column before the spare's name in the
	// spare box.
	[[nodiscard]] std::string SpareMark(bool a_equipped, bool a_favorite, bool a_legendary);

	// "(60.4%)", a spare's own condition as its card writes it, and "+10.4%",
	// what it restores, written the same way, the 2 columns after its name.
	[[nodiscard]] std::string SpareCondition(std::string_view a_condition);
	[[nodiscard]] std::string SpareRestores(std::string_view a_gain);

	// "+2 Damage" or "+3 DMG Resist", what a repair adds to 1 number the item
	// card prints, a column of the 3 repair boxes, or "+2 DMG" and "+3 DR" in
	// the short words. a_type is the damage type as the card numbers it, and
	// an armor piece's number is a resistance. Empty for a type the card has
	// no word for.
	[[nodiscard]] std::string GainByType(std::int64_t a_gain, std::uint32_t a_type, bool a_armor, bool a_short);

	// "Salvaged Steel, Screw (2) and Spring.", the components a spare gave
	// back, each with its count past 1.
	[[nodiscard]] std::string SpareSalvaged(std::span<const std::string> a_parts);

	// "Kept Long Ported Barrel, Suppressor and Standard Sights.", the mods a
	// spare handed back, and with a_more not named "... Suppressor and 2 more.".
	[[nodiscard]] std::string SpareKept(std::span<const std::string> a_mods, std::size_t a_more);

	// "Use up a mod to repair Combat Rifle from 60.5% condition:", the
	// question of the box that picks a spare's mods, a_from "60.5". Once the
	// mods picked fill the item: "Use up a mod or keep the rest, repairing
	// Combat Rifle to 100% condition:"
	[[nodiscard]] std::string SpareModQuestion(std::string_view a_name, std::string_view a_from);
	[[nodiscard]] std::string SpareModFull(std::string_view a_name);

	// "Keep a mod, or use up the rest to repair Combat Rifle to 100%
	// condition:", the question of the box that picks the mods to keep,
	// a_to "100".
	[[nodiscard]] std::string SpareModKeepQuestion(std::string_view a_name, std::string_view a_to);

	// "USE UP THE REST" and "KEEP THE REST", the first row of that box and
	// the one before CANCEL.
	[[nodiscard]] std::string SpareUseUpRest();
	[[nodiscard]] std::string SpareKeepRest();

	// "(30 caps)", a mod's value as a loose mod, in the words of RepairCaps.
	[[nodiscard]] std::string SpareModValue(std::uint32_t a_caps);

	// "+3.7%", "-4.2%", "0.0%" or "64.2%", the number a row of that box ends
	// with, a_number "+3.7", "-4.2", "0.0" and "64.2".
	[[nodiscard]] std::string SpareModGain(std::string_view a_number);

	// "This item can't be repaired at a workbench. Visit a trader to restore
	// it.", said when REPAIR is pressed on an item the bench has nothing to
	// rebuild from, see Workbench/Cost.h. a_trader adds the 2nd sentence.
	[[nodiscard]] std::string BenchCannotRepair(bool a_trader);

	// "Repairing this item from 10.4% needs Gun Nut 4. Find a spare or visit
	// a trader to restore it.", when the repair lock holds REPAIR back, see
	// Workbench/Lock.h. The 2nd sentence names a spare, a trader or both, as
	// a_spare and a_trader say, and is left out with neither.
	[[nodiscard]] std::string RepairLocked(std::string_view a_perk, std::uint32_t a_rank, std::string_view a_condition, bool a_spare, bool a_trader);

	// "This item can't be modified.", said at the bench for an item no mod
	// fits, once a repair brings it to full and whenever its mod slots are
	// asked for. See Workbench/Display.h.
	[[nodiscard]] std::string CannotModify();
}
