#include "UI/Repair/Workbench/Workbench.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Repair.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/MenuMovies.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Workbench/Bench.h"
#include "UI/Repair/Workbench/Cost.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Job.h"
#include "UI/Repair/Workbench/Label.h"
#include "UI/Repair/Workbench/Lists.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		using Params = Scaleform::GFx::FunctionHandler::Params;

		// -------------------------------------------------------------------
		// What the hooks stand in front of
		// -------------------------------------------------------------------

		// The sound the bench asks for when it turns the player away from the
		// mod slots, and the number Flash calls PlaySound by. Nothing else in
		// the menu asks for this sound. No sound in the game has this name, so
		// it plays nothing, and NEC only watches for it.
		constexpr const char* SLOTS_REFUSED = "UICancel";
		constexpr auto        PLAY_SOUND = static_cast<std::uintptr_t>(RE::WorkbenchMenuBase::CodeObjectFunction::kPlaySound);

		// What the game does before this, which every hook calls straight away
		// while its place or the repair is off. Of the 6 functions this file
		// patches on the bench's table, 3 are ones the weapon and armor benches
		// leave doing nothing or answer no to: OnSwitchBaseItem,
		// GetCanRepairSelectedItem and RepairSelectedItem. Each is what its
		// slot held before NEC: the game's own, or the hook of a DLL that
		// patched the slot first and hands the call on.
		REL::Relocation<bool (*)(RE::ExamineMenu*)>                _TryCreate;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                _HighlightWeaponPart;
		REL::Relocation<void (*)(RE::ExamineMenu*, const Params&)> _Call;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                _OnSwitchBaseItem;
		REL::Relocation<bool (*)(RE::ExamineMenu*)>                _GetCanRepairSelectedItem;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                _RepairSelectedItem;

		// Whether each place still runs, see CallPatch::PatchSlot. The 3 places
		// behind the REPAIR button first.
		CallPatch::LinkBase g_switchLink;
		CallPatch::LinkBase g_canRepairLink;
		CallPatch::LinkBase g_repairLink;
		CallPatch::LinkBase g_tryCreateLink;
		CallPatch::LinkBase g_highlightLink;

		// -------------------------------------------------------------------
		// The 3 places behind the REPAIR button
		// -------------------------------------------------------------------

		// Whenever the movie switches the item the bench shows, which vanilla
		// does just before redrawing its buttons, and at every scrap. A list
		// mod may switch through every row as it reads them. Running before
		// that redraw is what keeps RENAME on an item at full condition.
		void OnSwitchBaseItemHk(RE::ExamineMenu* a_menu)
		{
			_OnSwitchBaseItem(a_menu);
			if (!Repairs() || !a_menu || !a_menu->menuObj.IsObject()) {
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

		// Whether the button is live, asked whenever the inventory buttons
		// redraw over an item the flag offers REPAIR for, so it asks the cheap
		// question. Which levels are worth offering waits for the press. The
		// redraw has just handed the bar its buttons, so this is also where
		// REPAIR learns to read MEND.
		bool GetCanRepairSelectedItemHk(RE::ExamineMenu* a_menu)
		{
			if (!Repairs()) {
				return _GetCanRepairSelectedItem(a_menu);
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
			if (!Repairs()) {
				_RepairSelectedItem(a_menu);
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
				RE::UIUtils::PlayMenuSound(RepairPrompt::REFUSED_SOUND);
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

		// -------------------------------------------------------------------
		// Building, and the part the bench colours
		// -------------------------------------------------------------------

		// What a build asks for, as the game prices it. A repair never passes
		// here, see Box.h.
		void TraceBuild(RE::ExamineMenu* a_menu)
		{
			const auto* choice = a_menu->QCurrentModChoiceData();
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

		// Every BUILD, and the last check on an item too worn to modify, in
		// case its slot list is reached anyway.
		bool TryCreateHk(RE::ExamineMenu* a_menu)
		{
			if (TraceLog::IsOpen() && a_menu) {
				TraceBuild(a_menu);
			}
			if (!Repairs() || !g_tryCreateLink.Live()) {
				return _TryCreate(a_menu);
			}
			const auto selection = Selected(a_menu);
			if (selection.TooWorn()) {
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
			if (!Repairs() || !g_highlightLink.Live() || (a_menu && a_menu->GetCurrent3D())) {
				_HighlightWeaponPart(a_menu);
			}
		}

		// -------------------------------------------------------------------
		// The bench turning the player away
		// -------------------------------------------------------------------

		// Everything Flash asks code to do passes through here by number. The
		// one of interest is the cancel sound the bench asks for when it has
		// just turned the player away from the mod slots, the moment to say why,
		// and the only one, since the refusal happens inside the movie. That
		// holds however the player got there, MODIFY included, see Display.h.
		// The bench asks for the same sound for an item with no slots, such as
		// one listed only to be repaired, and that one is told so whatever its
		// condition, since a repair would open nothing.
		void CallHk(RE::ExamineMenu* a_menu, const Params& a_params)
		{
			_Call(a_menu, a_params);

			if (!Repairs() || reinterpret_cast<std::uintptr_t>(a_params.userData) != PLAY_SOUND ||
				a_params.argCount < 1 || !a_params.args || !a_params.args[0].IsString()) {
				return;
			}

			const std::string_view sound = a_params.args[0].GetString();
			if (sound != SLOTS_REFUSED) {
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

	// -------------------------------------------------------------------
	// Install, Load and the bench movie
	// -------------------------------------------------------------------

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::ExamineMenu::VTABLE[0] };

		// The 3 places behind the REPAIR button: the flag, the grey and the
		// press. A repair runs through no other place of the bench's, since NEC
		// puts its box up itself, see Box.h. A cut at any of them turns the
		// repair and its mod lock off together, see Repairs. NEC owns the
		// REPAIR button and never hands the call on at 2 of them while repairs
		// run, see CallPatch::NEVER_HANDS_ON.
		const auto canRepair = CallPatch::PatchSlot(menu, 0x32, GetCanRepairSelectedItemHk, "bench can repair", Part::kNone,
			CallPatch::NEVER_HANDS_ON, &g_canRepairLink);
		const auto switchItem = CallPatch::PatchSlot(menu, 0x37, OnSwitchBaseItemHk, "bench switch item", Part::kNone, true, &g_switchLink);
		const auto repair = CallPatch::PatchSlot(menu, 0x3A, RepairSelectedItemHk, "bench repair", Part::kNone, CallPatch::NEVER_HANDS_ON,
			&g_repairLink);
		_GetCanRepairSelectedItem = canRepair.value_or(0);
		_OnSwitchBaseItem = switchItem.value_or(0);
		_RepairSelectedItem = repair.value_or(0);

		// These 2 stand alone too. A skipped call there never costs a repair.
		_TryCreate = CallPatch::PatchSlot(menu, 0x1B, TryCreateHk, "bench try create", Part::kNone, true, &g_tryCreateLink).value_or(0);
		_HighlightWeaponPart = CallPatch::PatchSlot(menu, 0x28, HighlightWeaponPartHk, "bench highlight part", Part::kNone, true,
			&g_highlightLink).value_or(0);

		// Every repair reaches NEC through the slots above. This hook only
		// adds NEC's message once the game's call is through, so it shows
		// whenever the call reaches NEC while repairs work, and another DLL mod
		// here takes nothing.
		_Call = CallPatch::PatchSlot(menu, 0x01, CallHk, "bench calls", Part::kBenchMessages).value_or(0);

		// The list places stand alone, see Lists.h. A skipped call there leaves
		// 1 list as the game draws it.
		InstallLists();

		CraftingPerks::SetBench(&Repairs);
		if (!canRepair || !switchItem || !repair) {
			REX::WARN("Workbenches stay as they were, with no repairs from NEC.");
			return;
		}

		// As the settings stand at the start. The Settings line gives every
		// change from the MCM page. Any wear reads 99% at most, so 99 mends
		// nothing for free either.
		const auto mend = FreeRepairs()                  ? std::string{ "Every repair is put right on the spot for nothing, since fBenchCostMult is 0." } :
		                  FreeAbove() + 1 < Repair::FULL ? std::format("Condition above {:d}% is put right on the spot for nothing.", FreeAbove()) :
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
		return g_switchLink.Live() && g_canRepairLink.Live() && g_repairLink.Live();
	}

	void Load()
	{
		REX::INFO("The workbench speaks {:s}", Text::Language());
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "ExamineMenu.swf"sv)) {
			ForgetListed();
			ForgetWord();
			WatchEquipped(a_movie);
		}
	}
}
