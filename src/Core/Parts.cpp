#include "Core/Parts.h"

#include "Core/Pieces.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>

// What each setting works through. The MCM page reads it to say which
// settings do nothing, or which of their pieces are off, while another mod
// has a part, and the Settings line marks them the same way, so no setting
// needs code of its own for it.
//
// config.json lays out its blocks by under. A number moved here moves there
// too.
namespace
{
	using enum Piece;

	// A shot's jam comes through Gun wear from firing, which Jam's row rides
	// on, see Features.cpp. A single shot gun's comes through its own reload
	// call.
	constexpr Piece JAM[]{ kJamShot };
	constexpr Piece JAM_SIDE[]{ kJamReload };
	// The 2 side parts roll loot lying in the world and a stock's items with
	// no mod slots. A lost set costs only what that set did, see its WARN
	// line. The 2 exceptions are cases Worn loot handles on their own, so
	// losing one takes nothing from the switch.
	constexpr Piece WORN_LOOT[]{ kWornLoot };
	constexpr Piece WORN_LOOT_SIDE[]{ kWorldLoot, kModlessLoot };
	constexpr Piece WORN_LOOT_EXCEPTIONS[]{ kConsoleNew, kGifts };
	// A repair is priced from a worn item's price, which only Worn item
	// prices gives. Without it a quote still scales, but from the game's own
	// barter price.
	constexpr Piece TRADER_REPAIRS[]{ kTraderRepairs };
	constexpr Piece TRADER_REPAIRS_SIDE[]{ kStockMerchant, kStockLinked, kPrices };
	constexpr Piece TRADER_PRICE_SIDE[]{ kPrices };
	// A tip about a part that is off hides on purpose, so it loses nothing
	// else.
	constexpr Piece TIPS[]{ kTips };
	constexpr Piece HUD_BARS[]{ kHudBars };
	constexpr Piece LOOT_METERS[]{ kLootMeters };
	constexpr Piece CONFIRM_SCROLL[]{ kConfirmScroll };
	constexpr Piece INSPECT_PRICE[]{ kInspectPrice };
	// Melee hits and bashes wear a weapon through a hit event no mod can take,
	// so it never does nothing.
	constexpr Piece GUN_WEAR[]{ kGunWear };
	constexpr Piece DAMAGE[]{ kDmgMeleePhysical, kDmgMeleeEnergy, kDmgGunPhysical, kDmgGunEnergy, kDmgBlastPhysical,
		kDmgBlastEnergy, kDmgBlastObjects, kDmgHitEffects, kDmgBlastEffects, kCardDmgPhysical, kCardDmgEnergy,
		kCardDmgModEffects, kCardDmgOwnEffects, kCardDmgBlast };
	constexpr Piece ARMOR[]{ kArmorDr, kArmorEnergyRad, kArmorBestMark, kArmorNpcRanking, kCardArmorMenus, kCardArmorCompare,
		kCardArmorPipboy, kCardArmorDoll };
	// Read only inside Worn item prices' own hook.
	constexpr Piece PRICES[]{ kPrices };
	constexpr Piece FIRE_RATE[]{ kFireRate };
	constexpr Piece FIRE_RATE_SIDE[]{ kFireRateNpcGuns, kFireSound, kRateCardsContainers, kRateCardsWorkbench, kRateCardsCooking,
		kRateCardsPipboy, kRateBetter, kRateSort };
	// Your meter and an NPC's roll each work alone.
	constexpr Piece CRIT_METER[]{ kCritMeter, kNpcCrits };
	// Every shot's spread, yours and every NPC's, and the VATS hit chance
	// each work alone. The crosshair and the menus show the spread.
	constexpr Piece ACCURACY[]{ kAccuracy, kVatsAccuracy };
	constexpr Piece ACCURACY_SIDE[]{ kCrosshair, kAccuracyCardsContainers, kAccuracyCardsWorkbench, kAccuracyCardsCooking,
		kAccuracyCardsPipboy, kAccuracySort };
	constexpr Piece RANGE[]{ kRange };
	constexpr Piece RANGE_SIDE[]{ kRangeCardsContainers, kRangeCardsWorkbench, kRangeCardsCooking, kRangeCardsPipboy };

	// Every piece of Lists, in their order.
	template <const auto&... Lists>
	consteval auto Joined()
	{
		std::array<Piece, (std::size(Lists) + ...)> joined{};
		std::size_t                                 i = 0;
		for (const auto list : { std::span<const Piece>{ Lists }... }) {
			for (const auto piece : list) {
				joined[i++] = piece;
			}
		}
		return joined;
	}

	// The weapon start moves every weapon penalty's line, so it does
	// nothing only once all of them are off.
	constexpr auto WEAPON_PENALTIES = Joined<DAMAGE, FIRE_RATE, CRIT_METER, ACCURACY, RANGE, JAM>();
	constexpr auto WEAPON_PENALTIES_SIDE = Joined<FIRE_RATE_SIDE, ACCURACY_SIDE, RANGE_SIDE, JAM_SIDE>();

	constexpr Piece BENCH[]{ kBench };
	// The perk pages tell of a discount on a bench repair, and the bench's
	// lists show what it repairs, so the line of each part shows in this
	// block.
	constexpr Piece BENCH_EXCEPTIONS[]{ kPerksChart, kPerksPipboy, kBenchItemList, kBenchModLists };

	constexpr SettingLink SETTINGS[]{
		{ &Settings::bJam, nullptr, JAM, JAM_SIDE },
		{ &Settings::bSpawnCondition, nullptr, WORN_LOOT, WORN_LOOT_SIDE, WORN_LOOT_EXCEPTIONS },
		{ &Settings::bVendorRepair, nullptr, TRADER_REPAIRS, TRADER_REPAIRS_SIDE },
		{ &Settings::bLoadingTips, nullptr, TIPS },
		{ &Settings::bHudCondition, nullptr, HUD_BARS },
		{ &Settings::bQuickContainer, nullptr, LOOT_METERS },
		{ &Settings::bConfirmScroll, nullptr, CONFIRM_SCROLL },
		{ &Settings::bInspectPrice, nullptr, INSPECT_PRICE },
		{ &Settings::fWearRateMult, nullptr, {}, GUN_WEAR, {}, kMeleeWear },
		// An NPC's gun wears through the same fire call, so it sits in the
		// player's block and loses its guns with it.
		{ &Settings::fNpcWearRateMult, &Settings::fWearRateMult, {}, GUN_WEAR, {}, kMeleeWear },
		// An automatic's shots wear through the same fire call. A motor
		// blade's cuts wear through the hit event, so this row still sets
		// those.
		{ &Settings::fAutoWearRateMult, &Settings::fWearRateMult, {}, GUN_WEAR, {}, kMotorWear },
		{ &Settings::fDamageFloor, nullptr, DAMAGE },
		{ &Settings::fWeaponPenaltyStart, &Settings::fDamageFloor, WEAPON_PENALTIES, WEAPON_PENALTIES_SIDE },
		{ &Settings::fArmorFloor, nullptr, ARMOR },
		{ &Settings::fArmorPenaltyStart, &Settings::fArmorFloor, ARMOR },
		{ &Settings::fValueExponent, nullptr, PRICES },
		{ &Settings::fFireRateFloor, nullptr, FIRE_RATE, FIRE_RATE_SIDE },
		{ &Settings::fCritMeterFloor, nullptr, CRIT_METER },
		{ &Settings::fAccuracyFloor, nullptr, ACCURACY, ACCURACY_SIDE },
		{ &Settings::fRangeFloor, nullptr, RANGE, RANGE_SIDE },
		{ &Settings::fBenchCostMult, nullptr, BENCH, {}, BENCH_EXCEPTIONS },
		{ &Settings::iFreeMendAbove, &Settings::fBenchCostMult, BENCH },
		{ &Settings::fTraderPriceMult, &Settings::bVendorRepair, TRADER_REPAIRS, TRADER_PRICE_SIDE },
		// The 2 rows that draw the bars share bHudCondition, so they never
		// run once HUD condition bars is left to another mod.
		{ &Settings::fHudBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fHudBarY, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarX, &Settings::bHudCondition, HUD_BARS },
		{ &Settings::fPowerArmorBarY, &Settings::bHudCondition, HUD_BARS },
	};

	// Every setting changes something, no list names kNone or a piece twice,
	// and a piece that is always there is no other list's.
	consteval bool PiecesOnce()
	{
		for (const auto& link : SETTINGS) {
			if (link.core.empty() && link.always == kNone) {
				return false;
			}
			for (const auto list : { link.core, link.side, link.exceptions }) {
				for (const auto piece : list) {
					if (piece == kNone || Untakeable(piece)) {
						return false;
					}
					for (const auto other : { link.core, link.side, link.exceptions }) {
						if (std::ranges::count(other, piece) != (other.data() == list.data() ? 1 : 0)) {
							return false;
						}
					}
				}
			}
		}
		return true;
	}
	static_assert(PiecesOnce());

	// Every under is a setting that leads its own block.
	consteval bool UnderLeads()
	{
		for (const auto& link : SETTINGS) {
			if (!link.under) {
				continue;
			}
			bool found = false;
			for (const auto& lead : SETTINGS) {
				found = found || (!lead.under && lead.setting == link.under);
			}
			if (!found) {
				return false;
			}
		}
		return true;
	}
	static_assert(UnderLeads());
}

std::span<const SettingLink> SettingLinks()
{
	return SETTINGS;
}

const SettingLink* LinkOf(const Settings::Named& a_setting)
{
	const auto it = std::ranges::find(SETTINGS, &a_setting, &SettingLink::setting);
	return it != std::end(SETTINGS) ? &*it : nullptr;
}

std::bitset<PART_COUNT> PartsOf(const SettingLink& a_link)
{
	std::bitset<PART_COUNT> parts;
	for (const auto list : { a_link.core, a_link.side, a_link.exceptions }) {
		for (const auto piece : list) {
			parts.set(static_cast<std::size_t>(RowOfPiece(piece).part));
		}
	}
	parts.reset(static_cast<std::size_t>(Part::kNone));
	return parts;
}
