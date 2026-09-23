#include "UI/Repair/ConsoleRepair.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/Repair.h"
#include "Core/ItemCards.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <cstdint>

namespace ConsoleRepair
{
	namespace
	{
		// One optional whole number, the percent. The command table keeps a
		// pointer to it for as long as the game runs.
		RE::SCRIPT_PARAMETER PARAMETERS[]{
			{ "Percent", RE::SCRIPT_PARAM_TYPE::kInt, true },
		};

		bool Execute(const RE::SCRIPT_PARAMETER* a_parameters, const char* a_compiledParams, RE::TESObjectREFR* a_refObject,
			RE::TESObjectREFR* a_container, RE::Script* a_script, RE::ScriptLocals* a_scriptLocals, float&, std::uint32_t& a_offset)
		{
			// The engine writes only the numbers that were typed, so a bare srm
			// keeps this one.
			auto percent = static_cast<std::int32_t>(Repair::FULL);
			if (!RE::Script::ParseParameters(a_parameters, a_compiledParams, a_offset, a_refObject, a_container,
					a_script, a_scriptLocals, &percent)) {
				return false;
			}

			auto* const console = RE::ConsoleLog::GetSingleton();
			auto* const player = RE::PlayerCharacter::GetSingleton();
			auto* const weapon = player ? Equipped::Weapon(player) : nullptr;
			if (!weapon) {
				if (console) {
					console->Log("No weapon in hand that wears out.");
				}
				TraceLog::Line("wear", "srm found no weapon in hand that wears out");
				return true;
			}

			// 0 is allowed and leaves a broken weapon in hand, which play never
			// does, since the shot that breaks a weapon puts it away.
			const auto level = static_cast<std::uint32_t>(std::clamp(percent, 0, static_cast<std::int32_t>(Repair::FULL)));

			const auto health = Condition::FromPercent(level);
			const auto before = Equipped::WeaponHealth(player, weapon);

			// The workbench's own write, aimed at the stack in hand. The engine
			// splits one copy off a stack of several and moves the equipped
			// marks onto it.
			RE::BGSInventoryItem::FindEquippedStackFunctor inHand;
			RE::BGSInventoryItem::SetHealthFunctor set{ health };
			set.transferEquippedToSplitStack = true;
			player->FindAndWriteStackDataForInventoryItem(weapon, inHand, set);

			// The HUD reads the weapon again by itself. The Pip-Boy keeps the
			// cards it built, see ItemCards.h.
			ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);

			const auto name = RE::TESFullName::GetFullName(*weapon);
			TraceLog::Line("wear", "{:s} [{:08X}] {:.6f} -> {:.6f} from the console", name, weapon->formID, before, health);
			if (console) {
				console->Log("{:s} is at {:d}% condition.", name, level);
			}
			return true;
		}
	}

	void Install()
	{
		auto* const command = RE::SCRIPT_FUNCTION::LocateScriptCommand("ShowRepairMenu");
		if (!command) {
			REX::WARN("The console has no ShowRepairMenu command to take over, so srm repairs nothing.");
			return;
		}

		command->helpString = "Sets the weapon in hand to a condition in percent, or to full without one";
		command->SetParameters(PARAMETERS);
		command->executeFunction = Execute;

		REX::INFO("The console command srm repairs the weapon in hand, or sets it to the percent typed after it.");
	}
}
