#include "UI/Repair/Workbench/Missing.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		// How many of a component the bench can reach, loose or inside junk,
		// counted the way its requirements panel and the station count it.
		[[nodiscard]] std::uint32_t Held(RE::TESObjectREFR* a_pile, const RE::TESForm* a_component)
		{
			std::uint32_t count = 0;
			RE::ExamineMenu::CountComponentHeld(a_pile, { &a_component, &count }, false);
			return count;
		}

		// How many of a_component one a_item holds, as the game counts junk:
		// the first entry for it in the item's own Components list. A loose
		// component's scrap item lists its component too.
		[[nodiscard]] std::uint32_t UnitsIn(const RE::TESBoundObject* a_item, const RE::TESForm* a_component)
		{
			const auto* junk = a_item ? a_item->As<RE::TESObjectMISC>() : nullptr;
			if (!junk || !junk->componentData) {
				return 0;
			}
			const auto& list = *junk->componentData;
			const auto  entry = std::ranges::find_if(list, [a_component](const auto& a_entry) {
				return a_entry.first == a_component;
			});
			return entry == list.end() ? 0 : entry->second.i;
		}
	}

	bool ShowMissing(RE::ExamineMenu* a_menu, const Job& a_job)
	{
		if (!a_menu || a_menu->CheckModChoiceRequirements(&a_job.choice)) {
			return false;
		}

		// The box reads its title from this setting and never checks it
		// exists, so without it no box goes up.
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		const auto* title = settings ? settings->GetSetting("sCannotRepairMessage"sv) : nullptr;
		RE::UIUtils::PlayMenuSound(RepairPrompt::REFUSED_SOUND);
		if (!title) {
			TraceLog::Line("menu", "Workbench cannot pay for the repair to {:d}% and the game has no words for its box, so it shows none",
				a_job.level);
			return true;
		}

		// What the bench would take for the job, out of the same list the
		// check above reads. It takes all the list has of a short part, and
		// whole junk items for a met one, so a met part can count past its need.
		Taken taken;
		a_menu->GetRequiredComponents(&a_job.parts, &a_menu->optimizedAutoBuildInv, &taken);

		// The box draws its rows from the job's parts, and beside each part it
		// shows the count given here. A short row counts the check's list, so
		// a box that says the bench lacks parts always draws one. A met row
		// counts the pile the way the station does. The pile can count more
		// than the list, such as quest junk the check passes over, so it alone
		// could read Oil (1/1) on a row the check finds short. Made with the
		// engine's allocator, see Box.cpp.
		auto*       pile = a_menu->sharedContainerRef.get();
		auto*       data = new RE::ExamineConfirmMenu::InitDataRepairFailure(&a_job.parts);
		std::string held;
		for (const auto& part : a_job.parts) {
			auto* object = part.first ? part.first->As<RE::TESBoundObject>() : nullptr;
			if (!object) {
				continue;
			}
			std::uint32_t have = 0;
			for (const auto& item : taken) {
				have += item.second * UnitsIn(item.first, object);
			}
			if (pile && have >= part.second.i) {
				have = std::max(have, Held(pile, object));
			}
			data->availableComponents.emplace(object, have);
			held += std::format("{:s}{:s} {:d}/{:d}", held.empty() ? "" : ", ",
				RE::TESFullName::GetFullName(*object), have, part.second.i);
		}

		// OK does nothing more. The box and the bench free the data and the
		// callback, see Box.cpp, so neither is touched again here.
		a_menu->ShowConfirmMenu(data, new RE::RepairFailureCallback(a_menu));

		TraceLog::Line("menu", "Workbench cannot pay for the repair to {:d}%, so it listed the parts, held against needed: {:s}",
			a_job.level, held.empty() ? "nothing"sv : std::string_view{ held });
		return true;
	}
}
