#pragma once

#include <array>
#include <initializer_list>
#include <span>

// Redirecting one call instruction instead of the function it points at. A
// detour rewrites the function's first bytes, so every caller lands in the
// hook. This patches the 1 call that matters and leaves the rest alone, which
// is what the condition scaling needs: the calls combat makes, not the ones the
// save system makes.
namespace CallPatch
{
	// A call site: the Address Library ID of the function around it plus the
	// offset of the call instruction inside, read off the 1.11.240 build. The
	// library maps a stable ID to the function's address in any build, but not
	// offsets inside it, so PatchCall checks the bytes before writing. what
	// names the site in the log, short because it is on every trace line.
	struct CallSite
	{
		std::uint64_t id;
		std::size_t   offset;
		const char*   what;
	};

	// call rel32 and jmp rel32: 1 opcode byte and a 4 byte displacement counted
	// from the end of the instruction, 5 bytes in all.
	inline constexpr std::uint8_t OPCODE_CALL_REL32 = 0xE8;
	inline constexpr std::uint8_t OPCODE_JMP_REL32 = 0xE9;
	inline constexpr std::size_t  REL32_SIZE = 5;

	// Where a site's instruction sits in the running game.
	inline std::uintptr_t Address(const CallSite& a_site)
	{
		const REL::Relocation<std::uintptr_t> owner{ REL::ID(a_site.id) };
		return owner.address() + a_site.offset;
	}

	// Checks that a call to a_expected sits at the site, reading the opcode and
	// following the displacement. A game update that moved the code by 1 byte
	// would otherwise get a 5 byte write into the middle of another
	// instruction. Never writes, and logs a mismatch.
	inline bool Matches(const CallSite& a_site, std::uintptr_t a_expected)
	{
		const auto addr = Address(a_site);

		const auto opcode = *reinterpret_cast<const std::uint8_t*>(addr);
		const auto displacement = *reinterpret_cast<const std::int32_t*>(addr + 1);
		const auto target = addr + REL32_SIZE + displacement;

		if ((opcode != OPCODE_CALL_REL32 && opcode != OPCODE_JMP_REL32) || target != a_expected) {
			REX::ERROR("{:s} at {:X} is not the call it was expected to be, leaving it alone.", a_site.what, addr);
			return false;
		}
		return true;
	}

	// Points one call at a hook, once Matches has checked it. The function
	// itself is untouched, so the hook may call it through its address. A
	// site can be a jump: a call that is the last thing a function does is
	// often compiled as a jump into the callee, a tail call. Such a site is
	// repointed with a jump, and the hook returns what the callee would have.
	inline bool PatchCall(const CallSite& a_site, std::uintptr_t a_expected, std::uintptr_t a_hook)
	{
		if (!Matches(a_site, a_expected)) {
			return false;
		}

		const auto addr = Address(a_site);
		if (*reinterpret_cast<const std::uint8_t*>(addr) == OPCODE_JMP_REL32) {
			REL::GetTrampoline().write_jmp<5>(addr, a_hook);
		} else {
			REL::GetTrampoline().write_call<5>(addr, a_hook);
		}
		return true;
	}

	// call qword ptr [reg+disp32], the 6 byte call through a vtable: opcode FF,
	// a ModRM byte and the slot's offset as a 4 byte displacement. In the ModRM
	// byte the top 2 bits say a 4 byte displacement follows, the middle 3 pick
	// call and the low 3 name the register. Registers 8 to 15 add a prefix byte
	// and low bits 100 add a SIB byte, so neither form is patched.
	inline constexpr std::uint8_t OPCODE_GROUP_FF = 0xFF;
	inline constexpr std::uint8_t MODRM_MOD_REG_MASK = 0xF8;
	inline constexpr std::uint8_t MODRM_CALL_DISP32 = 0x90;
	inline constexpr std::uint8_t MODRM_RM_MASK = 0x07;
	inline constexpr std::uint8_t MODRM_RM_SIB = 0x04;
	inline constexpr std::size_t  VCALL_SIZE = 6;

	// PatchCall for a call through a vtable. The slot stands in for the
	// expected target, and the hook makes the virtual call itself.
	inline bool PatchVirtualCall(const CallSite& a_site, std::size_t a_slot, std::uintptr_t a_hook)
	{
		const auto addr = Address(a_site);

		const auto opcode = *reinterpret_cast<const std::uint8_t*>(addr);
		const auto modrm = *reinterpret_cast<const std::uint8_t*>(addr + 1);
		const auto displacement = *reinterpret_cast<const std::int32_t*>(addr + 2);

		const bool isSixByteVirtualCall = opcode == OPCODE_GROUP_FF &&
		                                  (modrm & MODRM_MOD_REG_MASK) == MODRM_CALL_DISP32 &&
		                                  (modrm & MODRM_RM_MASK) != MODRM_RM_SIB;
		if (!isSixByteVirtualCall || displacement != static_cast<std::int32_t>(a_slot * sizeof(std::uintptr_t))) {
			REX::ERROR("{:s} at {:X} is not the call it was expected to be, leaving it alone.", a_site.what, addr);
			return false;
		}

		REL::GetTrampoline().write_call<VCALL_SIZE>(addr, a_hook);
		return true;
	}

	// Patches every site in a list that calls a_target, and logs how many
	// succeeded.
	inline std::size_t PatchAll(std::span<const CallSite> a_sites, REL::ID a_target,
		std::span<const std::uintptr_t> a_hooks, const char* a_what)
	{
		const auto target = a_target.address();

		std::size_t patched = 0;
		for (std::size_t i = 0; i < a_sites.size(); i++) {
			if (PatchCall(a_sites[i], target, a_hooks[i])) {
				patched++;
			}
		}

		if (patched == 0) {
			REX::ERROR("{:s}: no call sites matched, this part of the mod will do nothing.", a_what);
		} else {
			REX::INFO("{:s}: {:d} of {:d} call sites.", a_what, patched, a_sites.size());
		}
		return patched;
	}

	// One call of a PatchTogether set: the site, what it calls, the hook.
	struct Patch
	{
		CallSite       site;
		std::uintptr_t expected;
		std::uintptr_t hook;
	};

	// Patches calls that only work together, all or none: a hook that reads
	// what another hook noted, or 2 results compared with each other. Every
	// site is checked before any is written.
	inline bool PatchTogether(std::initializer_list<Patch> a_patches)
	{
		bool matched = true;
		for (const auto& patch : a_patches) {
			if (!Matches(patch.site, patch.expected)) {
				matched = false;
			}
		}
		if (!matched) {
			return false;
		}

		for (const auto& patch : a_patches) {
			PatchCall(patch.site, patch.expected, patch.hook);
		}
		return true;
	}

	// The same hook for every site in a list.
	template <std::size_t N>
	std::array<std::uintptr_t, N> Repeat(std::uintptr_t a_hook)
	{
		std::array<std::uintptr_t, N> hooks{};
		hooks.fill(a_hook);
		return hooks;
	}

	template <class T, std::size_t N>
	std::array<std::uintptr_t, N> AsAddresses(const std::array<T, N>& a_hooks)
	{
		std::array<std::uintptr_t, N> addresses{};
		for (std::size_t i = 0; i < N; i++) {
			addresses[i] = reinterpret_cast<std::uintptr_t>(a_hooks[i]);
		}
		return addresses;
	}
}
