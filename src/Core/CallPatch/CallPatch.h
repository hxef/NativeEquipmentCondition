#pragma once

#include "Core/CallPatch/Link.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Settings.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct Feature;  // Core/Feature.h

// Patching the game's code: a call, or a pointer in a function table. Each
// place is checked first, written when its feature's install ends, built on
// top when another DLL left a plain hook, and left to a mod whose change NEC
// cannot build on. NEC writes every place once, at install, and never again.
//
// A call is redirected at the 1 call instruction instead of the function it
// points at. A detour rewrites the function's first bytes, so every caller
// lands in the hook. This patches the 1 call that matters and leaves the rest
// alone, which is what the condition scaling needs: the calls combat makes,
// not the ones the save system makes.
//
// CallPatch.cpp reads each place, Ledger.cpp keeps every place and writes
// them, Sets.cpp says whether a set's hooks run, Walk.cpp follows the hooks on
// a place and judges what another DLL left, with the memory reads of
// Reach.cpp, Recheck.cpp reads every place back as the game runs, Owners.cpp
// names who has a place, Losses.cpp works out which pieces are off and to
// whom, and Summary.cpp writes NEC.log's summary of it.
namespace CallPatch
{
	// A call site: the Address Library ID of the function around it plus the
	// offset of the call instruction inside, read off the 1.11.240 build. The
	// library maps a stable ID to the function's address in any build, but not
	// offsets inside it, so every site is checked before writing. what names
	// the site in the log, short because it is on every trace line.
	struct CallSite
	{
		std::uint64_t id;
		std::size_t   offset;
		const char*   what;
		bool          jump = false;  // the game has a tail jump here, not a call
	};

	// Each place belongs to a part, see Core/Parts.h: the open Together's,
	// else the one its patch call names, else its row's. Losing a place of
	// the row's own part in a row with a switch leaves the whole row to the
	// other mod and turns the switch off. A place of any other part goes
	// alone and the rest of the row works. A place from Part::kTrace on
	// stays in whatever its row's switch says and never takes anything. In a
	// row with no switch every set stands alone.

	class Together;

	// A set of places that only works whole. Its hooks ask on every call and
	// give the game's own result while it does not run: before the set is
	// written, when it never is, once a recheck finds one of its places taken,
	// and while it waits to see a hook of a changed place run, see Runs.
	class Held
	{
	public:
		Held() = default;

		// False when one of its places was taken as NEC noted it.
		explicit operator bool() const { return set != 0; }

		// 1 relaxed atomic load.
		[[nodiscard]] bool Intact() const;

		// Intact, for a hook of the set handing on through a_link. While the
		// set waits it first marks that a_link's hook ran, see Sets.cpp, so
		// it pays the same 1 load while the set runs.
		[[nodiscard]] bool Runs(const LinkBase& a_link) const;

	private:
		friend class Together;

		explicit Held(std::uint32_t a_set) :
			set(a_set)
		{}

		std::uint32_t set = 0;
	};

	// Each of these notes its place for the row whose turn it is, see Begin,
	// and returns whether NEC can write it: nobody else was there, or another
	// DLL left a plain hook that NEC runs on top of. End still leaves it
	// unwritten when another place of its set, or of a switched row's own
	// part, is taken. a_part names a part other than the row's own, see above.

	// Points one call at a hook, which hands each call on through a_link. A
	// site can be a jump: a call that is the last thing a function does is
	// often compiled as a jump into the callee, a tail call. Such a site is
	// repointed with a jump, and the hook returns what the callee would have.
	bool PatchCall(const CallSite& a_site, std::uintptr_t a_expected, std::uintptr_t a_hook, LinkBase& a_link,
		Part a_part = Part::kNone);

	// PatchCall for a call through a vtable. The slot stands in for the
	// expected target. While a_link is empty the hook makes the virtual call
	// itself, else it calls a_link, the hook of the DLL NEC runs on top of.
	bool PatchVirtualCall(const CallSite& a_site, std::size_t a_slot, std::uintptr_t a_hook, LinkBase& a_link,
		Part a_part = Part::kNone);

	// Patches every site in a list that calls a_target, site i with hook i
	// handing on through link i, and logs how many go in, once the row does.
	std::size_t PatchSites(std::span<const CallSite> a_sites, REL::ID a_target, std::span<const std::uintptr_t> a_hooks,
		std::span<LinkBase* const> a_links, const char* a_what, Part a_part);

	// PatchSites for an array or a span of Links, 1 a site.
	template <class Links>
	std::size_t PatchAll(std::span<const CallSite> a_sites, REL::ID a_target, std::span<const std::uintptr_t> a_hooks,
		Links&& a_links, const char* a_what, Part a_part = Part::kNone)
	{
		std::vector<LinkBase*> links;
		for (auto& link : a_links) {
			links.push_back(&link);
		}
		return PatchSites(a_sites, a_target, a_hooks, links, a_what, a_part);
	}

	// One call of a PatchTogether set: the site, what it calls, the hook, its Link.
	struct Patch
	{
		CallSite       site;
		std::uintptr_t expected;
		std::uintptr_t hook;
		LinkBase*      link;
	};

	// Patches calls that only work together, all or none: a hook that reads
	// what another hook noted, or 2 results compared with each other.
	Held PatchTogether(std::initializer_list<Patch> a_patches, Part a_part = Part::kNone);

	// The a_handsOn of a hook that never calls what it found while its set
	// runs. NEC never runs one on top of another DLL's, which it would skip.
	inline constexpr bool NEVER_HANDS_ON = false;

	// Points a pointer the game calls through at a hook: a function table slot
	// or a console command's function. It must be empty, lead into Fallout4.exe
	// or hold a plain hook of another DLL, which the hook hands on to. Returns
	// the original the hook calls, 0 for an empty place, or nothing where NEC
	// cannot write it, see above. a_link is filled the same way.
	std::optional<std::uintptr_t> PatchPointer(std::uintptr_t a_where, std::uintptr_t a_hook, const char* a_what,
		Part a_part = Part::kNone, bool a_handsOn = true, LinkBase* a_link = nullptr);

	// a_link is filled so the hook can ask its set, see LinkBase::Live, though
	// the hook hands on through the pointer this returns, not through it.
	template <class F>
	std::optional<std::uintptr_t> PatchSlot(const REL::Relocation<std::uintptr_t>& a_table, std::size_t a_index, F a_hook,
		const char* a_what, Part a_part = Part::kNone, bool a_handsOn = true, LinkBase* a_link = nullptr)
	{
		return PatchPointer(a_table.address() + a_index * sizeof(std::uintptr_t),
			REX::UNRESTRICTED_CAST<std::uintptr_t>(a_hook), a_what, a_part, a_handsOn, a_link);
	}

	// The a_everyCall of a Together whose hooks must run on every call, so any
	// change of one of its places after install turns it off for good.
	inline constexpr bool EVERY_CALL = true;

	// Every place noted while one lives goes in whole or not at all, belongs
	// to a_part and shares one Held. One opened inside another joins it.
	class Together
	{
	public:
		explicit Together(Part a_part = Part::kNone, bool a_everyCall = false);
		~Together();
		Together(const Together&) = delete;
		Together& operator=(const Together&) = delete;

		[[nodiscard]] Held Set() const;

	private:
		std::uint32_t set;
	};

	// main.cpp's Install walk. Begin opens a row's turn, End writes what the
	// row noted by the rule in Ledger.cpp, and a place noted outside a turn is
	// left alone. SayOnTop logs each place NEC runs on top of once every row
	// is in, since a later row can leave its pieces off.
	void Begin(const Feature& a_row);
	void End();
	void SayOnTop();

	// The game version F4SE reports, handed over as the plugin loads. On any
	// version but 1.11.240 every function table slot counts as taken, since
	// slot numbers can move between versions.
	void SetGameVersion(const REL::Version& a_version);

	// That version as text, such as 1.10.984.
	[[nodiscard]] std::string GameVersion();

	// A DLL found on a place: one NEC left the place to, one NEC runs on top
	// of, or one hooked over NEC. dll is a file name or empty. sure says it is
	// a mod: the game is 1.11.240, or the place leads outside Fallout4.exe, or
	// it changed since NEC wrote it or found it taken. Not sure means the game
	// version is not supported.
	struct Owner
	{
		std::string dll;
		bool        sure = false;

		bool operator==(const Owner&) const = default;
	};

	// The order owners are listed in: named DLLs by name, then another mod,
	// then this game version.
	[[nodiscard]] bool Before(const Owner& a_lhs, const Owner& a_rhs);

	// The owners as a list to print: each named DLL, then a_unnamed once for
	// any owner NEC cannot name. Empty when every owner is this game version.
	[[nodiscard]] std::vector<std::string> OwnerNames(std::span<const Owner> a_owners, std::string_view a_unnamed);

	// Whether a switch is left to another mod while the game runs. Asked by
	// the walks, FireRate and the MCM page, never by a hot hook.
	[[nodiscard]] bool IsYielded(const Settings::Live<bool>& a_switch);

	// Why pieces of a part are off, see Core/Pieces.h: the part whose places
	// were taken, the part itself or the part of a piece they need, as Jamming
	// goes with Gun wear from firing, and that part's pieces it lost.
	struct Cause
	{
		Part               part = Part::kNone;
		std::vector<Piece> pieces;  // the shown pieces of the lost part off for this cause
		std::vector<Piece> taken;
		std::vector<Owner> owners;  // each once, in the order of Before
	};

	// A part another mod took some of: 1 cause, or 1 for each where its pieces
	// went to 2 mods or off for 2 reasons, and its shown pieces still working.
	struct Loss
	{
		Part               part = Part::kNone;
		std::vector<Cause> causes;
		std::vector<Piece> works;
	};

	// The part's loss, nothing while all of it is NEC's. A trace only place
	// never counts, nor a piece off only as the switch it rides on is left to
	// another mod. Each takes the lock, so no hook asks one every hit or frame.
	[[nodiscard]] std::optional<Loss> LossOf(Part a_part);

	// Every part another mod took some of, in Features.cpp's order.
	[[nodiscard]] std::vector<Loss> Losses();

	// The loss that turned a switch off, nothing while it is free.
	[[nodiscard]] std::optional<Loss> YieldOf(const Settings::Live<bool>& a_switch);

	// What a setting still does, see SettingLink in Core/Parts.h: its core and
	// side pieces off, its core pieces working and the one no mod can take.
	struct Effect
	{
		std::vector<Piece> off;
		std::vector<Piece> works;
		bool               idle = false;  // the player has its switch, or the one it sits under, off, so it gets no line or mark

		[[nodiscard]] bool None() const { return !off.empty() && works.empty(); }
	};

	[[nodiscard]] Effect EffectOf(const Settings::Named& a_setting);

	// The log line of a switch left to another mod, "Slower fire when worn:
	// off, left to NECClashTest.dll.", empty while it is free.
	[[nodiscard]] std::string OffLine(const Settings::Live<bool>& a_switch);

	// The NEC.log line of every part whose own places another mod took:
	// "Left to other mods: CND on item cards (containers and traders,
	// NECClashTest.dll), Workbench repairs (NECClashTest.dll).", then the
	// parts only this game version keeps off, "Off on game version 1.10.984,
	// which is not supported: HUD condition bars." Empty when there are none.
	[[nodiscard]] std::string LeftLine();

	// The NEC.log line of every part NEC shares a place of with another DLL,
	// which still works: "Shared with other mods: Slower fire when worn
	// (NECClashTest.dll)." Empty while NEC shares nothing.
	[[nodiscard]] std::string SharedLine();

	// NEC.log's summary as it reads now, its Left, Shared and setting lines
	// that say something. KeepSummary keeps it as the last one written, true
	// when it reads differently from the one kept before.
	[[nodiscard]] std::vector<std::string> Summary();
	bool                                   KeepSummary(const std::vector<std::string>& a_summary);

	// Owners in a log line: "A.dll and B.dll", "another mod", or "this game
	// version (1.10.984), which is not supported" when every one is that.
	[[nodiscard]] std::string Words(std::span<const Owner> a_owners);

	// Reads back every place NEC noted, written or left alone, and follows the
	// hooks of one another DLL changed since. While the call still reaches NEC
	// the place is shared, except in a set that has to run on every call, see
	// EVERY_CALL. Else NEC's change there is off from now on, its pieces go to
	// that DLL, and NEC.log says which. A trace only place is logged and takes
	// nothing. a_moment says when, for the log. True when the Summary changes.
	bool Recheck(std::string_view a_moment);
}
