#include "Papyrus/Papyrus.h"

#include "Core/ScriptEvents.h"
#include "Papyrus/Events.h"
#include "Papyrus/Log.h"
#include "Papyrus/Natives.h"
#include "Papyrus/Waiting.h"

#include <cstdint>
#include <string_view>

// The bind. F4SE calls Bind on a thread of its own while the game builds each
// script VM, after F4SE's own scripts are bound.
namespace Papyrus
{
	namespace
	{
		bool Bind(RE::BSScript::IVirtualMachine* a_vm)
		{
			if (!a_vm) {
				return false;
			}
			// A call waits for the VM's next update unless its function is
			// marked callable from tasklets, and then it runs at once on the
			// script's own thread, beside anything the game does. Only the 2
			// version functions are, since they read only the version and the
			// event queue, which has a lock of its own. A call that finds an
			// inventory locked runs again at the VM's next update, see
			// Waiting.h.
			std::uint32_t bound = 0;
			std::uint32_t tasklets = 0;
			const auto    version = [&](std::string_view a_function, auto a_native) {
				if (a_vm->BindNativeMethod("NEC"sv, a_function, a_native, true)) {
					bound++;
					tasklets++;
				}
			};
			const auto bind = [&](std::string_view a_function, auto a_native) {
				if (a_vm->BindNativeMethod(Waiting::Make(a_function, a_native))) {
					bound++;
				} else {
					REX::ERROR("Papyrus: the VM did not bind NEC.{:s}.", a_function);
				}
			};
			version("GetVersion"sv, GetVersion);
			version("GetVersionString"sv, GetVersionString);
			bind("IsFeatureOn"sv, IsFeatureOn);
			bind("IsFeatureTakenOver"sv, IsFeatureTakenOver);
			bind("GetSettingFloat"sv, GetSettingFloat);
			bind("GetSettingInt"sv, GetSettingInt);
			bind("GetSettingBool"sv, GetSettingBool);
			bind("GetItems"sv, GetItems);
			bind("GetEquippedWeapon"sv, GetEquippedWeapon);
			bind("GetWornItem"sv, GetWornItem);
			bind("GetWornItems"sv, GetWornItems);
			bind("GetHolder"sv, GetHolder);
			bind("GetBase"sv, GetBase);
			bind("GetCount"sv, GetCount);
			bind("SetCondition"sv, SetCondition);
			bind("Damage"sv, Damage);
			bind("Repair"sv, Repair);
			bind("Wear"sv, Wear);
			bind("RollCondition"sv, RollCondition);
			bind("IsTracked"sv, IsTracked);
			bind("WhyNotTracked"sv, WhyNotTracked);
			bind("GetCondition"sv, GetCondition);
			bind("IsBroken"sv, IsBroken);
			bind("IsUnrolled"sv, IsUnrolled);
			bind("GetDamageShare"sv, GetDamageShare);
			bind("GetArmorShare"sv, GetArmorShare);
			bind("GetAccuracyShare"sv, GetAccuracyShare);
			bind("GetRangeShare"sv, GetRangeShare);
			bind("GetFireRateShare"sv, GetFireRateShare);
			bind("GetCritShare"sv, GetCritShare);
			bind("GetPriceShare"sv, GetPriceShare);
			bind("GetJamChance"sv, GetJamChance);
			bind("GetWearPerUse"sv, GetWearPerUse);
			bind("GetFreeMendAbove"sv, GetFreeMendAbove);
			bind("GetModLockBelow"sv, GetModLockBelow);
			bind("GetRepairDiscount"sv, GetRepairDiscount);
			bind("GetRepairCost"sv, GetRepairCost);
			bind("GetScrapYield"sv, GetScrapYield);
			bind("GetSpareGain"sv, GetSpareGain);
			bind("GetTraderRepairLevel"sv, GetTraderRepairLevel);
			bind("GetTraderRepairPrice"sv, GetTraderRepairPrice);
			bind("GetFoodKind"sv, GetFoodKind);
			bind("GetSpoilDays"sv, GetSpoilDays);
			bind("GetFreshness"sv, GetFreshness);
			bind("GetDaysLeft"sv, GetDaysLeft);
			bind("SetFreshness"sv, SetFreshness);
			bind("Spoil"sv, Spoil);
			bind("IsSpoiled"sv, IsSpoiled);
			bind("IsFridge"sv, IsFridge);
			Log::Bound(bound, tasklets);
			return true;
		}
	}

	void Install()
	{
		ScriptEvents::SetTask(RunTask);
		const auto* papyrus = F4SE::GetPapyrusInterface();
		if (!papyrus || !papyrus->Register(Bind)) {
			REX::ERROR("Papyrus: F4SE took no bind for the script NEC, so no other mod can call it.");
		}
	}
}
