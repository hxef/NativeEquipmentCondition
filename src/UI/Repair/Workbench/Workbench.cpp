#include "UI/Repair/Workbench/Workbench.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Repair.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/MenuMovies.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Job.h"
#include "UI/Repair/Workbench/Lists.h"

#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		using Params = Scaleform::GFx::FunctionHandler::Params;

		// What a repair's confirmation asks, in words the game already
		// translates.
		constexpr const char* QUESTION = "$Repair";

		// The sound the bench plays when it turns the player away from the mod
		// slots, and the number Flash calls PlaySound by. Nothing else in the
		// menu asks for this sound.
		constexpr const char* REFUSED_SOUND = "UICancel";
		constexpr auto        PLAY_SOUND = static_cast<std::uintptr_t>(RE::WorkbenchMenuBase::CodeObjectFunction::kPlaySound);

		// What the game does before this, which every hook calls straight away
		// while g_bench is not intact. Of the 11 functions this file patches
		// on the bench's table, 3 are ones the weapon and armor benches leave
		// empty or answer no to: OnSwitchBaseItem, GetCanRepairSelectedItem
		// and RepairSelectedItem. Each is what its slot held before NEC: the
		// game's own, 0 for an empty slot, or the hook of a DLL that patched
		// the slot first and hands the call on.
		REL::Relocation<void (*)(RE::ExamineMenu*, bool)>                 _BuildConfirmed;
		REL::Relocation<const ModChoice* (*)(RE::ExamineMenu*)>           _QCurrentModChoiceData;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                       _ShowBuildFailureMessage;
		REL::Relocation<bool (*)(RE::ExamineMenu*)>                       _TryCreate;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                       _HighlightWeaponPart;
		REL::Relocation<void (*)(RE::ExamineMenu*, const Params&)>        _Call;
		REL::Relocation<const char* (*)(RE::ExamineMenu*)>                _GetBuildConfirmButtonLabel;
		REL::Relocation<void (*)(RE::ExamineMenu*, char*, std::uint32_t)> _GetBuildConfirmQuestion;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                       _OnSwitchBaseItem;
		REL::Relocation<bool (*)(RE::ExamineMenu*)>                       _GetCanRepairSelectedItem;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                       _RepairSelectedItem;

		// The same for the callback the bench hands its confirmation box.
		REL::Relocation<RE::ExamineConfirmMenu::ICallback* (*)(RE::ExamineConfirmMenu::ICallback*, std::uint32_t)> _DeleteConfirmCallback;

		// Every place of the repair.
		CallPatch::Held g_bench;

		// Every change of the highlighted item, one line before the Flash side
		// redraws its buttons, which is what keeps RENAME on an item at full
		// condition.
		void OnSwitchBaseItemHk(RE::ExamineMenu* a_menu)
		{
			_OnSwitchBaseItem(a_menu);
			if (!g_bench.Intact() || !a_menu || !a_menu->menuObj.IsObject()) {
				return;
			}

			const auto selection = Selected(a_menu);
			Announce(a_menu, selection);

			if (selection.object) {
				TraceLog::Line("menu", "Workbench highlighted {:s} at {:d}%, a stack of {:d}, offering {:s}",
					selection.Name(), selection.percent, selection.count,
					selection.Trifling() ? "MEND" : (selection.Worn() ? "REPAIR" : "RENAME"));
			}
		}

		// Whether the button is live, asked on every redraw of the buttons, so
		// it asks the cheap question. Which levels are worth offering waits for
		// the press. The redraw has just handed the bar its buttons, so this is
		// also where REPAIR learns to read MEND.
		bool GetCanRepairSelectedItemHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Intact()) {
				return _GetCanRepairSelectedItem.address() && _GetCanRepairSelectedItem(a_menu);
			}
			const auto selection = Selected(a_menu);
			Label(a_menu, selection);
			return !Above(selection).empty();
		}

		// The button, and the only way into a repair. A barely worn item is
		// repaired on the spot. One the bench has nothing to rebuild from is
		// sent to a trader, see Cost.h. One with a single level worth offering
		// goes straight to the confirmation, through the task queue like the
		// prompt.
		void RepairSelectedItemHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Intact()) {
				if (_RepairSelectedItem.address()) {
					_RepairSelectedItem(a_menu);
				}
				return;
			}
			const auto selection = Selected(a_menu);
			TraceLog::Line("menu", "Workbench {:s} pressed on {:s} at {:d}%",
				selection.Trifling() ? "MEND" : "REPAIR", selection.Name(), selection.percent);

			if (selection.Trifling()) {
				Mend(a_menu);
				return;
			}
			if (!selection.Worn()) {
				return;
			}

			const auto priced = PriceOf(selection);
			if (priced.units == 0) {
				const auto said = Text::BenchCannotRepair();
				RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
				RE::UIUtils::PlayMenuSound(REFUSED_SOUND);
				TraceLog::Line("menu", "Workbench has nothing to rebuild {:s} from, so it sent the player to a trader",
					selection.Name());
				return;
			}

			const auto above = Above(selection);
			const auto offered = Offered(selection, priced);
			if (offered.size() < above.size()) {
				TraceLog::Line("menu",
					"Workbench dropped {:d} of {:d} levels for {:s}, each costing what the level above it costs",
					above.size() - offered.size(), above.size(), selection.Name());
			}

			if (offered.size() != 1) {
				AskWhichLevel(selection, offered);
				return;
			}

			const auto* tasks = F4SE::GetTaskInterface();
			const auto  level = offered.front();
			TraceLog::Line("menu", "Workbench had only {:d}% worth offering, so it did not ask", level);
			if (tasks) {
				tasks->AddTask([level] { Begin(OpenBench(), level); });
			}
		}

		// The job the bench is pricing. Everything the build path reads about
		// what is being made comes through here.
		const ModChoice* QCurrentModChoiceDataHk(RE::ExamineMenu* a_menu)
		{
			return g_bench.Intact() && Repairing(a_menu) ? &InHand().choice : _QCurrentModChoiceData(a_menu);
		}

		// The word on the button of the confirmation box.
		const char* GetBuildConfirmButtonLabelHk(RE::ExamineMenu* a_menu)
		{
			return g_bench.Intact() && Repairing(a_menu) ? REPAIR_WORD : _GetBuildConfirmButtonLabel(a_menu);
		}

		// What a build that is not a repair asks for, as the game prices it. A
		// mod built after a repair was turned down shows it is back on its own
		// parts.
		void TraceBuild(RE::ExamineMenu* a_menu)
		{
			const auto* choice = _QCurrentModChoiceData(a_menu);
			if (!choice) {
				return;
			}

			std::string parts;
			if (choice->requiredItems) {
				for (const auto& part : *choice->requiredItems) {
					parts += std::format("{:s}{:d} {:s}", parts.empty() ? "" : ", ", part.second.i,
						part.first ? RE::TESFullName::GetFullName(*part.first) : "nothing"sv);
				}
			}

			// A mod at the weapon and armor benches and an object at the other
			// stations, a form either way.
			const RE::TESForm* made = choice->object;
			TraceLog::Line("menu", "Workbench asks to build {:s} [{:08X}] for {:s}",
				made ? RE::TESFullName::GetFullName(*made) : "nothing"sv, made ? made->formID : 0U,
				parts.empty() ? "nothing"sv : std::string_view{ parts });
		}

		// What the confirmation box asks. The game's own names the mod being
		// made out of the recipe, and a repair's recipe has no name to give.
		void GetBuildConfirmQuestionHk(RE::ExamineMenu* a_menu, char* a_buffer, std::uint32_t a_length)
		{
			if (!g_bench.Intact() || !Repairing(a_menu)) {
				_GetBuildConfirmQuestion(a_menu, a_buffer, a_length);
				if (TraceLog::IsOpen()) {
					TraceBuild(a_menu);
				}
				return;
			}
			if (a_buffer && a_length > 0) {
				std::snprintf(a_buffer, a_length, "%s", QUESTION);
			}
		}

		// Yes on the confirmation box.
		void BuildConfirmedHk(RE::ExamineMenu* a_menu, bool a_ownerIsWorkbench)
		{
			if (g_bench.Intact() && Repairing(a_menu)) {
				Finish(a_menu);
				return;
			}
			_BuildConfirmed(a_menu, a_ownerIsWorkbench);
		}

		// The confirmation box's callback, being freed. The box calls back on
		// yes only and closes without a word when turned down, and the bench
		// frees the callback on its first frame after the box closes, so a
		// repair still standing here was turned down. The power armor station
		// hands its box the same callback, which is why the bench's job has to
		// be in hand as well.
		RE::ExamineConfirmMenu::ICallback* DeleteConfirmCallbackHk(RE::ExamineConfirmMenu::ICallback* a_callback, std::uint32_t a_flags)
		{
			if (!g_bench.Intact()) {
				return _DeleteConfirmCallback(a_callback, a_flags);
			}
			auto* menu = a_callback ? a_callback->thisMenu : nullptr;
			if (Repairing(menu) && InHand().choice.recipe) {
				TraceLog::Line("menu", "Workbench repair to {:d}% turned down at the confirmation", InHand().level);
				Drop(menu);
			}
			return _DeleteConfirmCallback(a_callback, a_flags);
		}

		// The components were not there. The game's own panel says so, and the
		// bench goes back to mods either way.
		void ShowBuildFailureMessageHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Intact()) {
				_ShowBuildFailureMessage(a_menu);
				return;
			}
			if (Repairing(a_menu)) {
				TraceLog::Line("menu", "Workbench could not pay for the repair");
			}
			_ShowBuildFailureMessage(a_menu);
			Drop(a_menu);
		}

		// The last check on an item too worn to modify, in case its slot list
		// is reached anyway.
		bool TryCreateHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Intact()) {
				return _TryCreate(a_menu);
			}
			const auto selection = Selected(a_menu);
			if (!Repairing(a_menu) && selection.TooWorn()) {
				TraceLog::Line("menu", "Workbench refused to build on {:s} at {:d}%, below the {:d}% floor",
					selection.Name(), selection.percent, MODIFY_FLOOR);
				return false;
			}
			return _TryCreate(a_menu);
		}

		// The bench colours the part of the item the highlighted slot belongs
		// to, reading the model in the viewer without looking, and there is
		// nothing there while the viewer is between models.
		void HighlightWeaponPartHk(RE::ExamineMenu* a_menu)
		{
			if (!g_bench.Intact() || (a_menu && a_menu->GetCurrent3D())) {
				_HighlightWeaponPart(a_menu);
			}
		}

		// Everything Flash asks code to do passes through here by number. The
		// one of interest is the cancel sound the bench plays when it has just
		// turned the player away from the mod slots, the moment to say why, and
		// the only one, since the refusal happens inside the movie. That holds
		// for the mouse, the key, the pad and MODIFY alike. The bench plays the
		// same sound for an item with no slots, such as one listed only to be
		// repaired, and that one is told so whatever its condition, since a
		// repair would open nothing.
		void CallHk(RE::ExamineMenu* a_menu, const Params& a_params)
		{
			_Call(a_menu, a_params);

			if (!g_bench.Intact() || reinterpret_cast<std::uintptr_t>(a_params.userData) != PLAY_SOUND ||
				a_params.argCount < 1 || !a_params.args || !a_params.args[0].IsString()) {
				return;
			}

			const std::string_view sound = a_params.args[0].GetString();
			if (sound != REFUSED_SOUND) {
				return;
			}

			const auto selection = Selected(a_menu);
			if (NoModFits(selection)) {
				const auto said = Text::CannotModify();
				RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
				TraceLog::Line("menu", "Workbench turned {:s} at {:d}% away from the mod slots, no mod fits it",
					selection.Name(), selection.percent);
				return;
			}
			if (!selection.TooWorn()) {
				return;
			}

			const auto said = Text::TooDamaged(MODIFY_FLOOR);
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			TraceLog::Line("menu", "Workbench turned {:s} at {:d}% away from the mod slots",
				selection.Name(), selection.percent);
		}
	}

	void Install()
	{
		// The 12 places of the repair, the button, the build and the
		// confirmation box, only work as one, so a place another mod has
		// leaves all of them. A skipped call in the middle of a repair would
		// let the game build NEC's repair recipe as a mod, and NEC cannot tell
		// a mod that always hands the call on from one that only sometimes
		// does. So any change here after NEC turns repairs off for good, see
		// CallPatch::EVERY_CALL.
		{
			const CallPatch::Together bench{ Part::kNone, CallPatch::EVERY_CALL };

			REL::Relocation<std::uintptr_t> menu{ RE::ExamineMenu::VTABLE[0] };

			_Call = CallPatch::PatchSlot(menu, 0x01, CallHk, "bench calls").value_or(0);
			_BuildConfirmed = CallPatch::PatchSlot(menu, 0x17, BuildConfirmedHk, "bench build confirmed").value_or(0);
			_QCurrentModChoiceData = CallPatch::PatchSlot(menu, 0x19, QCurrentModChoiceDataHk, "bench mod choice").value_or(0);
			_ShowBuildFailureMessage = CallPatch::PatchSlot(menu, 0x1A, ShowBuildFailureMessageHk, "bench build failure").value_or(0);
			_TryCreate = CallPatch::PatchSlot(menu, 0x1B, TryCreateHk, "bench try create").value_or(0);
			_HighlightWeaponPart = CallPatch::PatchSlot(menu, 0x28, HighlightWeaponPartHk, "bench highlight part").value_or(0);
			_GetBuildConfirmButtonLabel = CallPatch::PatchSlot(menu, 0x30, GetBuildConfirmButtonLabelHk, "bench confirm label").value_or(0);
			_GetBuildConfirmQuestion = CallPatch::PatchSlot(menu, 0x31, GetBuildConfirmQuestionHk, "bench confirm question").value_or(0);
			// NEC owns the REPAIR button here and never hands the call on, so
			// NEC never runs on top of a mod at these 2 slots, which it would
			// skip.
			_GetCanRepairSelectedItem = CallPatch::PatchSlot(menu, 0x32, GetCanRepairSelectedItemHk, "bench can repair", Part::kNone, CallPatch::NEVER_HANDS_ON).value_or(0);
			_OnSwitchBaseItem = CallPatch::PatchSlot(menu, 0x37, OnSwitchBaseItemHk, "bench switch item").value_or(0);
			_RepairSelectedItem = CallPatch::PatchSlot(menu, 0x3A, RepairSelectedItemHk, "bench repair", Part::kNone, CallPatch::NEVER_HANDS_ON).value_or(0);

			// Index 0 is the callback's destructor. Every bench hands its build
			// confirmation this callback, the power armor station included.
			// The boxes for leaving a bench and for scrapping have their own.
			REL::Relocation<std::uintptr_t> confirm{ RE::VTABLE::__ModConfirmCallback[0] };
			_DeleteConfirmCallback = CallPatch::PatchSlot(confirm, 0x00, DeleteConfirmCallbackHk, "bench confirm delete").value_or(0);

			g_bench = bench.Set();
		}

		// The 3 list slots and the list refresh call stand alone, see
		// Lists.cpp. They only list, fade and shut rows, and TryCreate still
		// refuses a mod on an item too worn to modify, so a skipped call
		// leaves 1 list as the game draws it. Another mod at one of them
		// takes only that place, but the item list and its refresh call work
		// only together, see PLACES in Core/Pieces.cpp.
		InstallLists();

		CraftingPerks::SetBench(&Repairs);
		if (!g_bench) {
			REX::ERROR("Workbenches stay as they were, with no repairs from NEC.");
			return;
		}

		// As the settings stand at the start. The Settings line gives every
		// change from the MCM page. Any wear reads 99% at most, so 99 mends
		// nothing for free either.
		const auto mend = FreeRepairs()                  ? std::string{ "Every repair is put right on the spot for nothing, since fBenchCostMult is 0." } :
		                  FreeAbove() + 1 < Repair::FULL ? std::format("Wear above {:d}% is put right on the spot for nothing.", FreeAbove()) :
		                                                   std::string{ "Every repair costs components." };
		REX::INFO("The workbench modifies nothing below {:d}% condition. {:s}", MODIFY_FLOOR, mend);
		std::string levels;
		for (const auto level : Repair::LEVELS) {
			levels += std::format("{:s}{:d}%", levels.empty() ? "" : " ", level);
		}
		REX::INFO("The workbench repairs weapons and armor to {:s}", levels);
		REX::INFO("Before any crafting perk, an item at nothing owes {:.2f} of what it is built from, and at each level {:s}",
			Repair::Debt(0, Scaled(WRECK_MULTIPLE)), Repair::Ladder(Scaled(WRECK_MULTIPLE)));
		REX::INFO("The whole of a crafting perk brings that {:.2f} down to {:.2f}",
			Scaled(WRECK_MULTIPLE), Scaled(CraftingPerks::SKILLED_MULTIPLE));
	}

	bool Repairs()
	{
		return g_bench.Intact();
	}

	void Load()
	{
		REX::INFO("The workbench speaks {:s}", Text::Language());
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "ExamineMenu.swf"sv)) {
			ForgetListed();
			WatchEquipped(a_movie);
		}
	}
}
