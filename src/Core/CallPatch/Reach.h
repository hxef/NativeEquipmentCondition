#pragma once

#include "Core/CallPatch/Ledger.h"

#include <cstdint>

// Reading a loaded module's image: its code sections and its function table,
// the pieces the walk needs to tell a hook's start from the middle of a
// function and to name the DLL an address lies in. Private to this folder.
namespace CallPatch
{
	// An entry of an image's function table, by offset from the image: where
	// a function starts and ends.
	struct Function
	{
		std::uint32_t begin;
		std::uint32_t end;
		std::uint32_t unwind;
	};

	// A loaded module's image, its headers read and checked.
	struct Image
	{
		std::uintptr_t                      base = 0;
		const REX::W32::IMAGE_NT_HEADERS64* headers = nullptr;

		explicit operator bool() const { return headers != nullptr; }
	};

	// The start of the module image an address lies in, 0 for memory no image
	// covers, whether an address lies in NEC.dll, the loaded module an address
	// lies in (empty when Windows does not name one), the flags of the section
	// it lies in (0 outside every section), and the entry of the image's
	// function table it lies in (nullptr outside every function).
	[[nodiscard]] std::uintptr_t  ImageBase(std::uintptr_t a_address);
	[[nodiscard]] bool            InNec(std::uintptr_t a_address);
	[[nodiscard]] Image           ImageAt(std::uintptr_t a_address);
	[[nodiscard]] std::uint32_t   SectionOf(const Image& a_image, std::uintptr_t a_address);
	[[nodiscard]] const Function* FunctionOf(const Image& a_image, std::uintptr_t a_address);
}
