#include "Gameplay/FoodSpoil/Food.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/Stamp.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FoodSpoil
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Params = Scaleform::GFx::FunctionHandler::Params;
		using Call = RE::ExamineMenu::CodeObjectFunction;

		// The most spoiled food 1 Fertilizer may cost, see iSpoiledPerFertilizer.
		constexpr std::int32_t MOST_NEEDED = 20;

		// c_Fertilizer in Fallout4.esm, the component recipes ask for. A
		// component keeps no editor ID in memory, so it goes by its ID. Its
		// scrap item is 1 loose Fertilizer, the one settlement brahmin make.
		constexpr RE::TESFormID FERTILIZER = 0x05A0C7;

		CallPatch::Held                                                g_fertilizer;
		CallPatch::Link<void (*)(RE::ExamineMenu*, const Params&)>     g_registerLink;
		CallPatch::Link<void (*)(RE::ExamineMenu*)>                    g_choicesLink;
		CallPatch::Link<bool (*)(RE::ExamineMenu*)>                    g_tryLink;
		CallPatch::Link<void (*)(RE::ExamineMenu*, bool)>              g_buildLink;
		REL::Relocation<void (*)(RE::ExamineMenu*, const Params&)>     _RegisterMenuComponents;
		REL::Relocation<void (*)(RE::ExamineMenu*)>                    _UpdateModChoiceList;
		REL::Relocation<bool (*)(RE::ExamineMenu*)>                    _TryCreate;
		REL::Relocation<void (*)(RE::ExamineMenu*, bool)>              _BuildConfirmed;
		REL::Relocation<void (*)(RE::ExamineMenu*, const Params&)>     _Call;

		// -------------------------------------------------------------------
		// The row's recipe
		// -------------------------------------------------------------------

		// The 1 category the row sits in, Utility.
		RE::BGSTypedKeywordValue<RE::KeywordType::kRecipeFilter> g_category{};

		// NEC's recipe, never a form: no ID, no conditions and nothing
		// required, so the game draws its row lit and opens its box. The hooks
		// below give it the real grey, the refusal and the build. Upcasts
		// place each function table where that part of a recipe begins.
		RE::BGSConstructibleObject& Recipe()
		{
			alignas(RE::BGSConstructibleObject) static std::byte space[sizeof(RE::BGSConstructibleObject)]{};
			static RE::BSTArray<RE::BSTTuple<RE::TESForm*, RE::BGSTypedFormValuePair::SharedVal>> nothing;
			static auto* recipe = [] {
				auto* out = reinterpret_cast<RE::BGSConstructibleObject*>(space);
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::TESForm*>(out)) =
					RE::BGSConstructibleObject::VTABLE[0].address();
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::BGSPickupPutdownSounds*>(out)) =
					RE::BGSConstructibleObject::VTABLE[1].address();
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::TESDescription*>(out)) =
					RE::BGSConstructibleObject::VTABLE[2].address();
				out->formType = RE::ENUM_FORM_ID::kCOBJ;
				out->requiredItems = &nothing;
				out->data.numConstructed = 1;
				out->filterKeywords.array = &g_category;
				out->filterKeywords.size = 1;
				return out;
			}();
			return *recipe;
		}

		// iSpoiledPerFertilizer kept from 0 to MOST_NEEDED.
		std::uint32_t Needed()
		{
			return static_cast<std::uint32_t>(std::clamp(Settings::iSpoiledPerFertilizer.GetValue(), 0, MOST_NEEDED));
		}

		// The loose Fertilizer the row makes, read each time so a mod that
		// gives c_Fertilizer another scrap item is followed.
		RE::TESObjectMISC* FertilizerItem()
		{
			const auto* component = g_dataHandler ? g_dataHandler->LookupForm<RE::BGSComponent>(FERTILIZER, "Fallout4.esm") : nullptr;
			return component ? component->scrapItem : nullptr;
		}

		// Utility's index among recipe categories, from a recipe of a_menu's
		// bench in that category, so the row joins a category the bench
		// shows. Nothing when no recipe there is in Utility.
		std::optional<std::uint16_t> UtilityIndex(const RE::CookingMenu& a_menu)
		{
			auto* utility = RE::TESForm::GetFormByEditorID<RE::BGSKeyword>("RecipeUtility");
			if (!utility) {
				return std::nullopt;
			}
			for (const auto* recipe : a_menu.recipes) {
				if (!recipe) {
					continue;
				}
				const auto& categories = recipe->filterKeywords;
				for (std::uint32_t i = 0; i < categories.size; i++) {
					const auto index = categories.array[i].keywordIndex;
					if (RE::BGSKeyword::GetTypedKeywordByIndex(RE::KeywordType::kRecipeFilter, index) == utility) {
						return index;
					}
				}
			}
			return std::nullopt;
		}

		// Whether a_menu's highlighted row is NEC's. The game must never build
		// NEC's row, so a row already on show stays NEC's even once the set
		// stops running.
		bool NecRow(RE::ExamineMenu* a_menu)
		{
			const auto* choice = a_menu ? a_menu->QCurrentModChoiceData() : nullptr;
			return choice && choice->recipe == &Recipe();
		}

		// NecRow, for the hook of the set handing on through a_link. It asks
		// the set first, so a set that waits sees the hook run.
		bool OnRow(RE::ExamineMenu* a_menu, const CallPatch::LinkBase& a_link)
		{
			static_cast<void>(g_fertilizer.Runs(a_link));
			return NecRow(a_menu);
		}

		// -------------------------------------------------------------------
		// Spoiled food in reach
		// -------------------------------------------------------------------

		// Every stack of food in a_menu's containers that spoiled or whose
		// time ran out, in the order StationStacks gives.
		std::vector<HeldStack> SpoiledStacks(const RE::ExamineMenu& a_menu)
		{
			const auto today = Today();
			return today ? StationStacks(a_menu, nullptr, false, *today) : std::vector<HeldStack>{};
		}

		std::uint32_t Count(const std::vector<HeldStack>& a_stacks)
		{
			std::uint32_t count = 0;
			for (const auto& stack : a_stacks) {
				count += stack.count;
			}
			return count;
		}

		// -------------------------------------------------------------------
		// The row, its panel and its build
		// -------------------------------------------------------------------

		// Lights or greys NEC's row among the rows on show, if it is there.
		void Grey(RE::ExamineMenu& a_menu)
		{
			const auto& choices = a_menu.modChoiceArray;
			const auto  at = std::ranges::find(choices, &Recipe(), &RE::WorkbenchMenuBase::ModChoiceData::recipe);
			Value       rows;
			Value       row;
			if (at == choices.end() || !a_menu.modChoiceList.IsObject() || !a_menu.modChoiceList.GetMember("entryList"sv, &rows) ||
				!rows.IsArray() || !rows.GetElement(static_cast<std::uint32_t>(at - choices.begin()), &row) || !row.IsObject()) {
				return;
			}
			row.SetMember("hasRequired"sv, Value(Count(SpoiledStacks(a_menu)) >= Needed()));
			a_menu.modChoiceList.SetMember("entryList"sv, rows);
		}

		// The requirements beside NEC's row: "Spoiled food 7/10", faded while
		// short, as the station shows a component.
		void ShowNeed(RE::ExamineMenu& a_menu)
		{
			auto& list = a_menu.requirementsList;
			if (!a_menu.uiMovie || !list.IsObject()) {
				return;
			}
			const auto words = Text::SpoiledFood();
			Value      rows;
			Value      row;
			a_menu.uiMovie->CreateArray(&rows);
			a_menu.uiMovie->CreateObject(&row);
			row.SetMember("text"sv, Value(words.c_str()));
			row.SetMember("requiredCount"sv, Value(Needed()));
			row.SetMember("accountedFor"sv, Value(Count(SpoiledStacks(a_menu))));
			rows.PushBack(row);
			list.SetMember("sort"sv, Value("text"));
			list.SetMember("entryList"sv, rows);
		}

		// Turns away a build with too little spoiled food in reach, as the
		// game turns away a recipe it lacks the parts for.
		void Refuse(std::uint32_t a_have)
		{
			RE::UIUtils::PlayMenuSound("UIMenuCancel");
			auto*       settings = RE::GameSettingCollection::GetSingleton();
			const auto* message = settings ? settings->GetSetting("sCannotBuildMessage"sv) : nullptr;
			if (message) {
				RE::SendHUDMessage::ShowHUDMessage(message->GetString().data(), nullptr, true, true);
			}
			TraceLog::Line("food", "Fertilizer refused with {:d} spoiled food in reach of {:d}", a_have, Needed());
		}

		// Takes the spoiled pieces 1 Fertilizer costs from what the bench
		// reaches, in order, and gives the player 1 Fertilizer. No experience,
		// as for a recipe that costs nothing.
		void Build(RE::ExamineMenu& a_menu)
		{
			auto*      player = RE::PlayerCharacter::GetSingleton();
			auto*      fertilizer = FertilizerItem();
			const auto stacks = SpoiledStacks(a_menu);
			const auto have = Count(stacks);
			if (!player || !fertilizer || have < Needed()) {
				Refuse(have);
				return;
			}

			auto                             owed = Needed();
			std::vector<RE::TESBoundObject*> taken;
			std::string                      said;
			for (const auto& stack : stacks) {
				if (owed == 0) {
					break;
				}
				const auto count = Take(stack, owed);
				if (count == 0) {
					continue;
				}
				owed -= count;
				if (std::ranges::find(taken, stack.food) == taken.end()) {
					taken.push_back(stack.food);
				}
				if (TraceLog::IsOpen()) {
					said += std::format("{:s}{:d} {:s}", said.empty() ? "" : ", ", count, RE::TESFullName::GetFullName(*stack.food));
				}
			}
			player->AddInventoryItem(fertilizer, nullptr, 1, nullptr, nullptr, nullptr);

			// Each food's put down sound under the chemistry station's loop, as
			// the game plays a recipe's parts.
			for (auto* food : taken) {
				a_menu.QueueCraftingComponent(food);
			}
			auto* store = RE::TESForm::GetFormByEditorID<RE::BGSDefaultObject>("CraftingLoopSound_Chemlab_DO");
			if (auto* loop = store ? store->GetForm<RE::BGSSoundDescriptorForm>() : nullptr) {
				a_menu.PlayCraftSound(loop);
			}

			// The rows again, as the game redraws them after a build. NEC's row
			// is only ever added once every slot of its set is written, so the
			// original is there.
			a_menu.UpdateOptimizedAutoBuildInv();
			_UpdateModChoiceList(&a_menu);
			Grey(a_menu);
			ShowNeed(a_menu);
			TraceLog::Line("food", "Fertilizer [{:08X}] made from {:s}, {:d} spoiled food left in reach", fertilizer->GetFormID(),
				said.empty() ? "nothing"sv : std::string_view{ said }, have - (Needed() - owed));
		}

		// -------------------------------------------------------------------
		// The hooks
		// -------------------------------------------------------------------

		// The menu fills its recipes as its movie loads. At a chemistry station
		// NEC's row joins them while food spoils and iSpoiledPerFertilizer is
		// above 0.
		void RegisterHk(RE::ExamineMenu* a_menu, const Params& a_params)
		{
			_RegisterMenuComponents(a_menu, a_params);
			if (!g_fertilizer.Runs(g_registerLink) || !Settings::bFoodSpoils.GetValue() || !a_menu || !StationHas(*a_menu, CHEMISTRY)) {
				return;
			}
			if (Needed() == 0) {
				TraceLog::Line("food", "No Fertilizer row: iSpoiledPerFertilizer is 0");
				return;
			}
			auto&      menu = static_cast<RE::CookingMenu&>(*a_menu);
			auto*      fertilizer = FertilizerItem();
			const auto category = UtilityIndex(menu);
			if (!fertilizer || !category) {
				TraceLog::Line("food", "No Fertilizer row: {:s}", fertilizer ? "no recipe here is in Utility" : "Fallout4.esm has no c_Fertilizer");
				return;
			}
			auto& recipe = Recipe();
			if (std::ranges::find(menu.recipes, &recipe) != menu.recipes.end()) {
				return;
			}
			recipe.createdItem = fertilizer;
			g_category.keywordIndex = *category;
			menu.recipes.push_back(&recipe);
		}

		// The rows of the highlighted category. NEC's row greys while the
		// bench reaches too little spoiled food.
		void UpdateChoicesHk(RE::ExamineMenu* a_menu)
		{
			_UpdateModChoiceList(a_menu);
			if (g_fertilizer.Runs(g_choicesLink) && a_menu) {
				Grey(*a_menu);
			}
		}

		// BUILD on a row, before its box opens.
		bool TryCreateHk(RE::ExamineMenu* a_menu)
		{
			if (OnRow(a_menu, g_tryLink)) {
				if (const auto have = Count(SpoiledStacks(*a_menu)); have < Needed()) {
					Refuse(have);
					return false;
				}
			}
			return _TryCreate(a_menu);
		}

		// The box's yes. The vanilla build would pay experience and report
		// a craft of a recipe that is no form.
		void BuildHk(RE::ExamineMenu* a_menu, bool a_ownerIsWorkbench)
		{
			if (!OnRow(a_menu, g_buildLink)) {
				_BuildConfirmed(a_menu, a_ownerIsWorkbench);
				return;
			}
			Build(*a_menu);
		}

		// Calls from the movie. On NEC's row the BUILD button follows the grey,
		// the requirements show what it needs, and the tag for search does
		// nothing, so the recipe never joins the engine's tagged set.
		void CallHk(RE::ExamineMenu* a_menu, const Params& a_params)
		{
			const auto call = static_cast<Call>(reinterpret_cast<std::uintptr_t>(a_params.userData));
			if ((call != Call::kCheckRequirements && call != Call::kUpdateRequirements && call != Call::kToggleFavoriteMod) ||
				!NecRow(a_menu)) {
				_Call(a_menu, a_params);
				return;
			}
			switch (call) {
			case Call::kCheckRequirements:
				if (a_params.retVal) {
					*a_params.retVal = Value(Count(SpoiledStacks(*a_menu)) >= Needed());
				}
				break;
			case Call::kUpdateRequirements:
				ShowNeed(*a_menu);
				break;
			default:
				if (a_params.retVal) {
					*a_params.retVal = Value(false);
				}
				break;
			}
		}
	}

	void InstallFertilizer(Part a_group)
	{
		const auto& menu = RE::CookingMenu::VTABLE[0];
		{
			// All or none: a row the game builds itself would pay experience and
			// hand out Fertilizer for nothing.
			const CallPatch::Together fertilizer{ a_group };
			_RegisterMenuComponents = CallPatch::PatchSlot(menu, { 0x39, 0x39 }, RegisterHk, "fertilizer row", Part::kNone, true,
				&g_registerLink)
			                              .value_or(0);
			_UpdateModChoiceList = CallPatch::PatchSlot(menu, { 0x3F, 0x3F }, UpdateChoicesHk, "fertilizer grey", Part::kNone, true,
				&g_choicesLink)
			                           .value_or(0);
			_TryCreate = CallPatch::PatchSlot(menu, { 0x1B, 0x1B }, TryCreateHk, "fertilizer refusal", Part::kNone, true, &g_tryLink)
			                 .value_or(0);
			_BuildConfirmed = CallPatch::PatchSlot(menu, { 0x17, 0x17 }, BuildHk, "fertilizer build", Part::kNone, true, &g_buildLink)
			                      .value_or(0);
			g_fertilizer = fertilizer.Set();
		}

		// Stands alone after the set, and another mod here takes nothing, see
		// Part::kFertilizerNeeds. A call that skips NEC lights BUILD on NEC's
		// row and leaves its needs blank, but the refusal still stops a build
		// with too little spoiled food.
		_Call = CallPatch::PatchSlot(menu, { 0x01, 0x01 }, CallHk, "fertilizer calls", Part::kFertilizerNeeds).value_or(0);
	}

	void SayFertilizer()
	{
		if (!g_fertilizer) {
			return;
		}
		const auto* item = FertilizerItem();
		if (!item) {
			REX::WARN("Fallout4.esm's c_Fertilizer has no scrap item, so chemistry stations make no Fertilizer.");
			return;
		}
		if (const auto needed = Needed(); needed > 0) {
			REX::INFO("Chemistry stations make {:s} [{:08X}] from {:d} spoiled food.", RE::TESFullName::GetFullName(*item), item->formID, needed);
		} else {
			REX::INFO("Chemistry stations make no {:s}, since iSpoiledPerFertilizer is 0.", RE::TESFullName::GetFullName(*item));
		}
	}
}
