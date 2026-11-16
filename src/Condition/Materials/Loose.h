#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

// The mods a copy hands back as loose mods, the Misc items a mod swap at the
// workbench leaves in the inventory. Vanilla's swap returns a mod when its
// entry is on, the mod names a loose mod, and the mod lacks the keyword
// if_NullMod, the default object CNMK. A mod it never returns, a legendary
// one among them, stays part of the copy.
namespace Materials
{
	// 1 mod of a copy that comes off as a loose mod.
	struct LooseMod
	{
		const RE::BGSMod::Attachment::Mod* mod{ nullptr };
		RE::TESObjectMISC*                 item{ nullptr };
		// The entry's rank. The loose mod carries it, so it stacks with the
		// one a swap returns.
		std::uint8_t rank{ 0 };
		// The mod's own name as the bench lists it, "Suppressor".
		std::string name;
		// The loose mod's value, what the Misc tab shows as VAL.
		std::uint32_t caps{ 0 };

		// The mod's form ID.
		[[nodiscard]] std::uint32_t FormID() const { return mod->GetFormID(); }
	};

	// Every mod of the copy with a_extra that comes off, in the order of its
	// mod list. None for a null list.
	[[nodiscard]] std::vector<LooseMod> LooseMods(const RE::ExtraDataList* a_extra);

	// The form ID of if_NullMod as the load order has it, 0005E10B in
	// vanilla, or 0 for none. For the trace.
	[[nodiscard]] std::uint32_t NullModKeyword();

	// Adds 1 of each loose mod to a_player's inventory with its rank, with no
	// line in the corner, as a swap does.
	void Give(RE::PlayerCharacter& a_player, std::span<const LooseMod> a_mods);
}
