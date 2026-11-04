#include "Gameplay/Scrapping/Scrapping.h"

#include "Condition/Materials/Yield.h"
#include "Core/TraceLog.h"

namespace Scrapping
{
	void Install()
	{
		InstallBench();
		InstallWorkshop();
	}

	std::uint32_t Count(const CountLink& a_link, const char* a_where, const RE::TESBoundObject& a_item, std::uint32_t a_percent,
		RE::TESForm* a_form, std::uint32_t a_count, bool a_applyBase, bool a_skipScalar)
	{
		const auto in = Materials::ScrapIn(a_count, a_percent);
		const auto out = a_link(a_form, in, a_applyBase, a_skipScalar);
		TraceLog::Line("scrap", "{:s} at {:d}%  {:s} {:d} -> {:d}, gives {:d}  {:s}", TraceLog::Who{ &a_item }, a_percent,
			TraceLog::Who{ a_form }, a_count, in, out, a_where);
		return out;
	}
}
