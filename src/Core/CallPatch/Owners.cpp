#include "Core/CallPatch/Ledger.h"

#include "Core/Feature.h"
#include "Core/Text/Text.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <iterator>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Who has a place, and how NEC.log names them. A place leads somewhere: a call
// to its target, a pointer to what it holds. Another mod's hook often sits
// behind a stub or 2, so the address is followed through them, and the module
// it lands in is the owner, see Walk.cpp.
namespace CallPatch
{
	namespace
	{
		// The game version F4SE reports, see SetGameVersion.
		REL::Version g_version;

		// Why NEC leaves a place alone, as a clause of the clash line, in the
		// order of Why. The unsupported version has lines of its own.
		constexpr std::string_view REASONS[]{
			""sv,
			""sv,
			"and its bytes are not the call the game has there"sv,
			"and it leads elsewhere in the game"sv,
			"and it leads into memory no mod owns"sv,
			"and it leads into the middle of a function"sv,
			"and NEC cannot read where it leads"sv,
			"and NEC's change there would skip that mod"sv,
		};
		static_assert(std::size(REASONS) == static_cast<std::size_t>(Why::kTotal));

		// An owner in a line: the DLL's file name, or another mod.
		std::string_view Named(const Owner& a_owner)
		{
			return a_owner.dll.empty() ? "another mod"sv : std::string_view{ a_owner.dll };
		}
	}

	void SetGameVersion(const REL::Version& a_version)
	{
		g_version = a_version;
	}

	std::string GameVersion()
	{
		return std::format("{:d}.{:d}.{:d}", g_version.major(), g_version.minor(), g_version.patch());
	}

	bool Supported()
	{
		return g_version == F4SE::RUNTIME_1_11_240;
	}

	std::uintptr_t LeadsTo(std::uintptr_t a_where, Kind a_kind)
	{
		if (a_kind == Kind::kPointer) {
			return Read<std::uintptr_t>(a_where);
		}
		const auto opcode = Read<std::uint8_t>(a_where);
		if (opcode == OPCODE_CALL_REL32 || opcode == OPCODE_JMP_REL32) {
			return a_where + REL32_SIZE + Read<std::int32_t>(a_where + 1);
		}
		const auto modrm = Read<std::uint8_t>(a_where + 1);
		if (opcode == OPCODE_GROUP_FF && (modrm == MODRM_CALL_RIP || modrm == MODRM_JMP_RIP)) {
			return Through(a_where + RIP_SIZE + Read<std::int32_t>(a_where + 2));
		}
		return 0;
	}

	Owner OwnerAt(std::uintptr_t a_where, Kind a_kind, bool a_changed)
	{
		const auto target = LeadsTo(a_where, a_kind);
		const auto lands = Follow(target);

		Owner owner;
		owner.dll = lands ? ModuleOf(lands) : std::string{};
		owner.sure = Supported() || a_changed || (lands && !InGame(lands));
		return owner;
	}

	bool LeadsIntoGame(std::uintptr_t a_where, Kind a_kind)
	{
		// The game's own call through a vtable, which leads into the game
		// through the object's table.
		if (a_kind == Kind::kVirtualCall && Read<std::uint8_t>(a_where) == OPCODE_GROUP_FF &&
			(Read<std::uint8_t>(a_where + 1) & MODRM_MOD_REG_MASK) == MODRM_CALL_DISP32) {
			return true;
		}
		const auto target = LeadsTo(a_where, a_kind);
		return InGame(Follow(target));
	}

	std::uint8_t SizeOf(Kind a_kind)
	{
		const auto size = a_kind == Kind::kPointer ? sizeof(std::uintptr_t) : a_kind == Kind::kVirtualCall ? VCALL_SIZE :
		                                                                                                     REL32_SIZE;
		return static_cast<std::uint8_t>(size);
	}

	std::string_view NameOf(const Place& a_place)
	{
		return Text::PartLogName(a_place.use == Use::kTraceOnly ? a_place.row->part : a_place.part);
	}

	std::string_view TraceTail(const Place& a_place)
	{
		if (a_place.use != Use::kTraceOnly) {
			return ""sv;
		}
		if (a_place.part != Part::kBenchMessages) {
			return " It only feeds the bug report logs, so play is not affected."sv;
		}
		// The messages come from NEC's hook, so a place NEC never wrote has
		// none.
		return a_place.size == 0 ? " NEC's 2 messages when MODIFY opens nothing stay off, so repairs are not affected."sv :
		                           " It only shows NEC's 2 messages when MODIFY opens nothing, so repairs are not affected."sv;
	}

	std::string ClashLine(const Place& a_place)
	{
		const auto& owner = a_place.owners.front();
		std::string line;
		if (!owner.sure) {
			line = std::format("{:s}: {:s} at {:X} is not what NEC expects on game version {:s}, which is not supported, so NEC leaves it alone.",
				NameOf(a_place), a_place.what, a_place.where, GameVersion());
		} else if (a_place.why == Why::kVersion) {
			line = std::format("{:s}: {:s} at {:X} is already changed by {:s}, so NEC leaves it alone.", NameOf(a_place), a_place.what,
				a_place.where, Named(owner));
		} else {
			line = std::format("{:s}: {:s} at {:X} is already changed by {:s}, {:s}, so NEC leaves it alone.", NameOf(a_place),
				a_place.what, a_place.where, Named(owner), REASONS[static_cast<std::size_t>(a_place.why)]);
		}
		line += TraceTail(a_place);
		return line;
	}

	std::string OnTopLine(const Place& a_place)
	{
		const auto many = a_place.under.size() > 1;
		const auto off = PiecesOffTail(a_place);
		return std::format("{:s}: {:s} at {:X} is already changed by {:s}, so NEC runs on top of {:s} and hands each call on.{:s}{:s}",
			NameOf(a_place), a_place.what, a_place.where, Over(a_place.under), many ? "them" : "it",
			!off.empty() ? std::string_view{ off } : many ? " All work."sv : " Both work."sv, off.empty() ? TraceTail(a_place) : ""sv);
	}

	// Until a recheck the places on top are exactly the ones install wrote
	// that way, in the order they were noted.
	void SayOnTop()
	{
		Lines lines;
		{
			const std::scoped_lock l{ Lock() };
			for (const auto& place : Places()) {
				if (place.share == Share::kOnTop) {
					lines.push_back({ REX::ELogLevel::Info, OnTopLine(place) });
				}
			}
		}
		Write(lines);
	}

	std::string Over(std::span<const Owner> a_owners)
	{
		if (a_owners.empty()) {
			return "another mod";
		}
		std::string words{ Named(a_owners.front()) };
		for (const auto& owner : a_owners.subspan(1)) {
			std::format_to(std::back_inserter(words), " over {:s}", Named(owner));
		}
		return words;
	}

	bool Before(const Owner& a_lhs, const Owner& a_rhs)
	{
		// 0 for a named DLL, 1 for another mod, 2 for this game version.
		const auto rank = [](const Owner& a_owner) { return !a_owner.dll.empty() ? 0 : a_owner.sure ? 1 : 2; };
		if (rank(a_lhs) != rank(a_rhs)) {
			return rank(a_lhs) < rank(a_rhs);
		}
		// By name in any case, as Windows reads file names.
		return std::ranges::lexicographical_compare(a_lhs.dll, a_rhs.dll, [](char a_left, char a_right) {
			return std::tolower(static_cast<unsigned char>(a_left)) < std::tolower(static_cast<unsigned char>(a_right));
		});
	}

	std::vector<std::string> OwnerNames(std::span<const Owner> a_owners, std::string_view a_unnamed)
	{
		std::vector<std::string> names;
		bool                     unnamed = false;
		for (const auto& owner : a_owners) {
			if (!owner.dll.empty()) {
				names.push_back(owner.dll);
			} else {
				unnamed = unnamed || owner.sure;
			}
		}
		if (unnamed) {
			names.emplace_back(a_unnamed);
		}
		return names;
	}

	bool OnlyVersion(std::span<const Owner> a_owners)
	{
		return !a_owners.empty() && std::ranges::none_of(a_owners, &Owner::sure);
	}

	std::string Words(std::span<const Owner> a_owners)
	{
		const auto names = OwnerNames(a_owners, "another mod");
		if (names.empty()) {
			const auto version = std::ranges::any_of(a_owners, [](const Owner& a_owner) { return !a_owner.sure; });
			return version ? std::format("this game version ({:s}), which is not supported", GameVersion()) : std::string{ "another mod" };
		}
		// "A", "A and B", "A, B and C".
		std::string words = names.front();
		for (std::size_t i = 1; i < names.size(); i++) {
			words += i + 1 == names.size() ? " and " : ", ";
			words += names[i];
		}
		return words;
	}
}
