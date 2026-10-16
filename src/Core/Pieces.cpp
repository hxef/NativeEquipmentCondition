#include "Core/Pieces.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>

// What each piece needs, and the part it belongs to. Which place is which
// piece is in Places.cpp.
namespace
{
	using enum Piece;

	constexpr PieceRow PIECES[]{
		{ kNone, Part::kNone, Shown::kHidden },
		{ kBench, Part::kBench, Shown::kPart },
		// The bench's small places do nothing while it does not repair, and
		// each works by itself, see UI/Repair/Workbench/Workbench.cpp.
		{ kBenchDetails, Part::kBench, Shown::kHidden, { kBench }, true },
		// The perk text tells of a discount only the bench gives, see
		// CraftingPerks::SetBench.
		{ kPerksChart, Part::kPerks, Shown::kName, { kBench } },
		{ kPerksPipboy, Part::kPerks, Shown::kName, { kBench } },
		{ kGunWear, Part::kGunWear, Shown::kPart },
		{ kDmgMeleePhysical, Part::kDamage, Shown::kName },
		{ kDmgMeleeEnergy, Part::kDamage, Shown::kName },
		{ kDmgGunPhysical, Part::kDamage, Shown::kName },
		{ kDmgGunEnergy, Part::kDamage, Shown::kName },
		{ kDmgBlastPhysical, Part::kDamage, Shown::kName },
		{ kDmgBlastEnergy, Part::kDamage, Shown::kName },
		{ kDmgBlastObjects, Part::kDamage, Shown::kName },
		{ kDmgHitEffects, Part::kDamage, Shown::kName },
		{ kDmgBlastEffects, Part::kDamage, Shown::kName },
		{ kCardDmgPhysical, Part::kCardDamage, Shown::kName },
		{ kCardDmgEnergy, Part::kCardDamage, Shown::kName },
		// A card's effects read the condition the card pair notes, and its
		// blast the one display notes, see HealthDamage/Card.cpp.
		{ kCardDmgModEffects, Part::kCardDamage, Shown::kName, { kCardDmgEnergy } },
		{ kCardDmgOwnEffects, Part::kCardDamage, Shown::kName, { kCardDmgEnergy } },
		{ kCardDmgBlast, Part::kCardDamage, Shown::kName, { kCardDmgPhysical } },
		{ kArmorDr, Part::kArmor, Shown::kName },
		{ kArmorEnergyRad, Part::kArmor, Shown::kName },
		{ kArmorBestMark, Part::kArmor, Shown::kName },
		// The 3 AI calls each scale only the rating they read, see
		// ArmorRating.cpp.
		{ kArmorNpcRanking, Part::kArmor, Shown::kName, {}, true },
		{ kCardArmorMenus, Part::kCardArmor, Shown::kName },
		{ kCardArmorCompare, Part::kCardArmor, Shown::kName },
		{ kCardArmorPipboy, Part::kCardArmor, Shown::kName },
		{ kCardArmorDoll, Part::kCardArmor, Shown::kName },
		{ kBrokenTakeOff, Part::kBroken, Shown::kName },
		// The mark put back sets is read only by the equip pair's hooks.
		{ kBrokenPutBack, Part::kBroken, Shown::kName, { kBrokenTakeOff } },
		{ kPrices, Part::kPrices, Shown::kPart },
		{ kFireRate, Part::kFireRate, Shown::kPart },
		// An NPC's gun and the firing sound read the shot that Gun wear from
		// firing's fire call notes.
		{ kFireRateNpcGuns, Part::kFireRate, Shown::kHidden, { kFireRate, kGunWear } },
		{ kFireSound, Part::kFireSound, Shown::kPart, { kFireRate, kGunWear } },
		{ kCritMeter, Part::kCritMeter, Shown::kPart },
		{ kNpcCrits, Part::kNpcCrits, Shown::kPart },
		// A shot's spread reads the condition Gun wear from firing's fire
		// call notes, see Gameplay/Aim/Aim.h. The crosshair shows the spread,
		// so it goes with it. VATS never spreads a shot, so it works alone.
		{ kAccuracy, Part::kAccuracy, Shown::kPart, { kGunWear } },
		{ kCrosshair, Part::kCrosshair, Shown::kPart, { kAccuracy } },
		{ kVatsAccuracy, Part::kVatsAccuracy, Shown::kPart },
		{ kRange, Part::kRange, Shown::kPart },
		// Every shot's jam comes through Gun wear from firing's fire call.
		{ kJamShot, Part::kJam, Shown::kPart, { kGunWear } },
		{ kJamReload, Part::kReloadJam, Shown::kPart, { kJamShot } },
		{ kWornLoot, Part::kSpawn, Shown::kPart },
		{ kConsoleNew, Part::kConsoleNew, Shown::kPart, { kWornLoot } },
		{ kWorldLoot, Part::kWorldLoot, Shown::kPart, { kWornLoot } },
		{ kModlessLoot, Part::kModlessLoot, Shown::kPart, { kWornLoot } },
		{ kGifts, Part::kGifts, Shown::kPart, { kWornLoot } },
		{ kCndContainers, Part::kCardCnd, Shown::kName },
		{ kCndWorkbench, Part::kCardCnd, Shown::kName },
		{ kCndCooking, Part::kCardCnd, Shown::kName },
		// The Pip-Boy builds a card at 2 places, and NEC adds its rows at both
		// or neither, see UI/Inventory/ItemCard/Hooks.cpp.
		{ kCndPipboy, Part::kCardCnd, Shown::kName },
		// The 3 card rate places go in together. A card's rate reads the
		// condition noted where the same menu builds its CND row, so each of
		// these card rates is a piece of its own that needs both.
		{ kRateCards, Part::kCardRate, Shown::kHidden, { kFireRate } },
		{ kRateCardsContainers, Part::kCardRate, Shown::kName, { kRateCards, kCndContainers } },
		{ kRateCardsWorkbench, Part::kCardRate, Shown::kName, { kRateCards, kCndWorkbench } },
		{ kRateCardsCooking, Part::kCardRate, Shown::kName, { kRateCards, kCndCooking } },
		{ kRateCardsPipboy, Part::kCardRate, Shown::kName, { kRateCards, kCndPipboy } },
		{ kRateBetter, Part::kCardRate, Shown::kName, { kFireRate } },
		// The sort's note of each stack's condition. The sort's rate and
		// accuracy read it, so it goes in while either goes in, see
		// ItemCard/Rate.cpp.
		{ kSortNote, Part::kSort, Shown::kPart },
		{ kRateSort, Part::kCardRate, Shown::kName, { kFireRate, kSortNote } },
		// The card accuracy and range places, like the card rates, each read
		// the condition noted where the same menu builds its CND row.
		{ kAccuracyCards, Part::kCardAccuracy, Shown::kHidden, { kAccuracy } },
		{ kAccuracyCardsContainers, Part::kCardAccuracy, Shown::kName, { kAccuracyCards, kCndContainers } },
		{ kAccuracyCardsWorkbench, Part::kCardAccuracy, Shown::kName, { kAccuracyCards, kCndWorkbench } },
		{ kAccuracyCardsCooking, Part::kCardAccuracy, Shown::kName, { kAccuracyCards, kCndCooking } },
		{ kAccuracyCardsPipboy, Part::kCardAccuracy, Shown::kName, { kAccuracyCards, kCndPipboy } },
		{ kAccuracySort, Part::kCardAccuracy, Shown::kName, { kAccuracy, kSortNote } },
		{ kRangeCards, Part::kCardRange, Shown::kHidden, { kRange } },
		{ kRangeCardsContainers, Part::kCardRange, Shown::kName, { kRangeCards, kCndContainers } },
		{ kRangeCardsWorkbench, Part::kCardRange, Shown::kName, { kRangeCards, kCndWorkbench } },
		{ kRangeCardsCooking, Part::kCardRange, Shown::kName, { kRangeCards, kCndCooking } },
		{ kRangeCardsPipboy, Part::kCardRange, Shown::kName, { kRangeCards, kCndPipboy } },
		{ kHudBars, Part::kHudBar, Shown::kPart },
		{ kLootMeters, Part::kQuick, Shown::kPart },
		// The list hooks ask whether the bench repairs, see
		// UI/Repair/Workbench/Lists.cpp. The item list's hook marks the
		// rebuild its refresh call changes, so those 2 places are 1 piece.
		// The mod slots and the mod choices each work by themselves.
		{ kBenchItemList, Part::kBenchLists, Shown::kName, { kBench } },
		{ kBenchModLists, Part::kBenchLists, Shown::kName, { kBench }, true },
		{ kConfirmScroll, Part::kScroll, Shown::kPart },
		{ kTraderRepairs, Part::kTraderRepairs, Shown::kPart },
		// A trader's stock is rolled in their band only while both switches
		// are on, see VendorRepair/Upkeep.cpp.
		{ kStockMerchant, Part::kStock, Shown::kName, { kTraderRepairs, kWornLoot } },
		{ kStockLinked, Part::kStock, Shown::kName, { kTraderRepairs, kWornLoot } },
		{ kInspectPrice, Part::kInspect, Shown::kPart },
		{ kSrm, Part::kSrm, Shown::kPart },
		{ kTips, Part::kTips, Shown::kPart },
		{ kMeleeWear, Part::kNone, Shown::kHidden },
	};

	// 1 row a piece, in the enum's order.
	consteval bool InOrder()
	{
		for (std::size_t i = 0; i < std::size(PIECES); i++) {
			if (static_cast<std::size_t>(PIECES[i].piece) != i) {
				return false;
			}
		}
		return std::size(PIECES) == PIECE_COUNT;
	}
	static_assert(InOrder());

	// A piece needs only pieces above it, so 1 pass from the top settles every
	// piece, and kNone only fills the end of its list.
	consteval bool NeedsAbove()
	{
		for (const auto& row : PIECES) {
			bool ended = false;
			for (const auto need : row.needs) {
				if (need == kNone) {
					ended = true;
				} else if (ended || need >= row.piece) {
					return false;
				}
			}
		}
		return true;
	}
	static_assert(NeedsAbove());

	// Every part has 1 piece shown by the part's name, or 2 or more with
	// names of their own. Only kNone and kMeleeWear belong to no part.
	consteval bool PartsShown()
	{
		for (auto i = static_cast<std::size_t>(Part::kPerks); i < static_cast<std::size_t>(Part::kTrace); i++) {
			std::size_t named = 0;
			std::size_t whole = 0;
			for (const auto& row : PIECES) {
				if (static_cast<std::size_t>(row.part) == i) {
					named += row.shown == Shown::kName ? 1 : 0;
					whole += row.shown == Shown::kPart ? 1 : 0;
				}
			}
			if (!(whole == 1 && named == 0) && !(whole == 0 && named >= 2)) {
				return false;
			}
		}
		return std::ranges::all_of(PIECES, [](const PieceRow& a_row) {
			return (a_row.part == Part::kNone) == (a_row.piece == kNone || a_row.piece == kMeleeWear) && a_row.part < Part::kTrace;
		});
	}
	static_assert(PartsShown());
}

std::span<const PieceRow> PieceRows()
{
	return PIECES;
}

bool Whole(Part a_part, std::span<const Piece> a_pieces)
{
	return std::ranges::all_of(PIECES, [&](const PieceRow& a_row) {
		return !ShownIn(a_row.piece, a_part) || std::ranges::find(a_pieces, a_row.piece) != a_pieces.end();
	});
}
