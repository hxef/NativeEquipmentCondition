#include "Core/CallPatch/Write.h"

#include <cstring>

namespace CallPatch
{
	// Fills a hook's Link as its place is written, see LinkBase.
	struct Fill
	{
		static void Into(LinkBase& a_link, const Place& a_place)
		{
			a_link._next = a_place.next;
			a_link._set = a_place.set;
			a_link._mark = a_place.mark;
		}
	};

	void WritePlace(Place& a_place)
	{
		// The Link first, so the hook hands on from its very first call.
		if (a_place.link) {
			Fill::Into(*a_place.link, a_place);
		}
		auto& trampoline = REL::GetTrampoline();
		if (a_place.kind == Kind::kCall) {
			trampoline.write_call<REL32_SIZE>(a_place.where, a_place.hook);
			a_place.size = REL32_SIZE;
		} else if (a_place.kind == Kind::kJump) {
			trampoline.write_jmp<REL32_SIZE>(a_place.where, a_place.hook);
			a_place.size = REL32_SIZE;
		} else if (a_place.kind == Kind::kVirtualCall) {
			trampoline.write_call<VCALL_SIZE>(a_place.where, a_place.hook);
			a_place.size = VCALL_SIZE;
		} else {
			REL::WriteSafeData(a_place.where, a_place.hook);
			a_place.size = sizeof(std::uintptr_t);
		}
		std::memcpy(a_place.wrote.data(), reinterpret_cast<const void*>(a_place.where), a_place.size);
		a_place.seen = a_place.wrote;
		a_place.watch = a_place.size;
		if (!a_place.under.empty()) {
			a_place.share = Share::kOnTop;
		}
	}
}
