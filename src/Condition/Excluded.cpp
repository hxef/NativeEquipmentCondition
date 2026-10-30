#include "Condition/Excluded.h"

#include "Condition/Condition.h"
#include "Core/ListFiles.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace Excluded
{
	namespace
	{
		// -------------------------------------------------------------------
		// The lines
		// -------------------------------------------------------------------

		constexpr ListFiles::List LIST{ "Exclude", "leaves no item alone", "" };

		// 1 line of a file into a_ids, said in NEC.log. a_mine holds what the
		// file left alone so far, for its summary.
		void Take(const ListFiles::Line& a_line, std::vector<RE::TESFormID>& a_ids, std::vector<RE::TESFormID>& a_mine)
		{
			const auto& form = a_line.form;
			const auto  id = form.formID;
			const auto  name = RE::TESFullName::GetFullName(form);
			if (!form.IsWeapon() && !form.Is(RE::ENUM_FORM_ID::kARMO)) {
				REX::INFO("{:s} line {:d}: {:s} [{:08X}] is not a weapon or armor, so NEC skips it.", a_line.file, a_line.number, name, id);
				return;
			}

			// Kept even when the kind leaves it out today, since armor's rule
			// is measured again at every save load.
			a_ids.push_back(id);
			if (const auto* why = Condition::KindWhyNoCondition(static_cast<const RE::TESBoundObject&>(form))) {
				REX::INFO("{:s} line {:d}: {:s} [{:08X}] takes no condition anyway, {:s}.", a_line.file, a_line.number, name, id, why);
			} else if (std::ranges::find(a_mine, id) == a_mine.end()) {
				a_mine.push_back(id);
				REX::DEBUG("{:s} line {:d}: {:s} [{:08X}] left alone.", a_line.file, a_line.number, name, id);
			}
		}

		// -------------------------------------------------------------------
		// The list
		// -------------------------------------------------------------------

		// Sorted form IDs, read from other threads, and whether it holds any.
		Snapshot<std::vector<RE::TESFormID>> g_list;
		std::atomic<bool>                    g_any{ false };
	}

	void Load()
	{
		std::vector<RE::TESFormID> ids;
		std::vector<RE::TESFormID> mine;
		ListFiles::Read(LIST, [&](const ListFiles::Line& a_line, ListFiles::Faults&) { Take(a_line, ids, mine); },
			[&](std::string_view a_file) {
				REX::INFO("{:s}: NEC leaves {:d} {:s} alone.", a_file, mine.size(), mine.size() == 1 ? "item" : "items");
				mine.clear();
			});

		std::ranges::sort(ids);
		const auto [first, last] = std::ranges::unique(ids);
		ids.erase(first, last);
		const auto any = !ids.empty();
		g_list.Publish(any ? std::make_shared<const std::vector<RE::TESFormID>>(std::move(ids)) : nullptr);
		g_any.store(any, std::memory_order_relaxed);
	}

	void Unload()
	{
		g_any.store(false, std::memory_order_relaxed);
		g_list.Publish(nullptr);
	}

	bool Any()
	{
		return g_any.load(std::memory_order_relaxed);
	}

	bool Has(const RE::TESForm& a_form)
	{
		if (!Any()) {
			return false;
		}
		const auto list = g_list.Get();
		return list && std::ranges::binary_search(*list, a_form.formID);
	}
}
