#include "Common.h"
#include "Version.h"

// Everything related to form data manipulation

RE::BGSKeyword* g_apCndKeyword;

static const constexpr auto PLUGIN_NAME = Version::ESP_FILE;
static const constexpr auto CND_AP_KWD_ID = 0x1A212;
static const constexpr auto CND_MODCOL_OMOD_ID = 0x26F0C;
static const constexpr auto CND_TEMPLATE_WEAPON = 0x1A1E7;
static const constexpr auto CND_WEAPON_INNR = 0x1A1E9;
// Condition related forms, from worst to best
static const constexpr std::uint32_t CND_KEYWORDS_ID[] = { 0x1E500, 0x1E501, 0x1E502, 0x1E503, 0x1E504, 0x1E505, 0x1E506, 0x1E507, 0x1E508, 0x1E509, 0x1E50A, 0x1E50B, 0x1E50C, 0x1E50D, 0x1E50E, 0x1E50F, 0x1E510, 0x1E511, 0x1E512, 0x1E513, 0x1E514 };
static const constexpr std::uint32_t CND_OMODS_ID[] = { 0x22A00, 0x22A01, 0x22A02, 0x22A03, 0x22A04, 0x22A05, 0x22A06, 0x22A07, 0x22A08, 0x22A09, 0x22A0A, 0x22A0B, 0x22A0C, 0x22A0D, 0x22A0E, 0x22A0F, 0x22A10, 0x22A11, 0x22A12, 0x22A13, 0x22A14 };
static const constexpr std::uint32_t CND_COBJ_ID[] =  { 0x3C000, 0x3C001, 0x3C002, 0x3C003, 0x3C004, 0x3C005, 0x3C006, 0x3C007, 0x3C008, 0x3C009, 0x3C00A };
static_assert(sizeof(CND_KEYWORDS_ID) == sizeof(CND_OMODS_ID));

const std::uint8_t NUM_CONDITION_LEVELS = sizeof(CND_KEYWORDS_ID) / sizeof(std::uint32_t);

std::vector<ConditionObject> g_cndObjects;

namespace Forms
{

	static constexpr std::string_view CND_NAMES[] = { "Broken", "Ruined", "Faulty", "Worn", "Good" };

	void Register()
	{
		logger::info("Registering forms...");

		if (!g_dataHandler->LookupLoadedModByName(PLUGIN_NAME)) {
			stl::report_and_fail(fmt::format(
				FMT_STRING("{:s} is not loaded."),
				PLUGIN_NAME));
		}

		g_apCndKeyword = g_dataHandler->LookupForm<RE::BGSKeyword>(CND_AP_KWD_ID, PLUGIN_NAME);
		if (!g_apCndKeyword) {
			stl::report_and_fail("Condition attachment point was not found.");
		}
		auto modcolCnd = g_dataHandler->LookupForm<RE::BGSMod::Attachment::Mod>(CND_MODCOL_OMOD_ID, PLUGIN_NAME);
		if (!modcolCnd) {
			stl::report_and_fail("Condition mod collection was not found.");
		}

		// Populate global condition keywords, omods and cobj formlists
		g_cndObjects.reserve(NUM_CONDITION_LEVELS);
		for (auto i = 0; i < NUM_CONDITION_LEVELS; i++) {
			auto* kwd = g_dataHandler->LookupForm<RE::BGSKeyword>(CND_KEYWORDS_ID[i], PLUGIN_NAME);
			if (!kwd) {
				stl::report_and_fail("Condition keyword was not found.");
			}
			auto* omod = g_dataHandler->LookupForm<RE::BGSMod::Attachment::Mod>(CND_OMODS_ID[i], PLUGIN_NAME);
			if (!omod) {
				stl::report_and_fail("Condition omod was not found.");
			}
			RE::BGSConstructibleObject* cobj;
			// cobj is initialized for even indices only.
			if (i % 2 == 0) {
				cobj = g_dataHandler->LookupForm<RE::BGSConstructibleObject>(CND_COBJ_ID[i / 2], PLUGIN_NAME);
				if (!cobj) {
					stl::report_and_fail("Condition cobj was not found.");
				}
			}
			g_cndObjects.push_back({ kwd, omod, cobj });
		}

		// Set condition cobj to create its corresponding condition omod
		for (auto i = 0; i < NUM_CONDITION_LEVELS; i++) {
			auto* cobj = g_cndObjects[i].cobj;
			auto* omod = g_cndObjects[i].omod;
			if (i != 0) { // broken omod is not craftable
				cobj->createdItem = omod;
			}
			cobj->data.numConstructed = 1;
			// add name and description to each condition omod
			if (i == 0) {
				omod->fullName = std::format("Condition: {:s}", CND_NAMES[0]);
			} else if (i == NUM_CONDITION_LEVELS - 1) {
				omod->fullName = std::format("Condition: {:s}", CND_NAMES[4]);
			} else {
				omod->fullName = std::format("Condition: {:d}%", i * 10);
			}
		}

		// Get index of condition ap keyword
		bool bFound = false;
		std::uint16_t apCndIndex = 0;
		const auto keywords = RE::BGSKeyword::GetTypedKeywords();
		if (keywords) {
			const auto& arr = (*keywords)[stl::to_underlying(RE::KeywordType::kAttachPoint)];
			for (std::uint16_t i = 0; i < arr.size(); i++) {
				if (arr[i] == g_apCndKeyword) {
					logger::info(FMT_STRING("Keyword {:s} has index {:d}."), g_apCndKeyword->GetFormEditorID(), i);
					apCndIndex = i;
					bFound = true;
					break;
				}
			}
		}
		if (!bFound) {
			stl::report_and_fail(fmt::format(
				FMT_STRING("Keyword index for {:s} was not found."),
				g_apCndKeyword->GetFormEditorID()));
		}

		// Some weapons don't have any object template item, but can be used by the player during normal gameplay.
		// Use the following weapon form as a template for creating an object template item for these kind of weapons.
		// (creating one from code does not work, something to do with RE::BGSMod::Template::Item constructor).
		// Another use for this is to initialize ap list for weapons that don't have any ap, but are still playable.
		auto cndTemplateWeapForm = g_dataHandler->LookupForm<RE::TESObjectWEAP>(CND_TEMPLATE_WEAPON, PLUGIN_NAME);
		if (!cndTemplateWeapForm) {
			stl::report_and_fail("Condition template weapon form was not found.");
		}
		assert(cndTemplateWeapForm->objectTemplate.items.size() == 1);

		// set of pointers to weapon INNR forms
		std::unordered_set<RE::BGSInstanceNamingRules*> weaponInnrPtrs;

		// The base condition INNR used for weapons that don't have an INNR form
		auto cndWeapInnr = g_dataHandler->LookupForm<RE::BGSInstanceNamingRules>(CND_WEAPON_INNR, PLUGIN_NAME);
		if (!cndTemplateWeapForm) {
			stl::report_and_fail("Weapon condition INNR form was not found.");
		}

		using TypedKwdAp = RE::BGSTypedKeywordValue<RE::KeywordType::kAttachPoint>;
		std::uint32_t numPatched = 0;

		// Iterate through all weapons
		for (auto weaponForm : g_dataHandler->GetFormArray<RE::TESObjectWEAP>()) {
			// skip weapons that are: non-playable, mines, grenades, without equip slot, without a world model, with no name
			if (!weaponForm->GetPlayable(weaponForm->GetBaseInstanceData()) || weaponForm->weaponData.type == RE::WEAPON_TYPE::kGrenade || weaponForm->weaponData.type == RE::WEAPON_TYPE::kMine || !weaponForm->equipSlot || weaponForm->model.empty() || weaponForm->fullName.empty()) {
				continue;
			}

			// Insert attach point keyword for condition
			auto& apArr = weaponForm->attachParents.array;
			auto& apArrSize = weaponForm->attachParents.size;
			if (!apArr) {
				logger::info(FMT_STRING("Weapon {:x} does not have an AP array."), weaponForm->GetFormID());
				apArrSize = 0;
			}
			void* allocApArr;
			if (apArr) {
				allocApArr = RE::aligned_realloc(apArr, alignof(TypedKwdAp), sizeof(TypedKwdAp) * (apArrSize + 1));
			} else {
				allocApArr = RE::aligned_alloc(alignof(TypedKwdAp), sizeof(TypedKwdAp));
			}
			if (!allocApArr) {
				stl::report_and_fail(fmt::format(
					FMT_STRING("Failed to alloc attachParents array for {:s}."),
					weaponForm->GetFormEditorID()));
			}
			apArr = reinterpret_cast<TypedKwdAp*>(allocApArr);
			apArr[apArrSize].keywordIndex = apCndIndex;
			apArrSize++;

			// Insert condition mod collection to all of the weapon's object template items

			auto& objTmpl = weaponForm->objectTemplate;
			if (objTmpl.items.empty()) {
				// If weapon does not have any obj template, copy and paste a new one from the cnd weapon template form
				logger::info(FMT_STRING("Weapon {:x} does not have an object template."), weaponForm->GetFormID());
				auto newItemCopy = RE::malloc<RE::BGSMod::Template::Item>();
				if (!newItemCopy) {
					stl::report_and_fail("Failed to allocate a RE::BGSMod::Template::Item.");
				}
				auto cndItem = cndTemplateWeapForm->objectTemplate.items.at(0);
				std::memcpy(newItemCopy, cndItem, sizeof(*newItemCopy));
				newItemCopy->parentTemplate = &objTmpl;
				objTmpl.items.push_back(newItemCopy);
			} else {
				// Else, iterate through all template items and update them with the condition modcol
				for (auto& item : objTmpl.items) {
					// This buffer is composed of the following elements in this order: list of omods ptrs, list of property mods ptrs, omods total size, prop mods total size.
					// Each omod and property mod is structured in blocks of 16 bytes. Each total size is a 4 byte int.
					auto& buffer = item->buffer;
					if (!buffer) {
						// Template items with no buffer seem to be ignored by the game, no point in adding condition modcol to this item
						continue;
					}
					// buffer size covers only the count of omods and prop mods, so it's a multiple of 16.
					auto& bufferSize = item->size;
					// in order to insert the condition modcol, we need a new buffer with size of old buffer + 16(condition modcol) + size of numbers representing the size of omod and prop-mod lists
					auto* alloc = RE::malloc(bufferSize + 16 + sizeof(std::uint32_t) * 2);
					if (!alloc) {
						stl::report_and_fail(fmt::format(
							FMT_STRING("Failed to allocate new object template item buffer for {:s}."),
							weaponForm->GetFormEditorID()));
					}

					auto* newBuffer = reinterpret_cast<std::byte*>(alloc);
					// write the condition modcol omod address at the beginning of new buffer.
					// for omods, first 8 bytes represent its memory address, and the purpose of the other 8 is unknown
					// for now we'll just copy the 8 bytes, which is the ptr to first omod(fingers crossed the oter 8 bytes don't matter)
					std::memcpy(newBuffer, &modcolCnd, sizeof(&modcolCnd));
					std::memcpy(newBuffer + 8, buffer + 8, 8);
					// append the values of the original buffer to the new buffer
					std::memcpy(newBuffer + 16, buffer, bufferSize + sizeof(std::uint32_t) * 2);
					// increment the omod list size
					std::uint32_t omodListSize;
					bufferSize += 16;
					std::memcpy(&omodListSize, newBuffer + bufferSize, sizeof(std::uint32_t));
					omodListSize += 16;
					std::memcpy(newBuffer + bufferSize, &omodListSize, sizeof(std::uint32_t));

					RE::free(buffer);
					buffer = newBuffer;
				}
			}

			// Store INNR form of this weapon
			auto& innr = weaponForm->instanceNamingRules;
			if (innr) {
				weaponInnrPtrs.insert(innr);
			} else {
				// Associate weapon condition INNR with this weapon.
				weaponForm->instanceNamingRules = cndWeapInnr;
			}

			logger::info(FMT_STRING("Patched weapon {:s}, id {:x}."), weaponForm->GetFullName(), weaponForm->GetFormID());
			numPatched++;
		}

		logger::info(FMT_STRING("Injected condition omods to object templates and condition attach point to {:d} weapon forms."), numPatched);

		if (weaponInnrPtrs.empty()) {
			stl::report_and_fail("Couldn't find any INNR form attached to weapons.");
		}

		// Inject condition naming rules to INNR forms associated with patched weapon forms
		using RuleDataType = RE::BGSInstanceNamingRules::RuleData;
		using RuleSetType = RE::BGSInstanceNamingRules::RuleSet;
		numPatched = 0;
		for (auto it = weaponInnrPtrs.begin(); it != weaponInnrPtrs.end(); ++it) {
			auto& innr = *it;
			// pick first ruleset since condition state should have the highest priority
			auto& ruleSet0 = innr->ruleSets[0];

			// Resize the ruleset array to meet new requirements
			// Resizing has very high chance to realloc the array. When this happens, all ruledata objects are reallocated, thus the vfptr tables will need to be fixed.
			ruleSet0.reserve(ruleSet0.size() + NUM_CONDITION_LEVELS - 1);

			for (auto i = 0; i < NUM_CONDITION_LEVELS - 1; i++) {
				RuleDataType ruleData;
				ruleData.index = 1;
				ruleData.propertyBridgeArrayIndex = -1;
				ruleData.keywords.numKeywords = 1;
				if (i == 0) {
					ruleData.text = CND_NAMES[0];
				} else if (i <= 3) {
					ruleData.text = CND_NAMES[1];
				} else if (i <= 6) {
					ruleData.text = CND_NAMES[2];
				} else if (i <= 9) {
					ruleData.text = CND_NAMES[3];
				}

				auto alloc = RE::malloc<RE::BGSKeyword*>();
				if (!alloc) {
					stl::report_and_fail("Failed to allocate pointer to BGSKeyword.");
				}
				alloc = &g_cndObjects[i].keyword;
				ruleData.keywords.keywords = alloc;

				ruleSet0.push_back(ruleData);
			}

			// fix the vfptr tables for each ruledata from ruleSet0.
			std::for_each(ruleSet0.begin(), ruleSet0.end(), [](auto& ruleData) {
				stl::emplace_vtable(&ruleData.keywords);
				stl::emplace_vtable(static_cast<RE::IKeywordFormBase*>(&ruleData.keywords));
			});

			numPatched++;
		}

		logger::info(FMT_STRING("Injected condition naming rules to {:d} INNR forms."), numPatched);

		// patch string format of this specific load screen
		auto loadScreen = g_dataHandler->LookupForm<RE::TESLoadScreen>(0x2B360, PLUGIN_NAME);
		loadScreen->loadingText = std::vformat(loadScreen->loadingText, std::make_format_args(CND_NAMES[0]));

		auto TestWeapo = g_dataHandler->LookupForm<RE::TESObjectWEAP>(0x15b0da, "Fallout4.esm");
		TestWeapo;
		int i = 0;
		i++;
	}
}
