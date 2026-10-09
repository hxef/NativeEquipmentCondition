#include "UI/Repair/Workbench/Missing.h"

#include "Core/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		// How many of a component the bench can reach, loose or inside junk,
		// counted the way its requirements panel counts it.
		[[nodiscard]] std::uint32_t Held(RE::TESObjectREFR* a_pile, const RE::TESForm* a_component)
		{
			std::uint32_t count = 0;
			RE::ExamineMenu::CountComponentHeld(a_pile, { &a_component, &count }, false);
			return count;
		}
	}

	bool ShowMissing(RE::ExamineMenu* a_menu, const Job& a_job)
	{
		if (!a_menu || a_menu->CheckModChoiceRequirements(&a_job.choice)) {
			return false;
		}

		// The box reads its title from this setting and never checks it
		// exists. Where the box cannot be built, the corner says the title, as
		// TryCreate says sCannotBuildMessage for a mod.
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		const auto* title = settings ? settings->GetSetting("sCannotRepairMessage"sv) : nullptr;
		auto*       pile = a_menu->sharedContainerRef.get();
		RE::UIUtils::PlayMenuSound(RepairPrompt::REFUSED_SOUND);
		if (!pile || !title) {
			if (title) {
				RE::SendHUDMessage::ShowHUDMessage(title->GetString().data(), nullptr, true, true);
			}
			TraceLog::Line("menu", "Workbench cannot pay for the repair to {:d}% and has no list to show, so the corner says so where the game has the words",
				a_job.level);
			return true;
		}

		// The box draws its rows from the job's parts, and beside each part it
		// shows the count given here. Made with the game's own allocator, see
		// Box.cpp.
		auto*       data = new RE::ExamineConfirmMenu::InitDataRepairFailure(&a_job.parts);
		std::string held;
		for (const auto& part : a_job.parts) {
			auto* object = part.first ? part.first->As<RE::TESBoundObject>() : nullptr;
			if (!object) {
				continue;
			}
			const auto have = Held(pile, object);
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
