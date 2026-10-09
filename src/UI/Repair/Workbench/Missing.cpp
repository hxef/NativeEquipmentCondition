#include "UI/Repair/Workbench/Missing.h"

#include "Core/TraceLog.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		// The sound the box's own Cancel key plays. TryCreate plays
		// SoundMenuCancel when it turns a build down, and no sound in the
		// game's files has that name.
		constexpr const char* REFUSED_SOUND = "UIMenuCancel";

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
		// exists.
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		auto*       pile = a_menu->sharedContainerRef.get();
		if (!pile || !settings || !settings->GetSetting("sCannotRepairMessage"sv)) {
			TraceLog::Line("menu", "Workbench cannot pay for the repair to {:d}% and has no list to show, so the corner says so",
				a_job.level);
			return false;
		}

		// The box draws its rows from the job's parts, and beside each part it
		// shows the count given here. The game frees the data and the
		// callback, so they are made with the game's own allocator, which
		// CommonLibF4's operator new does.
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

		// OK does nothing more. The box frees the data as it opens, once its
		// rows are drawn, and the bench frees the callback on its first frame
		// after the box closes, so neither is touched again here.
		RE::UIUtils::PlayMenuSound(REFUSED_SOUND);
		a_menu->ShowConfirmMenu(data, new RE::RepairFailureCallback(a_menu));

		TraceLog::Line("menu", "Workbench cannot pay for the repair to {:d}%, so it listed the parts, held against needed: {:s}",
			a_job.level, held.empty() ? "nothing"sv : std::string_view{ held });
		return true;
	}

	ScopedRepairWords::ScopedRepairWords()
	{
		auto*       settings = RE::GameSettingCollection::GetSingleton();
		auto*       build = settings ? settings->GetSetting("sCannotBuildMessage"sv) : nullptr;
		const auto* repair = settings ? settings->GetSetting("sCannotRepairMessage"sv) : nullptr;
		const auto  words = repair ? repair->GetString() : ""sv;
		if (build && !build->GetString().empty() && !words.empty()) {
			_build = build;
			_kept = build->GetString().data();
			_build->SetString(const_cast<char*>(words.data()));
		}
	}

	ScopedRepairWords::~ScopedRepairWords()
	{
		if (_build) {
			_build->SetString(const_cast<char*>(_kept));
		}
	}
}
