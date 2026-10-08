#pragma once

#include "Core/Settings.h"

#include <cstddef>
#include <cstdint>
#include <span>

// Every part of NEC another mod can take from it, by the name a player reads
// on the MCM page and in NEC.log, in Features.cpp's order. Each place NEC
// patches belongs to 1 part: its row's own, see Feature::part, or the one its
// patch call names, see CallPatch.h. Each part has 1 or more pieces, see
// Core/Pieces.h. The names live in Core/Text, 1 table a part, and what each
// setting works through in SETTINGS in Parts.cpp.
enum class Part : std::uint8_t
{
	kNone,           // no part, the default of a row that patches nothing
	kPerks,          // Perk repair discount text
	kGunWear,        // Gun wear from firing
	kDamage,         // Worn weapon damage
	kCardDamage,     // Damage shown in menus
	kArmor,          // Worn armor protection
	kCardArmor,      // Protection shown in menus
	kBroken,         // Broken gear rules
	kPrices,         // Worn item prices
	kFireRate,       // Slower fire when worn
	kFireSound,      // Slower firing sound
	kCritMeter,      // Fewer criticals when worn
	kNpcCrits,       // Fewer NPC criticals
	kJam,            // Jamming
	kReloadJam,      // Single shot gun jams
	kSpawn,          // Worn loot
	kConsoleNew,     // Console items arrive new
	kOldSaves,       // Old save stacks split up
	kGifts,          // Quest rewards arrive new
	kCardCnd,        // CND on item cards
	kCardRate,       // Fire rate shown in menus
	kHudBar,         // HUD condition bars
	kQuick,          // Loot preview meters
	kBench,          // Workbench repairs
	kBenchLists,     // Worn items in workbench lists
	kScroll,         // Scrolling component lists
	kTraderRepairs,  // Trader repairs
	kStock,          // Trader stock condition
	kInspect,        // Inspect price fix
	kSrm,            // Console repair (srm)
	kTips,           // Loading screen tips
	// From here on, places no mod takes anything from, each named after its
	// row's part.
	kTrace,          // a place that only feeds the trace logs
	kBenchMessages,  // the bench place that also shows NEC's 2 messages when MODIFY opens nothing
	kTotal,
};

inline constexpr std::size_t PART_COUNT = static_cast<std::size_t>(Part::kTotal);

// The 2 name files of Core/Text split the parts here: the Condition and
// Gameplay rows up to kGifts, the UI rows from kCardCnd to kTips.
static_assert(static_cast<int>(Part::kGifts) + 1 == static_cast<int>(Part::kCardCnd));
static_assert(static_cast<int>(Part::kTips) + 1 == static_cast<int>(Part::kTrace));

enum class Piece : std::uint8_t;  // Core/Pieces.h

// What a setting works through, by piece, for the MCM page and the Settings
// line. With every core piece off it does nothing. A side piece takes a piece
// of it away. An exception is something NEC changes about it, or a text that
// tells of it, which only gets its part's line. always is what it changes that
// no mod can take. A switch leads its own block on the page, and so does a
// number with nothing above it. Any other number sits in the block of the
// setting named by under, a switch or a number that leads, and config.json
// lays the page out the same way.
struct SettingLink
{
	const Settings::Named* setting;
	const Settings::Named* under;  // nullptr for a setting that leads its own block
	std::span<const Piece>      core{};
	std::span<const Piece>      side{};
	std::span<const Piece>      exceptions{};
	Piece                       always{};  // kNone for none
};

// Every setting that works through a part, in NEC.ini's order.
[[nodiscard]] std::span<const SettingLink> SettingLinks();

// a_setting's row, or nullptr for one that works through no part.
[[nodiscard]] const SettingLink* LinkOf(const Settings::Named& a_setting);
