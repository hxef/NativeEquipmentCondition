#include "Core/CallPatch/Reach.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

// Following the hooks on a place from the outside in, and judging what another
// DLL left at a place before NEC. A DLL's hook sits behind a stub or 2 in
// memory no module owns and lands in the DLL's own code, which keeps the
// address it hands each call on to in its data. NEC reads that address, link
// by link, until the line reaches NEC's own hook or skips it, see Reach.cpp
// for the memory reads. Runs at install and for a place a recheck found
// changed, never in a hook.
namespace CallPatch
{
	namespace
	{
		// Stubs followed in front of 1 hook, DLLs followed on 1 place, far
		// past any real load order, the most of a DLL's hook read for the
		// address it keeps, and the most of a hook with no table entry read
		// for its jump, many times the few instructions such a hook has.
		constexpr int           MAX_HOPS = 4;
		constexpr int           MAX_LINKS = 8;
		constexpr std::uint32_t MAX_SCAN = 0x1000;
		constexpr std::uint32_t MAX_BARE = 0x40;

		// The stubs NEC follows, each only outside a module, where it is a
		// stub and never a hook: mov rax to an address then jmp rax, or push
		// rax and ret, 12 bytes. mov r11 then jmp r11, 13 bytes. push the low
		// half of an address, mov the high half above it, then ret, 14 bytes.
		constexpr std::uint8_t MOV_RAX[]{ 0x48, 0xB8 };
		constexpr std::uint8_t JMP_RAX[]{ 0xFF, 0xE0 };
		constexpr std::uint8_t PUSH_RAX_RET[]{ 0x50, 0xC3 };
		constexpr std::uint8_t MOV_R11[]{ 0x49, 0xBB };
		constexpr std::uint8_t JMP_R11[]{ 0x41, 0xFF, 0xE3 };
		constexpr std::uint8_t PUSH_LOW = 0x68;
		constexpr std::uint8_t MOV_HIGH[]{ 0xC7, 0x44, 0x24, 0x04 };
		constexpr std::uint8_t RET = 0xC3;

		// A mov into a register from the slot at the end of the instruction
		// plus disp32, 7 bytes: a REX byte, 48 for rax to rdi and 4C for r8 to
		// r15, then 8B and a ModRM byte whose low 3 bits and top 2 are 101 and
		// 00. A lea of that slot is the same with 8D, as a debug build hands
		// the slot to a helper that calls through it. A REX.W byte 48 in front
		// of FF 25 changes nothing.
		constexpr std::uint8_t REX_W = 0x48;
		constexpr std::uint8_t REX_WR = 0x4C;
		constexpr std::uint8_t MOV_LOAD = 0x8B;
		constexpr std::uint8_t LEA = 0x8D;
		constexpr std::uint8_t MODRM_RIP_MASK = 0xC7;
		constexpr std::uint8_t MODRM_RIP = 0x05;
		constexpr std::size_t  MOV_RIP_SIZE = 7;

		// What marks the line of hooks on 1 place: its kind and address, what
		// NEC wrote (its stub, cell or hook), NEC's hook there, what NEC's
		// hook hands on to, and the game's own function a DLL over NEC calls
		// to skip NEC. 0 for what NEC has not got, as at install, when nothing
		// of NEC's is there yet.
		struct Ends
		{
			Kind           kind;
			std::uintptr_t where;
			std::uintptr_t mine;
			std::uintptr_t hook;
			std::uintptr_t next;
			std::uintptr_t game;
		};

		bool Same(std::uintptr_t a_at, std::span<const std::uint8_t> a_bytes)
		{
			return std::memcmp(reinterpret_cast<const void*>(a_at), a_bytes.data(), a_bytes.size()) == 0;
		}

		// Where the stub at a_at goes on to: nothing for no stub or a landing
		// in a module, which is a hook, and 0 when where it goes cannot be
		// read.
		std::optional<std::uintptr_t> Hop(std::uintptr_t a_at)
		{
			if (ImageBase(a_at) || !Readable(a_at, REL32_SIZE)) {
				return std::nullopt;
			}
			const auto opcode = Read<std::uint8_t>(a_at);
			if (opcode == OPCODE_JMP_REL32) {
				return a_at + REL32_SIZE + Read<std::int32_t>(a_at + 1);
			}
			if (opcode == OPCODE_GROUP_FF && Read<std::uint8_t>(a_at + 1) == MODRM_JMP_RIP && Readable(a_at, RIP_SIZE)) {
				return Through(a_at + RIP_SIZE + Read<std::int32_t>(a_at + 2));
			}
			if (Readable(a_at, 12) && Same(a_at, MOV_RAX) && (Same(a_at + 10, JMP_RAX) || Same(a_at + 10, PUSH_RAX_RET))) {
				return Read<std::uintptr_t>(a_at + 2);
			}
			if (Readable(a_at, 13) && Same(a_at, MOV_R11) && Same(a_at + 10, JMP_R11)) {
				return Read<std::uintptr_t>(a_at + 2);
			}
			if (Readable(a_at, 14) && opcode == PUSH_LOW && Same(a_at + 5, MOV_HIGH) && Read<std::uint8_t>(a_at + 13) == RET) {
				return static_cast<std::uintptr_t>(Read<std::uint32_t>(a_at + 9)) << 32 | Read<std::uint32_t>(a_at + 1);
			}
			return std::nullopt;
		}

		// The slot a hook that only hands on jumps through, 0 for any other
		// code: FF 25 or 48 FF 25 with disp32, or 48 8B 05 disp32 then jmp rax.
		std::uintptr_t PassSlot(std::uintptr_t a_at)
		{
			const auto at = Readable(a_at, 1) && Read<std::uint8_t>(a_at) == REX_W ? a_at + 1 : a_at;
			if (Readable(at, RIP_SIZE) && Read<std::uint8_t>(at) == OPCODE_GROUP_FF && Read<std::uint8_t>(at + 1) == MODRM_JMP_RIP) {
				return at + RIP_SIZE + Read<std::int32_t>(at + 2);
			}
			if (Readable(a_at, MOV_RIP_SIZE + 2) && Read<std::uint8_t>(a_at) == REX_W && Read<std::uint8_t>(a_at + 1) == MOV_LOAD &&
				Read<std::uint8_t>(a_at + 2) == MODRM_RIP && Same(a_at + MOV_RIP_SIZE, JMP_RAX)) {
				return a_at + MOV_RIP_SIZE + Read<std::int32_t>(a_at + 3);
			}
			return 0;
		}

		// Whether an address lies in a code section of a_image.
		bool InCode(const Image& a_image, std::uintptr_t a_at)
		{
			return (SectionOf(a_image, a_at) & REX::W32::IMAGE_SCN_MEM_EXECUTE) != 0;
		}

		// The slot a hook with no table entry hands on through: the first jump
		// of PassSlot's shape in its first MAX_BARE bytes, through a slot of
		// its own data, 0 for none. Such a hook calls nothing and moves no
		// stack, so it does a little work and then hands on with that jump.
		// The read stays in its code and stops where a function with an entry
		// starts.
		std::uintptr_t BareSlot(const Image& a_image, std::uintptr_t a_at)
		{
			for (auto at = a_at; at < a_at + MAX_BARE && InCode(a_image, at) && !FunctionOf(a_image, at); at++) {
				const auto slot = PassSlot(at);
				if (slot && (SectionOf(a_image, slot) & REX::W32::IMAGE_SCN_MEM_WRITE) != 0) {
					return slot;
				}
			}
			return 0;
		}

		// Whether a function of a_image starts at a_at: an entry of its table
		// starts there, or a hook with no entry hands on from there, see
		// BareSlot.
		bool StartsFunction(const Image& a_image, std::uintptr_t a_at)
		{
			if (const auto* function = FunctionOf(a_image, a_at)) {
				return a_image.base + function->begin == a_at;
			}
			return PassSlot(a_at) != 0 || BareSlot(a_image, a_at) != 0;
		}

		// Whether an address a DLL keeps is NEC's at this place: exactly what
		// NEC wrote there or its hook there. Any other address in NEC.dll is
		// NEC's at another place, which skips this one, see Skips.
		bool IsNecHere(std::uintptr_t a_kept, const Ends& a_ends)
		{
			return a_kept && (a_kept == a_ends.mine || (a_ends.hook && Follow(a_kept) == a_ends.hook));
		}

		// Whether an address a DLL keeps skips NEC: what NEC hands on to, the
		// game's own function, anything else in the game, or NEC's code for
		// another place. Asked once IsNecHere said no.
		bool Skips(std::uintptr_t a_kept, const Ends& a_ends)
		{
			const auto lands = Follow(a_kept);
			return a_kept && (a_kept == a_ends.game || InGame(lands) || InNec(lands) ||
								 (a_ends.next && (a_kept == a_ends.next || (lands && lands == Follow(a_ends.next)))));
		}

		// Within 2 gigabytes of a_where, as every address a call or a jump
		// reaches is: a stub or game code, never a DLL's function.
		bool InReach(std::uintptr_t a_address, std::uintptr_t a_where)
		{
			return (a_address > a_where ? a_address - a_where : a_where - a_address) < 0x80000000ULL;
		}

		// Whether an address lands in the code of a loaded module other than
		// a_image and Fallout4.exe, the next link of a line. A pointer to a
		// module's data, like an F4SE interface a hook keeps, is no link.
		bool LeadsOn(std::uintptr_t a_address, const Image& a_image)
		{
			const auto landing = Follow(a_address);
			if (!landing || InGame(landing) || ImageBase(landing) == a_image.base) {
				return false;
			}
			const auto image = ImageAt(landing);
			return image && InCode(image, landing);
		}

		// Whether an address lands at the start of a function of another
		// DLL's code. That is the only next link a lea's slot may hold.
		// NEC.dll is no such DLL, or the walk would take any function of NEC
		// for its hook here.
		bool LeadsOnToStart(std::uintptr_t a_address, const Image& a_image)
		{
			const auto landing = Follow(a_address);
			return LeadsOn(a_address, a_image) && !InNec(landing) && StartsFunction(ImageAt(landing), landing);
		}

		// The address the hook at a_at of a_image hands each call on to, read
		// from where it keeps it, 0 when NEC finds none. A hook that only hands
		// on, or one with no table entry, keeps it in the slot it jumps
		// through, see BareSlot. Any other hook is read for the slots of its
		// own data it calls through, loads from or takes the address of: NEC's
		// own address there wins, then one that leads on to another DLL's
		// code, then one that skips NEC. A lea's slot counts only for the
		// first 2, see LeadsOnToStart, since a hook takes the address of
		// plenty it never calls.
		std::uintptr_t KeptBy(const Image& a_image, std::uintptr_t a_at, const Ends& a_ends)
		{
			if (const auto slot = PassSlot(a_at)) {
				return Through(slot);
			}
			const auto* function = FunctionOf(a_image, a_at);
			if (!function) {
				return Through(BareSlot(a_image, a_at));
			}
			if (a_image.base + function->begin != a_at) {
				return 0;
			}
			const auto end = a_image.base + std::min(function->end, function->begin + MAX_SCAN);
			if (!Readable(a_at, end - a_at)) {
				return 0;
			}
			std::uintptr_t onward = 0;
			std::uintptr_t skips = 0;
			for (auto at = a_at; at + RIP_SIZE <= end; at++) {
				const auto     first = Read<std::uint8_t>(at);
				const auto     second = Read<std::uint8_t>(at + 1);
				std::uintptr_t slot = 0;
				bool           lea = false;
				if (first == OPCODE_GROUP_FF && (second == MODRM_CALL_RIP || second == MODRM_JMP_RIP)) {
					slot = at + RIP_SIZE + Read<std::int32_t>(at + 2);
				} else if ((first == REX_W || first == REX_WR) && (second == MOV_LOAD || second == LEA) && at + MOV_RIP_SIZE <= end &&
						   (Read<std::uint8_t>(at + 2) & MODRM_RIP_MASK) == MODRM_RIP) {
					slot = at + MOV_RIP_SIZE + Read<std::int32_t>(at + 3);
					lea = second == LEA;
				}
				if (!slot || (SectionOf(a_image, slot) & REX::W32::IMAGE_SCN_MEM_WRITE) == 0) {
					continue;
				}
				const auto kept = Through(slot);
				if (!kept || ImageBase(kept) == a_image.base) {
					continue;
				}
				if (IsNecHere(kept, a_ends)) {
					return kept;
				}
				if ((a_ends.kind == Kind::kCall || a_ends.kind == Kind::kJump) && !InReach(kept, a_ends.where)) {
					continue;
				}
				if (lea) {
					if (!onward && LeadsOnToStart(kept, a_image)) {
						onward = kept;
					}
					continue;
				}
				if (Skips(kept, a_ends)) {
					skips = skips ? skips : kept;
				} else if (!onward && LeadsOn(kept, a_image)) {
					onward = kept;
				}
			}
			return onward ? onward : skips;
		}

		// Follows a line of hooks from a_kept inward, link by link. A stub NEC
		// cannot follow ends it as another mod's.
		Walked WalkFrom(std::uintptr_t a_kept, const Ends& a_ends)
		{
			Walked walked;
			for (int i = 0; i < MAX_LINKS && a_kept; i++) {
				if (IsNecHere(a_kept, a_ends)) {
					walked.reaches = Reaches::kNec;
					return walked;
				}
				if (Skips(a_kept, a_ends)) {
					walked.reaches = Reaches::kGame;
					return walked;
				}
				const auto landing = Follow(a_kept);
				const auto image = landing ? ImageAt(landing) : Image{};
				// A hook that hands on into its own DLL's code is still 1 link of
				// that DLL, named once.
				Owner owner{ image ? ModuleOf(landing) : std::string{}, true };
				if (walked.dlls.empty() || walked.dlls.back() != owner) {
					walked.dlls.push_back(std::move(owner));
				}
				if (!image) {
					return walked;
				}
				a_kept = KeptBy(image, landing, a_ends);
			}
			return walked;
		}
	}

	// Where a_address lands once every stub in front of it is followed: the
	// first address in a module, or the first that is no stub. 0 when a stub
	// cannot be read, or past MAX_HOPS of them.
	std::uintptr_t Follow(std::uintptr_t a_address)
	{
		auto at = a_address;
		for (int i = 0; i <= MAX_HOPS; i++) {
			if (!at || !Readable(at, 1)) {
				return 0;
			}
			const auto next = Hop(at);
			if (!next) {
				return at;
			}
			at = *next;
		}
		return 0;
	}

	Found BuildableAt(const Ask& a_ask)
	{
		if (!Supported()) {
			return { .why = Why::kVersion };
		}
		if (!a_ask.handsOn) {
			return { .why = Why::kSkips };
		}

		// The same instruction the game has there, and for a call through a
		// vtable another DLL's FF 15.
		if (a_ask.kind == Kind::kCall || a_ask.kind == Kind::kJump) {
			const auto opcode = a_ask.kind == Kind::kJump ? OPCODE_JMP_REL32 : OPCODE_CALL_REL32;
			if (Read<std::uint8_t>(a_ask.where) != opcode) {
				return { .why = Why::kShape };
			}
		} else if (a_ask.kind == Kind::kVirtualCall &&
				   (Read<std::uint8_t>(a_ask.where) != OPCODE_GROUP_FF || Read<std::uint8_t>(a_ask.where + 1) != MODRM_CALL_RIP)) {
			return { .why = Why::kShape };
		}
		const auto entry = LeadsTo(a_ask.where, a_ask.kind);

		// A hook NEC can call as the game would: the start of a function in
		// the code of a loaded DLL. A stub NEC cannot read back is often
		// written into the middle of game code to read the caller's stack,
		// and under NEC its caller would be NEC's hook.
		const auto landing = Follow(entry);
		if (!landing) {
			return { .why = Why::kUnread };
		}
		if (InGame(landing)) {
			return { .why = Why::kGame };
		}
		const auto image = ImageAt(landing);
		if (!image) {
			return { .why = Why::kNowhere };
		}
		if (InNec(landing) || !InCode(image, landing) || !StartsFunction(image, landing)) {
			return { .why = Why::kMiddle };
		}

		Found found{ .next = entry };
		found.under = WalkFrom(entry, { a_ask.kind, a_ask.where, 0, 0, 0, a_ask.game }).dlls;
		if (found.under.empty()) {
			found.under.push_back({ ModuleOf(landing), true });
		}
		return found;
	}

	Walked Walk(const Place& a_place)
	{
		// What NEC wrote: a call to its stub, a call through its cell, or its
		// hook in a pointer.
		std::int32_t displacement = 0;
		std::memcpy(&displacement, a_place.wrote.data() + (a_place.kind == Kind::kVirtualCall ? 2 : 1), sizeof(displacement));
		const auto mine = a_place.kind == Kind::kPointer     ? a_place.hook :
		                  a_place.kind == Kind::kVirtualCall ? a_place.where + VCALL_SIZE + displacement :
		                                                       a_place.where + REL32_SIZE + displacement;
		return WalkFrom(LeadsTo(a_place.where, a_place.kind), { a_place.kind, a_place.where, mine, a_place.hook, a_place.next, a_place.game });
	}
}
