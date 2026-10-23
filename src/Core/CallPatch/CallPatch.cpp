#include "Core/CallPatch/CallPatch.h"

#include "Core/CallPatch/Ledger.h"
#include "Core/Runtime.h"

#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace CallPatch
{
	namespace
	{
		// Whether the vanilla call or jump to a_expected sits at a_where,
		// reading the opcode and following the displacement. A game update
		// that moved the code by 1 byte would otherwise get a 5 byte write
		// into the middle of another instruction, and a jump written where the
		// game has a call returns somewhere else.
		bool Matches(std::uintptr_t a_where, std::uint8_t a_opcode, std::uintptr_t a_expected)
		{
			return Read<std::uint8_t>(a_where) == a_opcode && a_where + REL32_SIZE + Read<std::int32_t>(a_where + 1) == a_expected;
		}

		// The slot of the running version's side, nothing for NO_SLOT.
		std::optional<std::size_t> SlotOf(Slot a_slot)
		{
			const auto slot = Runtime::OldGen() ? a_slot.og : a_slot.ae;
			return slot != NO_SLOT ? std::optional{ slot } : std::nullopt;
		}

		// Notes a place this game version has none of, which is never read
		// and stays off.
		bool NoteNoPlace(const char* a_what, Kind a_kind, std::uintptr_t a_hook, Part a_part, LinkBase* a_link)
		{
			return Note({ .what = a_what, .where = 0, .kind = a_kind, .hook = a_hook, .part = a_part, .free = false, .link = a_link });
		}
	}

	const At& Side(const CallSite& a_site)
	{
		return Runtime::OldGen() ? static_cast<const At&>(a_site.og) : a_site.ae;
	}

	std::optional<std::uintptr_t> Address(const CallSite& a_site)
	{
		const auto& side = Side(a_site);
		if (side.id == RE::NO_ID) {
			return std::nullopt;
		}
		const auto offset = RE::FindOffset(side.id);
		if (!offset) {
			REX::ERROR("{:s} names Address Library ID {:d}, which game version {:s} does not have, so NEC leaves it off.", a_site.what,
				side.id, Runtime::GameVersion());
			return std::nullopt;
		}
		return REX::FModule::GetExecutingModule().GetBaseAddress() + *offset + side.offset;
	}

	bool PatchCall(const CallSite& a_site, const RE::RuntimeID& a_expected, std::uintptr_t a_hook, LinkBase& a_link, Part a_part)
	{
		const auto& side = Side(a_site);
		const auto kind = side.jump ? Kind::kJump : Kind::kCall;
		const auto addr = Address(a_site);
		if (!addr || !a_expected.has()) {
			return NoteNoPlace(a_site.what, kind, a_hook, a_part, &a_link);
		}
		const auto expected = a_expected.address();
		const auto opcode = side.jump ? OPCODE_JMP_REL32 : OPCODE_CALL_REL32;
		return Note({ .what = a_site.what,
			.where = *addr,
			.kind = kind,
			.hook = a_hook,
			.part = a_part,
			.free = Matches(*addr, opcode, expected),
			.game = expected,
			.link = &a_link });
	}

	bool PatchVirtualCall(const CallSite& a_site, Slot a_slot, std::uintptr_t a_hook, LinkBase& a_link, Part a_part)
	{
		const auto where = Address(a_site);
		const auto slot = SlotOf(a_slot);
		if (!where || !slot) {
			return NoteNoPlace(a_site.what, Kind::kVirtualCall, a_hook, a_part, &a_link);
		}
		const auto addr = *where;

		const auto opcode = Read<std::uint8_t>(addr);
		const auto modrm = Read<std::uint8_t>(addr + 1);
		const auto displacement = Read<std::int32_t>(addr + 2);

		const bool isSixByteVirtualCall = opcode == OPCODE_GROUP_FF &&
		                                  (modrm & MODRM_MOD_REG_MASK) == MODRM_CALL_DISP32 &&
		                                  (modrm & MODRM_RM_MASK) != MODRM_RM_SIB;
		const bool free = isSixByteVirtualCall && displacement == static_cast<std::int32_t>(*slot * sizeof(std::uintptr_t));
		return Note({ .what = a_site.what, .where = addr, .kind = Kind::kVirtualCall, .hook = a_hook, .part = a_part, .free = free, .link = &a_link });
	}

	std::size_t PatchSites(std::span<const CallSite> a_sites, const RE::RuntimeID& a_target, std::span<const std::uintptr_t> a_hooks,
		std::span<LinkBase* const> a_links, const char* a_what, Part a_part)
	{
		// A place NEC runs on top of goes in too, so it counts. A place covered
		// by another site on OG counts as handled.
		std::size_t patched = 0;
		std::size_t covered = 0;
		for (std::size_t i = 0; i < a_sites.size(); i++) {
			const auto& site = a_sites[i];
			if (Runtime::OldGen() && site.og.covered) {
				covered++;
			} else if (PatchCall(site, a_target, a_hooks[i], *a_links[i], a_part)) {
				patched++;
			}
		}

		// Every place NEC leaves alone has its own line already, see Note.
		if (patched > 0) {
			SayPatched(a_part, std::format("{:s}: {:d} of {:d} call sites.", a_what, patched, a_sites.size() - covered));
		}
		return patched + covered;
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
		const auto free = Runtime::Supported() && (held == 0 || InGame(held));
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

	std::optional<std::uintptr_t> PatchTableSlot(const RE::RuntimeID& a_table, Slot a_slot, std::uintptr_t a_hook, const char* a_what,
		Part a_part, bool a_handsOn, LinkBase* a_link)
	{
		const auto slot = SlotOf(a_slot);
		if (!slot || !a_table.has()) {
			NoteNoPlace(a_what, Kind::kPointer, a_hook, a_part, a_link);
			return std::nullopt;
		}
		return PatchPointer(a_table.address() + *slot * sizeof(std::uintptr_t), a_hook, a_what, a_part, a_handsOn, a_link);
	}
}
