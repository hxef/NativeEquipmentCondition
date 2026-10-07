#include "Core/CallPatch/CallPatch.h"

#include "Core/CallPatch/Ledger.h"

#include <cstdint>
#include <format>
#include <optional>

namespace CallPatch
{
	namespace
	{
		// Whether the game's own call or jump to a_expected sits at a_where,
		// reading the opcode and following the displacement. A game update
		// that moved the code by 1 byte would otherwise get a 5 byte write
		// into the middle of another instruction, and a jump written where the
		// game has a call returns somewhere else.
		bool Matches(std::uintptr_t a_where, std::uint8_t a_opcode, std::uintptr_t a_expected)
		{
			return Read<std::uint8_t>(a_where) == a_opcode && a_where + REL32_SIZE + Read<std::int32_t>(a_where + 1) == a_expected;
		}
	}

	bool PatchCall(const CallSite& a_site, std::uintptr_t a_expected, std::uintptr_t a_hook, LinkBase& a_link, Part a_part)
	{
		const auto addr = Address(a_site);
		const auto opcode = a_site.jump ? OPCODE_JMP_REL32 : OPCODE_CALL_REL32;
		return Note({ .what = a_site.what,
			.where = addr,
			.kind = a_site.jump ? Kind::kJump : Kind::kCall,
			.hook = a_hook,
			.part = a_part,
			.free = Matches(addr, opcode, a_expected),
			.game = a_expected,
			.link = &a_link });
	}

	bool PatchVirtualCall(const CallSite& a_site, std::size_t a_slot, std::uintptr_t a_hook, LinkBase& a_link, Part a_part)
	{
		const auto addr = Address(a_site);

		const auto opcode = Read<std::uint8_t>(addr);
		const auto modrm = Read<std::uint8_t>(addr + 1);
		const auto displacement = Read<std::int32_t>(addr + 2);

		const bool isSixByteVirtualCall = opcode == OPCODE_GROUP_FF &&
		                                  (modrm & MODRM_MOD_REG_MASK) == MODRM_CALL_DISP32 &&
		                                  (modrm & MODRM_RM_MASK) != MODRM_RM_SIB;
		const bool free = isSixByteVirtualCall && displacement == static_cast<std::int32_t>(a_slot * sizeof(std::uintptr_t));
		return Note({ .what = a_site.what, .where = addr, .kind = Kind::kVirtualCall, .hook = a_hook, .part = a_part, .free = free, .link = &a_link });
	}

	std::size_t PatchSites(std::span<const CallSite> a_sites, REL::ID a_target, std::span<const std::uintptr_t> a_hooks,
		std::span<LinkBase* const> a_links, const char* a_what, Part a_part)
	{
		const auto target = a_target.address();

		// A place NEC runs on top of goes in too, so it counts.
		std::size_t patched = 0;
		for (std::size_t i = 0; i < a_sites.size(); i++) {
			if (PatchCall(a_sites[i], target, a_hooks[i], *a_links[i], a_part)) {
				patched++;
			}
		}

		// Every place NEC leaves alone has its own line already, see Note.
		if (patched > 0) {
			SayPatched(a_part, std::format("{:s}: {:d} of {:d} call sites.", a_what, patched, a_sites.size()));
		}
		return patched;
	}

	Held PatchTogether(std::initializer_list<Patch> a_patches, Part a_part)
	{
		const Together together{ a_part };
		for (const auto& patch : a_patches) {
			PatchCall(patch.site, patch.expected, patch.hook, *patch.link, a_part);
		}
		return together.Set();
	}

	std::optional<std::uintptr_t> PatchPointer(std::uintptr_t a_where, std::uintptr_t a_hook, const char* a_what, Part a_part,
		bool a_handsOn, LinkBase* a_link)
	{
		const auto held = Read<std::uintptr_t>(a_where);
		const auto free = Supported() && (held == 0 || InGame(held));
		if (!Note({ .what = a_what,
				.where = a_where,
				.kind = Kind::kPointer,
				.hook = a_hook,
				.part = a_part,
				.free = free,
				.handsOn = a_handsOn,
				.link = a_link })) {
			return std::nullopt;
		}
		return held;
	}
}
