#include "Gameplay/Scrapping/Scrapping.h"

#include "Condition/Materials/Yield.h"
#include "Core/TraceLog.h"

namespace Scrapping
{
	namespace
	{
		// The newest list being built on this thread.
		thread_local Tally* t_tally = nullptr;
	}

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
		if (t_tally) {
			t_tally->Add(a_item, a_percent, { a_form, a_count, out });
		}
		return out;
	}

	Tally::Tally() :
		outer(t_tally)
	{
		t_tally = this;
	}

	Tally::~Tally()
	{
		t_tally = outer;
	}

	void Tally::Add(const RE::TESBoundObject& a_item, std::uint32_t a_percent, const Materials::Tallied& a_count)
	{
		item = &a_item;
		percent = a_percent;
		counts.push_back(a_count);
	}

	RE::TESObjectMISC* Tally::LeastPart(const char* a_where, std::uint32_t a_items) const
	{
		const auto* part = item ? Materials::LeastPart(counts) : nullptr;
		if (!part) {
			return nullptr;
		}
		TraceLog::Line("scrap", "{:s} at {:d}%  {:s} gives 1 per item, {:d} in all, as every count came to 0  {:s}",
			TraceLog::Who{ item }, percent, TraceLog::Who{ part }, a_items, a_where);
		return part->scrapItem;
	}
}
