#include "Core/Pieces.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

// Which pieces each place is, and what each piece needs. A place that only
// feeds the trace logs has no row. A new place gets a row here, or its
// part's line never names it.
namespace
{
	using enum Piece;

	constexpr PieceRow PIECES[]{
		{ kNone, Part::kNone, Shown::kHidden },
		{ kBench, Part::kBench, Shown::kPart },
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
		// A detail of a switched row needs the row's own piece, as its hooks
		// ask the same switch.
		{ kNpcCrits, Part::kNpcCrits, Shown::kPart, { kCritMeter } },
		// Every shot's jam comes through Gun wear from firing's fire call.
		{ kJamShot, Part::kJam, Shown::kPart, { kGunWear } },
		{ kJamReload, Part::kReloadJam, Shown::kPart, { kJamShot } },
		{ kWornLoot, Part::kSpawn, Shown::kPart },
		{ kConsoleNew, Part::kConsoleNew, Shown::kPart, { kWornLoot } },
		{ kOldSaves, Part::kOldSaves, Shown::kPart, { kWornLoot } },
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
		{ kRateSort, Part::kCardRate, Shown::kName, { kFireRate } },
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
		// Jam
		{ "reload", { kJamReload } },
		// SpawnCondition
		{ "additem locked", { kWornLoot } },
		{ "additem", { kWornLoot } },
		{ "addcount locked", { kWornLoot } },
		{ "addcount", { kWornLoot } },
		{ "container locked", { kWornLoot } },
		{ "container", { kWornLoot } },
		{ "console commands", { kConsoleNew } },
		{ "saved stack", { kOldSaves } },
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
		{ "sort mods", { kRateSort } },
		{ "sort fire rate", { kRateSort } },
		// HudParts
		{ "HUD menu delete", { kHudBars } },
		// QuickContainer
		{ "quick container row", { kLootMeters } },
		{ "quick container rows", { kLootMeters } },
		// Workbench
		{ "bench calls", { kBench } },
		{ "bench build confirmed", { kBench } },
		{ "bench mod choice", { kBench } },
		{ "bench build failure", { kBench } },
		{ "bench try create", { kBench } },
		{ "bench highlight part", { kBench } },
		{ "bench confirm label", { kBench } },
		{ "bench confirm question", { kBench } },
		{ "bench can repair", { kBench } },
		{ "bench switch item", { kBench } },
		{ "bench repair", { kBench } },
		{ "bench confirm delete", { kBench } },
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
			return (a_row.part == Part::kNone) == (a_row.piece == kNone || a_row.piece == kMeleeWear) && a_row.part != Part::kTrace;
		});
	}
	static_assert(PartsShown());

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

std::span<const PieceRow> PieceRows()
{
	return PIECES;
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

bool Whole(Part a_part, std::span<const Piece> a_pieces)
{
	return std::ranges::all_of(PIECES, [&](const PieceRow& a_row) {
		return !ShownIn(a_row.piece, a_part) || std::ranges::find(a_pieces, a_row.piece) != a_pieces.end();
	});
}
