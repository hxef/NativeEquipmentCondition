#include "Degradation.h"

namespace Degradation
{
	// SetHealthPerc removes the health extradata if set to 1.0.
	// Must not be higher than 1.0 because it increases the value of the item (leftover code Skyrim).
	const float Condition::MAX_HEALTH = 0.9999999f;
	const float Condition::MIN_HEALTH = 0.0f;
	const float Condition::INVALID_HEALTH = -1.0f;

	// Namespace-scope counterparts declared in Degradation.h. These were declared but
	// never defined, which would not link once EquippedItemData was referenced.
	// Kept in step with the Condition members above (which are private, so they
	// cannot simply be aliased here).
	const float MAX_HEALTH = 0.9999999f;
	const float MIN_HEALTH = 0.0f;
	const float INVALID_HEALTH = -1.0f;

	void ForEachStackWithLock(RE::BGSInventoryList* a_inv,
		std::function<bool(RE::BGSInventoryItem&)> a_filter,
		std::function<bool(RE::BGSInventoryItem&, RE::BGSInventoryItem::Stack&)> a_continue)
	{
		const RE::BSAutoWriteLock l(a_inv->rwLock);
		a_inv->ForEachStack(a_filter, a_continue);
	}

	void Condition::Decrease(const RE::TESBoundObject& object)
	{
		if (!this->stack.extra) {
			return;
		}

		auto* xInstanceData = this->stack.extra->GetByType<RE::ExtraInstanceData>();
		if (!xInstanceData) {
			REX::WARN("item has no instance data, condition will not be decreased");
			return;
		}

		const auto amount = GetDegradationRate(xInstanceData->data.get());
		const auto curHealth = stack.extra->GetHealthPerc();
		const auto newHealth = SetHealth(curHealth - amount);

		const auto curCndLevel = static_cast<std::uint8_t>(GetLevel(curHealth));
		const auto newCndLevel = static_cast<std::uint8_t>(GetLevel(newHealth));

		// If current and new cnd levels don't match, attach the new condition mod, removing the current condition mod.
		if (curCndLevel != newCndLevel) {
			auto* xObjectInstance = stack.extra->GetByType<RE::BGSObjectInstanceExtra>();
			if (!xObjectInstance) {
				REX::WARN("item has no object instance extra, condition mods won't be applied");
				return;
			}
			xObjectInstance->RemoveMod(GetMod(curCndLevel), 1);
			xObjectInstance->AddMod(*GetMod(newCndLevel), 1, 0, false);
			object.ApplyMods(xInstanceData->data, xObjectInstance);
			UpdateInstanceData(xInstanceData->data.get());
		}
	}

	float Condition::SetHealth(float a_health)
	{
		// Clamp health value.
		if (a_health < MIN_HEALTH) {
			a_health = MIN_HEALTH;
		} else if (a_health >= MAX_HEALTH) {
			a_health = MAX_HEALTH;
		}
		this->stack.extra->SetHealthPerc(a_health);
		return a_health;
	}

	uint8_t Condition::GetLevel(const float& a_health)
	{
		return ceil(a_health / 0.05) * 0.05 * (NUM_CONDITION_LEVELS - 1);
	}

	float WeaponCondition::GetDegradationRate(RE::TBO_InstanceData* a_tboInstanceData)
	{
			const auto& instanceData = static_cast<RE::TESObjectWEAP::InstanceData*>(a_tboInstanceData);
			uint32_t totalAttackDamage = instanceData->attackDamage;
			auto damageTypes = instanceData->damageTypes;
			if (damageTypes) {
				for (auto& dmg : *damageTypes) {
					totalAttackDamage += dmg.second.i;
				}
			}
			return totalAttackDamage * 0.00003;
	}

	void WeaponCondition::UpdateInstanceData(RE::TBO_InstanceData* a_tboInstanceData)
	{
		auto instanceData = static_cast<RE::TESObjectWEAP::InstanceData*>(a_tboInstanceData);

	}

	RE::BGSMod::Attachment::Mod* WeaponCondition::GetMod(uint8_t level)
	{
		return g_cndObjects[level].omod;
	}
}
