#include "Gameplay/FoodSpoil/Food.h"

#include "Core/ListFiles.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The lines
		// -------------------------------------------------------------------

		constexpr ListFiles::List LIST{ "Fridge", "counts no container as a fridge", "" };

		// 1 line into a_ids, said in NEC.log. Only a container counts, never
		// a door, a static or an activator that looks like a fridge. a_mine
		// holds what the file named so far, for its summary.
		void Take(const ListFiles::Line& a_line, std::vector<RE::TESFormID>& a_ids, std::vector<RE::TESFormID>& a_mine)
		{
			const auto id = a_line.form.formID;
			if (!a_line.form.Is(RE::ENUM_FORM_ID::kCONT)) {
				REX::INFO("{:s} line {:d}: {:s} [{:08X}] is not a container, so NEC skips it.", a_line.file, a_line.number,
					RE::TESFullName::GetFullName(a_line.form), id);
				return;
			}
			a_ids.push_back(id);
			if (std::ranges::find(a_mine, id) == a_mine.end()) {
				a_mine.push_back(id);
			}
		}

		// -------------------------------------------------------------------
		// The list
		// -------------------------------------------------------------------

		// Sorted form IDs, read from other threads.
		Snapshot<std::vector<RE::TESFormID>> g_list;
	}

	void LoadFridges()
	{
		std::vector<RE::TESFormID> ids;
		std::vector<RE::TESFormID> mine;
		ListFiles::Read(LIST, [&](const ListFiles::Line& a_line, ListFiles::Faults&) { Take(a_line, ids, mine); },
			[&](std::string_view a_file) {
				REX::INFO("{:s}: NEC counts {:d} {:s}.", a_file, mine.size(), mine.size() == 1 ? "container as a fridge" : "containers as fridges");
				mine.clear();
			});

		std::ranges::sort(ids);
		const auto [first, last] = std::ranges::unique(ids);
		ids.erase(first, last);
		g_list.Publish(std::make_shared<const std::vector<RE::TESFormID>>(std::move(ids)));
	}

	void UnloadFridges()
	{
		g_list.Publish(nullptr);
	}

	bool IsFridge(const RE::TESForm& a_base)
	{
		const auto list = g_list.Get();
		return list && std::ranges::binary_search(*list, a_base.formID);
	}
}
