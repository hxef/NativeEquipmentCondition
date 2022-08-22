#include "Common.h"
#include "Version.h"

// MAX_HEALTH should not be higher than 1.0 because it increases the value of the item(maybe some leftover code from older games).
extern const float MAX_HEALTH = 1.0;
extern const float MIN_HEALTH = 0.0;
extern const float INVALID_HEALTH = -1.0;

namespace Papyrus {

	constexpr auto SCRIPT_NAME{ Version::PROJECT };

	namespace {

		// Note: this class write-locks actor's inventory during its lifetime.
		class EquippedItemData {
		public:
			EquippedItemData(const RE::Actor& refr) {
				inv = refr.inventoryList;
				if (!inv) {
					return;
				}
				inv->rwLock.lock_write();
			}

			~EquippedItemData() {
				inv->rwLock.unlock_write();
			}

			// Returns a vector of mods attached to the BGSObjectInstanceExtra parameter.
			inline static std::vector<RE::BGSMod::Attachment::Mod*> GetModsFromObjectExtra(const RE::BGSObjectInstanceExtra* xObj) {
				std::vector<RE::BGSMod::Attachment::Mod*> attachedMods;
				const auto indices = xObj->GetIndexData();
				for (const auto& idx : indices) {
					attachedMods.push_back(RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(idx.objectID));
				}
				return attachedMods;
			}

			// Adds a new omod to the a BGSObjectInstanceExtra object.
			inline static void AttachMod(RE::BGSObjectInstanceExtra* const xObj, const RE::BGSMod::Attachment::Mod& mod) {
				xObj->AddMod(mod, 0, 1, 0);
			}

			inline bool FindEquipped() {
				for (const auto& item : inv->data) {
					if (!IsExpectedType(item)) {
						continue;
					}

					for (auto stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
						auto& xStack = stack->extra;
						if (!stack->IsEquipped() || !xStack) {
							continue;
						}

						// equipped items that have no ExtraInstanceData or BGSObjectInstanceExtra should be skipped,
						// given that all items that support condition mods are expected to have an instance data(because they should have one condition OMOD)
						const auto xInstanceData = xStack->GetByType<RE::ExtraInstanceData>();
						if (!xInstanceData) {
							continue;
						}
						const auto xObjectInstance = xStack->GetByType<RE::BGSObjectInstanceExtra>();
						if (!xObjectInstance) {
							continue;
						}

						const auto instanceData = xInstanceData->data.get();
						if (!instanceData) {
							continue;
						}

						if (!GetExpectedData(instanceData)) {
							continue;
						}

						// TODO: copy stack flags
						if (stack->count > 1 && !xStack->HasType(RE::EXTRA_DATA_TYPE::kUniqueID)) {
							// create the split stack
							auto split = new RE::BGSInventoryItem::Stack;
							stl::emplace_vtable(split);

							// create split stack's extra data list
							auto& xSplit = split->extra;
							xSplit = RE::BSTSmartPointer(new RE::ExtraDataList);

							// create a copy of stack's BGSObjectInstanceExtra and attach it to the split stack
							auto xSplitObjInstance = new RE::BGSObjectInstanceExtra();
							std::vector<RE::BGSMod::Attachment::Mod*> attachedMods = GetModsFromObjectExtra(xObjectInstance);
							for (auto& mod : attachedMods) {
								AttachMod(xSplitObjInstance, *mod);
							}
							xSplitObjInstance->flags = xObjectInstance->flags;
							xSplit->AddExtra(xSplitObjInstance);

							// create ExtraInstanceData from the BGSObjectInstanceExtra created above and attach it to the split stack
							RE::BSTSmartPointer<RE::TBO_InstanceData> xSplitTBO_InstanceData;
							xSplitObjInstance->CreateBaseInstanceData(*item.object, xSplitTBO_InstanceData);
							auto xSplitInstanceData = new RE::ExtraInstanceData(item.object, xSplitTBO_InstanceData);
							xSplit->AddExtra(xSplitInstanceData);

							// create a copy of stack's ExtraTextDisplayData and attach it to the split stack
							auto xStackTextDisplayData = xStack->GetByType<RE::ExtraTextDisplayData>();
							if (xStackTextDisplayData) {
								auto xSplitTextDisplayData = new RE::ExtraTextDisplayData();
								xSplitTextDisplayData->type = RE::EXTRA_DATA_TYPE::kTextDisplayData;
								xSplitTextDisplayData->flags = xStackTextDisplayData->flags;
								xSplitTextDisplayData->displayName = xStackTextDisplayData->displayName;
								xSplitTextDisplayData->displayNameText = xStackTextDisplayData->displayNameText;
								xSplitTextDisplayData->ownerQuest = xStackTextDisplayData->ownerQuest;
								xSplitTextDisplayData->ownerInstance = xStackTextDisplayData->ownerInstance;
								xSplitTextDisplayData->textPairs = xStackTextDisplayData->textPairs;
								xSplitTextDisplayData->customNameLength = xStackTextDisplayData->customNameLength;
								xSplit->AddExtra(xSplitTextDisplayData);
							}

							// create a copy of stack's ExtraHealth and attach it to the split stack
							auto xStackExtraHealth = xStack->GetByType<RE::ExtraHealth>();
							if (xStackExtraHealth) {
								auto xSplitExtraHealth = new RE::ExtraHealth();
								xSplitExtraHealth->health = xStackExtraHealth->health;
								xSplit->AddExtra(xSplitExtraHealth);
							}

							// adjust stack counts
							split->count = stack->count - 1;
							stack->count = 1;

							// add the created split stack to the inventory item
							auto it = stack;
							while (true) {
								if (!it->nextStack.get()) {
									it->nextStack = RE::BSTSmartPointer(split);
									break;
								}
								it = it->nextStack.get();
							}
						}

						// init base members
						this->instanceDataExtra = xInstanceData;
						this->tboInstanceData = instanceData;
						this->objectInstanceExtra = xObjectInstance;
						this->health = &InitConditionIfNeeded(stack);

						return true;
					}
				}
				return false;
			}

			// Decreases health value based on item features, attaches a new condition mod if a certain health threshold is hit.
			// Returns the new health value.
			inline float DegradeCondition() {
				const auto amount = GetDegradationRate();
				const auto oldHealth = GetHealth();
				const auto newHealth = ModHealth(-amount);

				const auto oldCndLevel = static_cast<std::uint8_t>(ceil(oldHealth * 10));
				const auto newCndLevel = static_cast<std::uint8_t>(ceil(newHealth * 10));

				std::vector<RE::BGSMod::Attachment::Mod*> attachedMods = GetModsFromObjectExtra(objectInstanceExtra);
				const auto keywordData = tboInstanceData->GetKeywordData();
				keywordData;

				// if old and new cnd levels don't match, attach corresponding condition mod to this item
				if (oldCndLevel != newCndLevel) {
					// find and remove any attached condition omod first
					for (auto& attached : attachedMods) {
						for (auto& cndObj : g_cndObjects) {
							if (attached == cndObj.omod) {
								RemoveMod(*attached);
								break;
							}
						}
					}
					// attach the new cnd omod
					AttachMod(*g_cndObjects[newCndLevel].omod);
				}

				return newHealth;
			}

			// Returns the health of the item
			inline float GetHealth() {
				return *health;
			}

		private:
			RE::ExtraInstanceData* instanceDataExtra;
			RE::TBO_InstanceData* tboInstanceData;
			RE::BGSObjectInstanceExtra* objectInstanceExtra;
			RE::BGSInventoryList* inv;
			// pointer to health value of the health extradata
			float* health;

			virtual bool IsExpectedType(const RE::BGSInventoryItem&) = 0;
			virtual bool GetExpectedData(RE::TBO_InstanceData* const) = 0;
			// Returns how much health should be decreased for each degradation event.
			virtual float GetDegradationRate() = 0;

			// Initializes health extradata and condition omod of the stack item if not present, to full condition.
			// Returns the reference to the health value of the health extradata.
			inline float& InitConditionIfNeeded(RE::BGSInventoryItem::Stack* const stack) {
				auto xHealth = stack->extra->GetByType<RE::ExtraHealth>();
				if (!xHealth) {
					// initialize health extradata for this item to max health
					xHealth = new RE::ExtraHealth();
					xHealth->type = xHealth->TYPE;
					xHealth->health = MAX_HEALTH;
					stack->extra->AddExtra(xHealth);

					// attach full condition mod
					AttachMod(*g_cndObjects[NUM_CONDITION_LEVELS - 1].omod);
				}
				return xHealth->health;
			}

			// Modify the health extradata value of the item based on the amount. Returns the new health value.
			inline float ModHealth(float amount) {
				auto& h = *health;
				h += amount;
				// clamp health value
				if (h < MIN_HEALTH) {
					h = MIN_HEALTH;
				} else if (h >= MAX_HEALTH) {
					h = MAX_HEALTH;
				}
				return h;
			}

			inline void AttachMod(const RE::BGSMod::Attachment::Mod& mod) {
				AttachMod(objectInstanceExtra, mod);
			}

			inline void RemoveMod(const RE::BGSMod::Attachment::Mod& mod) {
				objectInstanceExtra->RemoveMod(&mod, 0);
			}

			//// Sets item health to the value corresponding to its condition keyword that is added to it
			//inline float SetHealthByCndKwd() {
			//	// check if a degradation keyword is added and set health accordingly
			//	const auto keywordData = tboInstanceData->GetKeywordData();
			//	bool bFound = false;
			//	if (keywordData) {
			//		for (auto i = 0; i < NUM_CONDITION_LEVELS; i++) {
			//			if (keywordData->HasKeyword(g_CndKeywords[i], tboInstanceData)) {
			//				xHealth->health = (float)i / 10.0f;
			//				bFound = true;
			//				break;
			//				// logger::debug(FMT_STRING("{:s} xHealth initialized to {:f} has {:s} condition keyword."), item.object->GetFormEditorID(), xHealth->health, g_CndKeywords[i]->GetFormEditorID());
			//			}
			//		}
			//	}
			//	if (!bFound) {
			//		// initialize with max health if no degradation keywords are added
			//		xHealth->health = MAX_HEALTH;
			//	}
			//}
		};

		class EquippedWeapon : public EquippedItemData {
			using EquippedItemData::EquippedItemData;

		private:
			RE::TESObjectWEAP::InstanceData* eqWeaponInstanceData;

			bool IsExpectedType(const RE::BGSInventoryItem& item) override {
				if (item.object->IsWeapon()) {
					return true;
				}
				return false;
			}

			bool GetExpectedData(RE::TBO_InstanceData* const instanceData) override {
				const auto weaponInstance = RE::fallout_cast<RE::TESObjectWEAP::InstanceData*>(const_cast<RE::TBO_InstanceData*>(instanceData));
				if (!weaponInstance) {
					return false;
				}

				if (weaponInstance->type == RE::WEAPON_TYPE::kGrenade || weaponInstance->type == RE::WEAPON_TYPE::kMine) {
					// throwables not supported.
					return false;
				}

				this->eqWeaponInstanceData = weaponInstance;
				return true;
			}

			float GetDegradationRate() override {
				return 0.02f;
			}
		};

		// PAPYRUS FUNCTIONS //
		// Returns the health of the equipped weapon, or INVALID_HEALTH if no supported weapon is found.
		float GetEqWeapHealth(std::monostate, RE::Actor* actorRef) {
			auto weapon = EquippedWeapon(*actorRef);
			if (!weapon.FindEquipped()) {
				return INVALID_HEALTH;
			}
			return weapon.GetHealth();
		}

		// Degrades the condition of the equipped weapon. Returns the updated weapon health, or INVALID_HEALTH if no weapon is found.
		float DegradeEqWeapCnd(std::monostate, RE::Actor* actorRef) {
			auto weapon = EquippedWeapon(*actorRef);
			if (!weapon.FindEquipped()) {
				return INVALID_HEALTH;
			}
			return weapon.DegradeCondition();
		}

		void WorkbenchMenuOpened(std::monostate, bool bOpened) {

			// A map of OMOD pointers to vector of (component form, count) pairs that make up the OMOD, sorted by value.
			static std::unordered_map<RE::BGSMod::Attachment::Mod*, std::vector<std::tuple<RE::TESForm*, std::uint32_t>>> omodRequirements;
			static RE::BGSInventoryItem* moddedInventoryItem;

			// Clean up static vars if workbench menu was closed
			if (!bOpened) {
				omodRequirements.clear();
				moddedInventoryItem = nullptr;
				return;
			}

			const auto ui = RE::UI::GetSingleton();
			auto examineMenu = RE::fallout_cast<RE::ExamineMenu*>(ui->GetMenu("ExamineMenu").get());
			if (!examineMenu) {
				return;
			}

			// build the omod requirements map if empty
			if (omodRequirements.empty()) {
				for (auto& cobjForm : g_dataHandler->GetFormArray<RE::BGSConstructibleObject>()) {
					auto* createdMod = RE::fallout_cast<RE::BGSMod::Attachment::Mod*>(cobjForm->createdItem);
					if (!createdMod) {
						continue;
					}
					auto* requiredItems = cobjForm->requiredItems;
					if (!requiredItems) {
						continue;
					}
					for (auto* req = requiredItems->begin(); req != requiredItems->end(); req++) {
						omodRequirements[createdMod].push_back(std::make_tuple(req->first, req->second.i));
					}
					std::sort(omodRequirements[createdMod].begin(), omodRequirements[createdMod].end(), [](const auto& a, const auto& b) -> bool {
						auto* value_a = RE::fallout_cast<RE::TESValueForm*>(std::get<0>(a));
						auto* value_b = RE::fallout_cast<RE::TESValueForm*>(std::get<0>(b));
						if (!value_a) {
							return false;
						} else if (!value_b) {
							return true;
						}
						return value_a->value < value_b->value;
					});
				}
			}

			// Update the condition cobjs requirements when player hovers over another inventory item
			if (moddedInventoryItem != &examineMenu->moddedInventoryItem) {
				moddedInventoryItem = &examineMenu->moddedInventoryItem;

				 //auto attachedMods 
			}


		}

	}

	bool RegisterFunctions(RE::BSScript::IVirtualMachine* a_VM) {
		a_VM->BindNativeMethod(SCRIPT_NAME, "GetEqWeapHealth", GetEqWeapHealth, false);
		a_VM->BindNativeMethod(SCRIPT_NAME, "DegradeEqWeapCnd", DegradeEqWeapCnd, false);
		a_VM->BindNativeMethod(SCRIPT_NAME, "WorkbenchMenuOpened", WorkbenchMenuOpened, false);
		return true;
	}

}
