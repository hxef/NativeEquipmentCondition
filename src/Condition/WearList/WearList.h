#pragma once

#include "Core/Plugin.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The wear list, for a player or a patch that sets how fast a weapon or a
// piece of armor wears and how much of its damage or protection it keeps when
// worn. A file in Data/F4SE/Plugins/NEC/Wear/, such as MyWeapons.ini, has 1
// form a line in the form Core/ListFiles.h gives, with 1 or more words and
// their numbers after it, in any order:
//
//   Fallout4.esm|187A10 = wear 1.2 damage 0.5
//   Fallout4.esm|11D3C3 = wear 1.5 protection 0.5
//
// wear runs from 0 to 10 and multiplies how fast the item wears, 1 for
// normal. damage runs from 0 to 1, the share of its damage a weapon keeps at
// 0 condition, and stands in for the MCM slider Damage at 0 condition.
// protection is the same for armor, the share of its protection kept at 0,
// in place of fArmorFloor. The ID is a weapon, a piece of armor or clothing,
// a mod for either or a keyword. A weapon's line takes no protection and an
// armor line no damage, while a keyword's takes both, each for the kind it
// fits. The item's own line beats a keyword's, and of its keywords the line
// read first counts. Each listed mod on the copy multiplies the wear, up to
// 10 in all, and the mod line with damage or protection read first beats a
// keyword's. A form set again keeps the number read first. Power armor
// pieces keep vanilla's own wear, so their lines are skipped. NEC ships an example file
// there that changes nothing.
namespace WearList
{
	// Reads every .ini file in the folder and says in NEC.log what each line
	// sets and why it skipped one. Runs after Excluded, since a listed weapon
	// is skipped. Keeps form IDs and keywords only, so Unload lets them go.
	void Load();

	// Forgets the list, so every weapon and piece wears and keeps damage or
	// protection as the MCM sliders say until Load runs.
	void Unload();

	// Whether any file set a number, 1 relaxed load.
	[[nodiscard]] bool Any();

	// 1 number a line set, and where.
	struct Set
	{
		float         value = 1.0F;
		std::uint32_t file = 0;  // its index in the files the list read
		std::uint32_t line = 0;  // the first line is 1
		std::uint32_t load = 0;  // which read of the list, so a later read never names its file wrong
	};

	// What the lines give 1 copy of a weapon or a piece of armor.
	struct Pick
	{
		// How many lines the trace names, the item's or keyword's and 7
		// mods'. Every line past them still counts in wear, and the trace
		// says how many more there are.
		static constexpr std::size_t MOST_FROM = 8;

		// The factor on how fast it wears, 1 with no line.
		float wear = 1.0F;

		// The share of damage a weapon keeps at 0 condition, or nothing for
		// the slider's.
		std::optional<Set> damage;

		// The share of protection a piece keeps at 0 condition, or nothing
		// for fArmorFloor's.
		std::optional<Set> protection;

		// The lines wear multiplies, the item's or keyword's first. wearCount
		// counts them all, and wearFrom holds the first MOST_FROM.
		std::array<Set, MOST_FROM> wearFrom{};
		std::size_t                wearCount = 0;

		// Whether the lines multiplied to more than 10.
		bool capped = false;

		// ", wear x3.00 by Wear/A_First.ini line 5 x1.50 and line 6 x2.00",
		// for a trace line, or empty with no line. Files are named by the list
		// read last, see Where in Table.h.
		[[nodiscard]] std::string WearTail() const;
	};

	// "  keeps 0.50 at 0 by Wear/A_First.ini line 6", for a trace line about
	// a_damage, a Pick's damage, or empty with none. The file is named by the
	// list read last, as in WearTail, so the damage hooks carry the number
	// alone.
	[[nodiscard]] std::string DamageTail(const std::optional<Set>& a_damage);

	// The lines for 1 copy of a_weapon: its own, its keywords' as a_instance
	// gives them, and the enabled mods a_extra lists, a legendary one
	// included. a_instance and a_extra may be null, which leaves out the
	// copy's mods and the keywords they add. Nothing at all while no file
	// set a number. The list is safe from any thread, see Core/Snapshot.h, and
	// the caller keeps a_extra alive for the call.
	[[nodiscard]] Pick Of(const RE::TESObjectWEAP& a_weapon, const RE::TESObjectWEAP::InstanceData* a_instance,
		const RE::ExtraDataList* a_extra);

	// The same for 1 copy of a_armor, with protection in place of damage.
	[[nodiscard]] Pick Of(const RE::TESObjectARMO& a_armor, const RE::TESObjectARMO::InstanceData* a_instance,
		const RE::ExtraDataList* a_extra);
}
