#include "Core/TraceHold.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace TraceLog
{
	namespace
	{
		// A line of the Papyrus file that waits while its thread holds, see
		// Papyrus::Hold.
		struct Held
		{
			bool        begin{ false };
			std::string tag;
			std::string text;
		};

		thread_local bool              t_holding{ false };
		thread_local std::vector<Held> t_held;
	}

	namespace detail
	{
		bool Kept(detail::Target a_target, bool a_begin, std::string_view a_tag, std::string_view a_text)
		{
			if (a_target != detail::Target::kPapyrus || !t_holding) {
				return false;
			}
			t_held.push_back({ a_begin, std::string(a_tag), std::string(a_text) });
			return true;
		}
	}

	void Papyrus::Hold()
	{
		t_held.clear();
		t_holding = true;
	}

	void Papyrus::Release(std::size_t a_lines)
	{
		t_holding = false;
		auto held = std::move(t_held);
		t_held.clear();
		for (std::size_t i = 0; i < held.size() && i < a_lines; ++i) {
			if (held[i].begin) {
				detail::Begin(detail::Target::kPapyrus, held[i].tag, held[i].text);
			} else {
				detail::Line(detail::Target::kPapyrus, held[i].tag, held[i].text);
			}
		}
	}
}
