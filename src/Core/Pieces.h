#pragma once

#include "Core/Parts.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// The pieces of every part: the smallest thing a player could notice being
// off by itself. Places nobody could tell apart in play are 1 piece, and a
// place can be 1 piece, 2 or none. The MCM page and NEC.log name each piece
// that is off, "CND on item cards (containers and traders): off, left to
// A.dll." A piece is off when a place of it is off, or when a piece it needs
// is off. PLACES in Pieces.cpp says which place is which piece, and the names
// live in Core/Text.
enum class Piece : std::uint8_t
{
	kNone,
	// Workbench repairs, first since the perk text needs it
	kBench,
	// Perk repair discount text
	kPerksChart,
	kPerksPipboy,
	// Gun wear from firing
	kGunWear,
	// Worn weapon damage
	kDmgMeleePhysical,
	kDmgMeleeEnergy,
	kDmgGunPhysical,
	kDmgGunEnergy,
	kDmgBlastPhysical,
	kDmgBlastEnergy,
	kDmgBlastObjects,
	kDmgHitEffects,
	kDmgBlastEffects,
	// Damage shown in menus
	kCardDmgPhysical,
	kCardDmgEnergy,
	kCardDmgModEffects,
	kCardDmgOwnEffects,
	kCardDmgBlast,
	// Worn armor protection
	kArmorDr,
	kArmorEnergyRad,
	kArmorBestMark,
	kArmorNpcRanking,
	// Protection shown in menus
	kCardArmorMenus,
	kCardArmorCompare,
	kCardArmorPipboy,
	kCardArmorDoll,
	// Broken gear rules
	kBrokenTakeOff,
	kBrokenPutBack,
	// Worn item prices
	kPrices,
	// Slower fire when worn
	kFireRate,
	kFireRateNpcGuns,
	// Slower firing sound
	kFireSound,
	// Fewer criticals when worn, Fewer NPC criticals
	kCritMeter,
	kNpcCrits,
	// Jamming, Single shot gun jams
	kJamShot,
	kJamReload,
	// Worn loot and its 3 exceptions
	kWornLoot,
	kConsoleNew,
	kOldSaves,
	kGifts,
	// CND on item cards
	kCndContainers,
	kCndWorkbench,
	kCndCooking,
	kCndPipboyLists,
	kCndPipboyUpdates,
	// Fire rate shown in menus
	kRateCards,
	kRateCardsContainers,
	kRateCardsWorkbench,
	kRateCardsCooking,
	kRateCardsPipboyLists,
	kRateCardsPipboyUpdates,
	kRateBetter,
	kRateSort,
	// The rest of the parts from HUD condition bars to Loading screen tips,
	// each 1 piece but Worn items in workbench lists and Trader stock
	// condition, which have 2 each
	kHudBars,
	kLootMeters,
	kBenchItemList,
	kBenchModLists,
	kConfirmScroll,
	kTraderRepairs,
	kStockMerchant,
	kStockLinked,
	kInspectPrice,
	kSrm,
	kTips,
	// Wear from melee hits and bashes, which no mod can take
	kMeleeWear,
	kTotal,
};

inline constexpr std::size_t PIECE_COUNT = static_cast<std::size_t>(Piece::kTotal);

// How the page names a piece.
enum class Shown : std::uint8_t
{
	kName,    // by its own name, after its part's: "CND on item cards (containers and traders)"
	kPart,    // by its part's name, as the only piece of the part a player sees
	kHidden,  // never in a part's line, only in a setting's sentence or as what another piece needs
};

// One piece, the part it belongs to and the pieces it needs. A part whose
// pieces have names has 2 or more of them, any other part has exactly 1
// piece shown by the part's name. alone marks a piece whose places each work
// by themselves, so NEC's change at one still works while another mod has
// another.
struct PieceRow
{
	Piece                piece;
	Part                 part;
	Shown                shown;
	std::array<Piece, 3> needs{};  // kNone fills the rest. The first one that is off is named as the cause
	bool                 alone = false;
};

// Every piece, in the enum's order, so a piece's row is found by its place.
[[nodiscard]] std::span<const PieceRow> PieceRows();

[[nodiscard]] inline const PieceRow& RowOfPiece(Piece a_piece)
{
	return PieceRows()[static_cast<std::size_t>(a_piece)];
}

// The pieces a place is now, by the name the place is patched with, "card
// fire rate". None for a place that only feeds the trace logs, and none
// for a place whose loss alone changes nothing a player sees, see PLACES in
// Pieces.cpp.
[[nodiscard]] std::span<const Piece> PiecesAt(std::string_view a_what);

// Whether a_piece is a piece of a_part the page names.
[[nodiscard]] inline bool ShownIn(Piece a_piece, Part a_part)
{
	const auto& row = RowOfPiece(a_piece);
	return row.part == a_part && row.shown != Shown::kHidden;
}

// Whether a_pieces hold every piece of a_part the page names, so the part's
// own name says it.
[[nodiscard]] bool Whole(Part a_part, std::span<const Piece> a_pieces);
