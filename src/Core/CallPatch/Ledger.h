#pragma once

#include "Core/CallPatch/CallPatch.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// What the files of the folder share. Private to this folder.
namespace CallPatch
{
	// call rel32 and jmp rel32: 1 opcode byte and a 4 byte displacement counted
	// from the end of the instruction, 5 bytes in all.
	inline constexpr std::uint8_t OPCODE_CALL_REL32 = 0xE8;
	inline constexpr std::uint8_t OPCODE_JMP_REL32 = 0xE9;
	inline constexpr std::size_t  REL32_SIZE = 5;

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

	// FF 15 disp32 calls through the pointer at the end of the instruction
	// plus disp32, and FF 25 disp32 jumps through it, 6 bytes each.
	inline constexpr std::uint8_t MODRM_CALL_RIP = 0x15;
	inline constexpr std::uint8_t MODRM_JMP_RIP = 0x25;
	inline constexpr std::size_t  RIP_SIZE = 6;

	// Where a site's instruction sits in the running game.
	inline std::uintptr_t Address(const CallSite& a_site)
	{
		const REL::Relocation<std::uintptr_t> owner{ REL::ID(a_site.id) };
		return owner.address() + a_site.offset;
	}

	// How a place is written: a call or a jump of 5 bytes, a call through a
	// vtable of 6, or a pointer of 8.
	enum class Kind
	{
		kCall,
		kJump,
		kVirtualCall,
		kPointer,
	};

	// What losing a place costs, from its part: main for the row's own,
	// alone for any other, nothing from kTrace on.
	enum class Use
	{
		kMain,
		kDetail,
		kTraceOnly,
	};

	// What stands on a place, for the page and NEC.log. Never read by a hook.
	// NEC's hook runs at a written place while it is kOwn, kOnTop, kShared or
	// kProven. A free place NEC never wrote, as its set or row did not go in,
	// stays kOwn with size 0.
	enum class Share : std::uint8_t
	{
		kOwn,     // NEC's alone: nobody else was there, or it is back to NEC's bytes
		kOnTop,   // NEC runs on top of a DLL it found there at install
		kShared,  // a DLL hooked over NEC later and the call still reaches NEC
		kProven,  // kShared in a held set whose hook has run since
		kCut,     // a DLL took it later and skips NEC, a set of every call changed, or another place of its set was cut
		kLeft,    // taken when NEC noted it, in a way NEC cannot build on, so left alone
	};

	// Why NEC leaves alone a place another DLL changed first, see BuildableAt.
	enum class Why : std::uint8_t
	{
		kNone,     // NEC writes it
		kVersion,  // the game version is not supported
		kShape,    // its bytes are not the call the game has there
		kGame,     // it leads elsewhere in the game
		kNowhere,  // it leads into memory no mod owns
		kMiddle,   // it leads into the middle of a function
		kUnread,   // NEC cannot read where it leads
		kSkips,    // NEC's hook never hands on, so it would skip that DLL
		kTotal,
	};

	// One place a row asks for, as CallPatch.cpp read it.
	struct Ask
	{
		const char*    what;
		std::uintptr_t where;
		Kind           kind;
		std::uintptr_t hook;
		Part           part;            // kNone: the open Together's, else the row's
		bool           free;            // the game's own is there
		std::uintptr_t game = 0;        // the function the game calls at a call or a jump
		bool           handsOn = true;  // the hook hands every call on, see NEVER_HANDS_ON
		LinkBase*      link = nullptr;  // filled as the place is written
	};

	// Notes a place for the row whose turn it is and logs a clash. False when
	// NEC leaves it alone: taken, noted outside a turn or twice, or no part.
	bool Note(const Ask& a_ask);

	// Logs a line saying a_part's places went in. In a switched row a line of
	// its own part waits for End, dropped when the row yields, so a part left
	// to another mod never reads as patched.
	void SayPatched(Part a_part, std::string a_line);

	// A log line gathered under the lock and written after it. off names the
	// key of a switch turned off, and the line says why, see LogChange.
	struct Line
	{
		REX::ELogLevel   level;
		std::string      text;
		std::string_view off{};
	};

	using Lines = std::vector<Line>;

	// The ledger's lock, and Write, which writes gathered lines once the lock
	// is let go.
	[[nodiscard]] std::mutex& Lock();
	void                      Write(const Lines& a_lines);

	// Whether the game is 1.11.240, the version every place was read off.
	[[nodiscard]] bool Supported();

	// In Owners.cpp. Whether every owner is this game version. It reads only
	// the owners it is given, so it needs no lock.
	[[nodiscard]] bool OnlyVersion(std::span<const Owner> a_owners);

	// In Reach.cpp. Memory reads that touch no ledger state, so they take no
	// lock and need none, and CallPatch.cpp reads a place with them before
	// Note takes the lock. Read checks nothing: a place in Fallout4.exe is
	// read as is, any other address only once Readable finds a_size bytes
	// there on 1 committed page range that reads with no fault.
	[[nodiscard]] bool Readable(std::uintptr_t a_address, std::size_t a_size);

	template <class T>
	[[nodiscard]] T Read(std::uintptr_t a_address)
	{
		T value{};
		std::memcpy(&value, reinterpret_cast<const void*>(a_address), sizeof(T));
		return value;
	}
	// What the pointer at a_slot holds and where an address lands past the
	// stubs in front of a hook (Follow, in Walk.cpp), 0 when unreadable, the
	// file name of the DLL an address lies in (empty for Fallout4.exe, NEC.dll
	// and memory no module owns), and whether it lies in Fallout4.exe.
	[[nodiscard]] std::uintptr_t Through(std::uintptr_t a_slot);
	[[nodiscard]] std::uintptr_t Follow(std::uintptr_t a_address);
	[[nodiscard]] std::string    ModuleOf(std::uintptr_t a_address);
	[[nodiscard]] bool           InGame(std::uintptr_t a_address);

	// The ledger itself, kept in Ledger.cpp. Everything below is used only
	// while Lock() is held, and none of it takes the lock. About 100 places
	// are patched, each set holding 1 or more. Set 0 is none and never runs.
	inline constexpr std::size_t MAX_SETS = 512;
	inline constexpr std::size_t MAX_PLACES = 512;

	struct Place
	{
		const Feature*              row;  // whose Install noted it
		std::uint32_t               set;  // places that go in together share one
		Part                        part;
		Use                         use;
		const char*                 what;
		std::uintptr_t              where;
		Kind                        kind;
		std::uintptr_t              hook;
		std::uintptr_t              game;      // the function the game calls at a call or a jump, else 0
		LinkBase*                   link;      // the hook's, filled as the place is written
		bool                        free;      // NEC can write it: nobody else was there, or a plain hook
		std::uint16_t               mark = 0;  // its index in the ledger, for the proof
		std::uintptr_t              next = 0;  // what its hook hands on to
		Share                       share = Share::kOwn;
		Why                         why = Why::kNone;
		std::vector<Owner>          owners{};  // who has it while it is off, first found first
		std::vector<Owner>          under{};   // the DLLs NEC runs on top of, outermost first
		std::vector<Owner>          line{};    // the DLLs over NEC as the last walk found them
		std::array<std::uint8_t, 8> wrote{};
		std::array<std::uint8_t, 8> seen{};    // what the last read found there
		std::uint8_t                size = 0;  // bytes NEC wrote, 0 while it is not written
		std::uint8_t                watch = 0; // bytes a recheck reads back, set as NEC notes it
	};

	// Whether a place is off for good, so the page names who has it. A place
	// of a held set also counts as off on the page and in NEC.log while the
	// set waits, see Losses.cpp.
	[[nodiscard]] inline bool IsOff(const Place& a_place)
	{
		return a_place.share == Share::kCut || a_place.share == Share::kLeft;
	}

	// A switch left to another mod and the part it went off with.
	struct Yielded
	{
		const Settings::Live<bool>* on;
		Part                        cause;
	};

	// What 1 End or 1 recheck changed, said once it is over, so a part with
	// many places reads as 1 line naming every owner.
	struct Pass
	{
		struct Off
		{
			const Feature* row;
			Part           cause;
			bool           was;  // the switch was on
		};

		std::bitset<PART_COUNT> parts;  // parts that lost a place in it
		std::vector<Off>        offs;   // switches turned off in it
	};

	[[nodiscard]] std::vector<Place>& Places();

	// The record of a switch left to another mod, nullptr while it is free.
	[[nodiscard]] const Yielded* YieldLocked(const Settings::Live<bool>* a_switch);
	[[nodiscard]] bool           YieldedLocked(const Settings::Live<bool>* a_switch);

	// Its set stops for good, and the place is a_share, kCut or kLeft.
	void Lose(Place& a_place, Pass& a_pass, Share a_share);

	// In Sets.cpp. Whether a set's hooks run, see Held::Runs: a set waits
	// while a place of it changed and its hook has not run since. Mark notes a
	// Together's set, whose hooks ask its Held, and whether they run on every
	// call. Start runs a set as its places are written, Stop stops it for
	// good, Hold makes a place wait for its hook (false when its set is off),
	// Release ends the wait, and Proving says whether a place still waits.
	enum class Run { kOff, kLive, kWaiting };

	[[nodiscard]] Run  RunOf(std::uint32_t a_set);
	[[nodiscard]] bool HeldSet(std::uint32_t a_set);
	[[nodiscard]] bool EveryCall(std::uint32_t a_set);
	[[nodiscard]] bool Proving(const Place& a_place);
	void               Mark(std::uint32_t a_set, bool a_held, bool a_everyCall);
	void               Start(std::uint32_t a_set);
	void               Stop(std::uint32_t a_set);
	bool               Hold(const Place& a_place);
	void               Release(const Place& a_place);

	// In Losses.cpp. The lines of a pass, 1 a part and 1 a switch turned off.
	// a_install says the row's turn just ended, so nothing of a switched row
	// is written. PiecesOffTail ends the line of a place the call still
	// reaches when its pieces are all off for a cause outside its set, such
	// as a piece they need, and is empty otherwise. Another place of a piece
	// marked alone is no such cause, see PieceRow in Core/Pieces.h.
	void                      Say(const Pass& a_pass, Lines& a_lines, bool a_install);
	[[nodiscard]] std::string PiecesOffTail(const Place& a_place);

	// In Owners.cpp. Who has a place, read from what it leads to now, sure to
	// be a mod when a_changed says it changed since NEC noted it. Then the
	// lines for a place left alone and one NEC runs on top of, the English
	// name of a place's part (its row's own for a trace only place) and the
	// sentence that ends a trace only place's line.
	[[nodiscard]] Owner            OwnerAt(std::uintptr_t a_where, Kind a_kind, bool a_changed);
	[[nodiscard]] std::string      ClashLine(const Place& a_place);
	[[nodiscard]] std::string      OnTopLine(const Place& a_place);
	[[nodiscard]] std::string_view NameOf(const Place& a_place);
	[[nodiscard]] std::string_view TraceTail(const Place& a_place);

	// The DLLs of a line of hooks, outermost first, "B.dll over A.dll", what a
	// place leads to now, whether that is into Fallout4.exe, and the bytes a
	// place of a kind takes, 5, 6 or 8.
	[[nodiscard]] std::string    Over(std::span<const Owner> a_owners);
	[[nodiscard]] std::uintptr_t LeadsTo(std::uintptr_t a_where, Kind a_kind);
	[[nodiscard]] bool           LeadsIntoGame(std::uintptr_t a_where, Kind a_kind);
	[[nodiscard]] std::uint8_t   SizeOf(Kind a_kind);

	// In Walk.cpp. BuildableAt gives what NEC found at a place another DLL
	// changed first: what its hook hands on to and the DLLs it runs on top
	// of, or why NEC leaves it alone. Walk follows the hooks on a written
	// place from the outermost in, naming every DLL it passed.
	struct Found
	{
		std::uintptr_t     next = 0;
		Why                why = Why::kNone;
		std::vector<Owner> under{};
	};
	enum class Reaches { kNec, kGame, kUnread };
	struct Walked
	{
		Reaches            reaches = Reaches::kUnread;
		std::vector<Owner> dlls{};
	};
	[[nodiscard]] Found  BuildableAt(const Ask& a_ask);
	[[nodiscard]] Walked Walk(const Place& a_place);
}
