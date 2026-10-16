#include "Core/Pieces.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

// Which pieces each place is. A place that only feeds the trace logs has no
// row. A new place gets a row here, or its part's line never names it.
namespace
{
	using enum Piece;

	// -------------------------------------------------------------------
	// Every place
	// -------------------------------------------------------------------

	// When a place's pieces count as off with it.
	enum class Counts : std::uint8_t
	{
		kAlways,
		kRestockAnyDay,  // only while a trader can restock at every build of the barter screen, see RestocksAnyDay
	};

	struct PlaceRow
	{
		std::string_view     what;
		std::array<Piece, 2> pieces{};  // kNone fills the rest
		Counts               counts = Counts::kAlways;
	};

	// Every place by the name its patch call gives it, in Features.cpp's
	// order, with each piece a player sees go off while another mod has the
	// place.
	constexpr PlaceRow PLACES[]{
		// CraftingPerks
		{ "perk chart", { kPerksChart } },
		{ "Pip-Boy perks page", { kPerksPipboy } },
		// WeaponEvents
		{ "fire", { kGunWear } },
		// HealthDamage
		{ "display", { kCardDmgPhysical } },
		{ "attack1", { kDmgMeleePhysical } },
		{ "attack1 health", { kDmgMeleePhysical } },
		{ "attack2", { kDmgGunPhysical } },
		{ "attack2 health", { kDmgGunPhysical } },
		{ "types1", { kDmgMeleeEnergy } },
		{ "types2", { kDmgGunEnergy } },
		{ "types3", { kDmgBlastEnergy } },
		{ "hit effect", { kDmgHitEffects } },
		// The game asks here only whether a blast does more than 0 damage,
		// which a worn weapon's blast still does. Only at 0 condition with
		// fDamageFloor at 0 does NEC's answer differ, and what the game's walk
		// over each actor's body parts does then was not read.
		{ "targets", {} },
		{ "damage", { kDmgBlastObjects } },
		{ "hit", { kDmgBlastPhysical } },
		{ "blast targets", { kDmgBlastEffects } },
		{ "blast effect", { kDmgBlastEffects } },
		{ "card health", { kCardDmgEnergy } },
		{ "card types", { kCardDmgEnergy } },
		{ "card mod effects", { kCardDmgModEffects } },
		{ "card effects", { kCardDmgOwnEffects } },
		{ "card blast", { kCardDmgBlast } },
		// ArmorRating
		{ "damage resist", { kArmorDr } },
		{ "one piece rating", { kArmorBestMark } },
		{ "AI desirability", { kArmorNpcRanking } },
		{ "AI evaluate armor", { kArmorNpcRanking } },
		{ "AI evaluate armor again", { kArmorNpcRanking } },
		{ "typed resistances", { kArmorEnergyRad } },
		{ "card resistances", { kCardArmorMenus } },
		{ "card resistances compared", { kCardArmorCompare } },
		{ "pipboy card resistances", { kCardArmorPipboy } },
		{ "paper doll resistances", { kCardArmorDoll } },
		// BrokenEquip
		{ "equip check", { kBrokenTakeOff } },
		{ "equip button", { kBrokenTakeOff } },
		{ "put back", { kBrokenPutBack } },
		// ItemValue
		{ "value health", { kPrices } },
		{ "value", { kPrices } },
		// FireRate
		{ "fire speed", { kFireRate } },
		{ "fire rate", { kFireRate } },
		{ "fire sound", { kFireSound } },
		{ "blade cut", { kFireRate } },
		// CritMeter
		{ "crit meter", { kCritMeter } },
		{ "crit chance", { kNpcCrits } },
		{ "crit roll", { kNpcCrits } },
		// Accuracy
		{ "spread", { kAccuracy } },
		{ "crosshair", { kCrosshair } },
		{ "crosshair size", { kCrosshair } },
		{ "VATS accuracy", { kVatsAccuracy } },
		// Range
		{ "range falloff", { kRange } },
		// Jam
		{ "reload", { kJamReload } },
		// SpawnCondition
		{ "additem locked", { kWornLoot } },
		{ "additem", { kWornLoot } },
		{ "container locked", { kWornLoot } },
		{ "container", { kWornLoot } },
		{ "list build", { kModlessLoot } },
		{ "list rebuild", { kModlessLoot } },
		{ "pickup", { kWornLoot } },
		{ "handover", { kWornLoot } },
		{ "actor handover", { kWornLoot } },
		{ "spawndupe", { kConsoleNew } },
		{ "new reference", { kWorldLoot } },
		{ "drop", { kWorldLoot } },
		{ "first attach reroll", { kWorldLoot } },
		{ "respawn reroll", { kWorldLoot } },
		{ "unchanged reroll", { kWorldLoot } },
		{ "enable reroll", { kWorldLoot } },
		{ "reroll mods", { kWorldLoot } },
		{ "spawn container", { kWorldLoot } },
		{ "condition item health", { kWorldLoot } },
		{ "script item health", { kWorldLoot } },
		{ "console commands", { kConsoleNew } },
		{ "script additem", { kGifts } },
		{ "script removeitem", { kGifts } },
		// ItemCard
		{ "container card", { kCndContainers } },
		{ "examine card", { kCndWorkbench } },
		{ "cooking card", { kCndCooking } },
		{ "pipboy card", { kCndPipboy } },
		{ "pipboy card rebuild", { kCndPipboy } },
		{ "card fire rate", { kRateCards } },
		{ "card fire rate compared", { kRateCards } },
		{ "pipboy card fire rate", { kRateCards } },
		{ "better check types", { kRateBetter } },
		{ "better check equipped types", { kRateBetter } },
		{ "better check fire rate", { kRateBetter } },
		{ "better check equipped fire rate", { kRateBetter } },
		{ "sort mods", { kSortNote } },
		{ "sort fire rate", { kRateSort } },
		{ "card accuracy", { kAccuracyCards } },
		{ "card accuracy compared", { kAccuracyCards } },
		{ "pipboy card accuracy", { kAccuracyCards } },
		{ "sort accuracy", { kAccuracySort } },
		{ "card range", { kRangeCards } },
		{ "card range compared", { kRangeCards } },
		{ "pipboy card range", { kRangeCards } },
		// HudParts
		{ "HUD menu delete", { kHudBars } },
		// QuickContainer
		{ "quick container row", { kLootMeters } },
		{ "quick container rows", { kLootMeters } },
		// Workbench
		// No mod takes anything here, see Part::kBenchMessages, so the piece
		// never goes off with this place. It only lets the place's line say
		// NEC's 2 messages do nothing while Workbench repairs is off.
		{ "bench calls", { kBench } },
		// Only the mod lock's last check, since a repair never passes here. A
		// worn item's slots stay shut without it.
		{ "bench try create", { kBenchDetails } },
		// The game and a repair reach it only with a model in the viewer.
		{ "bench highlight part", { kBenchDetails } },
		{ "bench can repair", { kBench } },
		{ "bench switch item", { kBench } },
		{ "bench repair", { kBench } },
		{ "bench item list", { kBenchItemList } },
		{ "bench mod slots", { kBenchModLists } },
		{ "bench mod choices", { kBenchModLists } },
		{ "item list refresh", { kBenchItemList } },
		// ConfirmScroll
		{ "confirm box messages", { kConfirmScroll } },
		{ "confirm box input", { kConfirmScroll } },
		{ "confirm box keys", { kConfirmScroll } },
		// VendorRepair
		{ "barter messages", { kTraderRepairs } },
		{ "barter key", { kTraderRepairs } },
		{ "barter highlight", { kTraderRepairs } },
		{ "barter list", { kTraderRepairs } },
		// No mod takes anything here either, see Part::kTraderKeys. It only
		// lets the place's line say the key rule does nothing while Trader
		// repairs is off.
		{ "barter input", { kTraderRepairs } },
		// A restock falls due only as the screen opens, for both kinds of
		// chest. A trade and an investment build the screen again with the
		// game paused, so what was due has already restocked.
		{ "barter screen opens", { kStockMerchant, kStockLinked } },
		{ "barter trade", { kStockMerchant, kStockLinked }, Counts::kRestockAnyDay },
		{ "barter investment", { kStockMerchant, kStockLinked }, Counts::kRestockAnyDay },
		{ "merchant container restock", { kStockMerchant } },
		{ "linked chest restock", { kStockLinked } },
		// InspectPrice
		{ "inspect price", { kInspectPrice } },
		// ConsoleRepair
		{ "console srm", { kSrm } },
		// LoadingTips
		{ "loading screens", { kTips } },
	};

	// -------------------------------------------------------------------
	// The checks
	// -------------------------------------------------------------------

	// Every place once, with no piece or with pieces of a part, kNone only
	// filling the end and no piece twice.
	consteval bool PlacesOnce()
	{
		for (std::size_t i = 0; i < std::size(PLACES); i++) {
			const auto& pieces = PLACES[i].pieces;
			if (std::ranges::count(pieces, kMeleeWear) != 0 || (pieces[0] == kNone && pieces[1] != kNone) ||
				(pieces[1] != kNone && pieces[1] == pieces[0])) {
				return false;
			}
			for (std::size_t j = 0; j < i; j++) {
				if (PLACES[j].what == PLACES[i].what) {
					return false;
				}
			}
		}
		return true;
	}
	static_assert(PlacesOnce());

	// Whether a trader can restock at every build of the barter screen,
	// which only iDaysToRespawnVendor below 1 allows. It is 2 in Fallout4.esm.
	// Read at every call, since a mod can change it, and counted as no until
	// every file has loaded.
	bool RestocksAnyDay()
	{
		auto*       settings = g_dataHandler ? RE::GameSettingCollection::GetSingleton() : nullptr;
		const auto* days = settings ? settings->GetSetting("iDaysToRespawnVendor"sv) : nullptr;
		return days && days->GetType() == RE::Setting::SETTING_TYPE::kInt && days->GetInt() < 1;
	}
}

std::span<const Piece> PiecesAt(std::string_view a_what)
{
	const auto it = std::ranges::find(PLACES, a_what, &PlaceRow::what);
	if (it == std::end(PLACES) || (it->counts == Counts::kRestockAnyDay && !RestocksAnyDay())) {
		return {};
	}
	const auto count = std::ranges::find(it->pieces, Piece::kNone) - it->pieces.begin();
	return { it->pieces.data(), static_cast<std::size_t>(count) };
}
