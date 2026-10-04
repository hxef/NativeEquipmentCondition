#include "Core/CallPatch/Reach.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// The low memory and module reads the walk is built on. Readable, see
// Ledger.h, keeps every read off a guarded page, and a module's image is read
// only once Windows names it a loaded module, so a page a DLL guards or frees
// is never touched. An 8 byte aligned read on x64 is whole, so reading
// another DLL's data races nothing fatal.
namespace CallPatch
{
	namespace
	{
		// REX's KERNEL32.h names no PAGE_GUARD, the flag of a page that faults
		// on its first touch, no MEM_IMAGE, the memory of a module's image, and
		// no set of readable page protections.
		constexpr std::uint32_t PAGE_GUARD = 0x100;
		constexpr std::uint32_t READABLE = 0x02 | 0x04 | 0x08 | 0x20 | 0x40 | 0x80;
		constexpr std::uint32_t MEM_IMAGE = 0x1000000;
	}

	bool Readable(std::uintptr_t a_address, std::size_t a_size)
	{
		REX::W32::MEMORY_BASIC_INFORMATION info{};
		if (!a_address || !REX::W32::VirtualQuery(reinterpret_cast<const void*>(a_address), &info, sizeof(info))) {
			return false;
		}
		if (info.state != REX::W32::MEM_COMMIT || (info.protect & PAGE_GUARD) != 0 || (info.protect & READABLE) == 0) {
			return false;
		}
		return a_address + a_size <= reinterpret_cast<std::uintptr_t>(info.baseAddress) + info.regionSize;
	}

	std::uintptr_t Through(std::uintptr_t a_slot)
	{
		return Readable(a_slot, sizeof(std::uintptr_t)) ? Read<std::uintptr_t>(a_slot) : 0;
	}

	std::uintptr_t ImageBase(std::uintptr_t a_address)
	{
		REX::W32::MEMORY_BASIC_INFORMATION info{};
		if (!a_address || !REX::W32::VirtualQuery(reinterpret_cast<const void*>(a_address), &info, sizeof(info)) || info.type != MEM_IMAGE) {
			return 0;
		}
		return reinterpret_cast<std::uintptr_t>(info.allocationBase);
	}

	bool InGame(std::uintptr_t a_address)
	{
		return a_address && ImageBase(a_address) == REX::FModule::GetExecutingModule().GetBaseAddress();
	}

	bool InNec(std::uintptr_t a_address)
	{
		return a_address && ImageBase(a_address) == REX::FModule::GetCurrentModule().GetBaseAddress();
	}

	std::string ModuleOf(std::uintptr_t a_address)
	{
		const auto base = ImageBase(a_address);
		if (!base || base == REX::FModule::GetExecutingModule().GetBaseAddress() || base == REX::FModule::GetCurrentModule().GetBaseAddress()) {
			return {};
		}
		wchar_t    path[REX::W32::MAX_PATH]{};
		const auto length = REX::W32::GetModuleFileNameW(reinterpret_cast<REX::W32::HMODULE>(base), path, REX::W32::MAX_PATH);
		if (length == 0) {
			return {};
		}
		const std::wstring_view full{ path, length };
		const auto              slash = full.find_last_of(L"\\/");
		std::string             name;
		if (!REX::UTF16_TO_UTF8(slash == std::wstring_view::npos ? full : full.substr(slash + 1), name)) {
			return {};
		}
		return name;
	}

	Image ImageAt(std::uintptr_t a_address)
	{
		const auto base = ImageBase(a_address);
		wchar_t    path[REX::W32::MAX_PATH]{};
		if (!base || REX::W32::GetModuleFileNameW(reinterpret_cast<REX::W32::HMODULE>(base), path, REX::W32::MAX_PATH) == 0 ||
			!Readable(base, sizeof(REX::W32::IMAGE_DOS_HEADER))) {
			return {};
		}
		const auto* dos = reinterpret_cast<const REX::W32::IMAGE_DOS_HEADER*>(base);
		const auto  at = base + static_cast<std::uintptr_t>(dos->lfanew);
		if (dos->magic != REX::W32::IMAGE_DOS_SIGNATURE || !Readable(at, sizeof(REX::W32::IMAGE_NT_HEADERS64))) {
			return {};
		}
		const auto* headers = reinterpret_cast<const REX::W32::IMAGE_NT_HEADERS64*>(at);
		const auto  sections = reinterpret_cast<std::uintptr_t>(REX::W32::IMAGE_FIRST_SECTION(headers));
		if (headers->signature != REX::W32::IMAGE_NT_SIGNATURE || headers->optionalHeader.magic != REX::W32::IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
			!Readable(sections, headers->fileHeader.sectionCount * sizeof(REX::W32::IMAGE_SECTION_HEADER))) {
			return {};
		}
		return { base, headers };
	}

	std::uint32_t SectionOf(const Image& a_image, std::uintptr_t a_address)
	{
		const auto  offset = a_address - a_image.base;
		const auto* sections = REX::W32::IMAGE_FIRST_SECTION(a_image.headers);
		for (std::uint16_t i = 0; i < a_image.headers->fileHeader.sectionCount; i++) {
			if (a_address >= a_image.base && offset >= sections[i].virtualAddress && offset < sections[i].virtualAddress + sections[i].virtualSize) {
				return sections[i].characteristics;
			}
		}
		return 0;
	}

	const Function* FunctionOf(const Image& a_image, std::uintptr_t a_address)
	{
		const auto& table = a_image.headers->optionalHeader.dataDirectory[REX::W32::IMAGE_DIRECTORY_ENTRY_EXCEPTION];
		const auto  start = a_image.base + table.virtualAddress;
		const auto  count = table.size / sizeof(Function);
		if (a_address < a_image.base || a_address - a_image.base >= a_image.headers->optionalHeader.imageSize || !table.virtualAddress ||
			count == 0 || !Readable(start, count * sizeof(Function))) {
			return nullptr;
		}

		// The table is sorted by where each function starts, so the one before
		// the first that starts past the address is the one that holds it.
		const std::span functions{ reinterpret_cast<const Function*>(start), count };
		const auto      offset = static_cast<std::uint32_t>(a_address - a_image.base);
		const auto      after = std::ranges::upper_bound(functions, offset, {}, &Function::begin);
		if (after == functions.begin() || offset >= std::prev(after)->end) {
			return nullptr;
		}
		return &*std::prev(after);
	}
}
