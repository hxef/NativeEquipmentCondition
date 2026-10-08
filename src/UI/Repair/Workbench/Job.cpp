#include "UI/Repair/Workbench/Job.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Repair.h"
#include "Core/ItemCards.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Restore.h"
#include "UI/Repair/Workbench/Cost.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Workbench
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Repair::Debt;
		using Repair::FULL;

		// The job's recipe, with nothing in it but the job's components, see
		// Job.h. Upcasts place each function table where that part of a recipe
		// begins.
		[[nodiscard]] RE::BGSConstructibleObject& JobRecipe()
		{
			alignas(RE::BGSConstructibleObject) static std::byte space[sizeof(RE::BGSConstructibleObject)]{};
			static auto* recipe = [] {
				auto* out = reinterpret_cast<RE::BGSConstructibleObject*>(space);
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::TESForm*>(out)) =
					RE::BGSConstructibleObject::VTABLE[0].address();
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::BGSPickupPutdownSounds*>(out)) =
					RE::BGSConstructibleObject::VTABLE[1].address();
				*reinterpret_cast<std::uintptr_t*>(static_cast<RE::TESDescription*>(out)) =
					RE::BGSConstructibleObject::VTABLE[2].address();
				out->formType = RE::ENUM_FORM_ID::kCOBJ;
				out->requiredItems = &InHand().parts;
				return out;
			}();
			return *recipe;
		}

		// What a craft worth this much pays, by the game's own settings, so a
		// mod that changes them changes this too. Nothing for a craft worth
		// nothing. In float, as the game counts it: in double, components
		// worth 100 would pay 4 rather than 5.
		[[nodiscard]] float CraftExperience(std::uint32_t a_worth)
		{
			auto*       settings = RE::GameSettingCollection::GetSingleton();
			const auto* mult = settings ? settings->GetSetting("fWorkbenchExperienceMult"sv) : nullptr;
			const auto* base = settings ? settings->GetSetting("fWorkbenchExperienceBase"sv) : nullptr;
			const auto* cap = settings ? settings->GetSetting("fWorkbenchExperienceMax"sv) : nullptr;
			if (a_worth == 0 || !mult || !base || !cap) {
				return 0.0F;
			}
			const auto xp = std::floor(static_cast<float>(a_worth) * mult->GetFloat() + base->GetFloat());
			return std::min(cap->GetFloat(), std::max(1.0F, xp));
		}

		// A repair the player cannot pay for gets the game's own words for
		// it. TryCreate then puts up the corner message of sCannotBuildMessage,
		// "You lack the requirements to create this item.". The game reads
		// that setting there and nowhere else. While this lives, it holds the
		// words of sCannotRepairMessage, "You lack the requirements to repair
		// this item.", in the player's language, and it gets its own words
		// back after. The message copies the words as it goes up.
		class ScopedRepairWords
		{
		public:
			ScopedRepairWords()
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

			~ScopedRepairWords()
			{
				if (_build) {
					_build->SetString(const_cast<char*>(_kept));
				}
			}

			ScopedRepairWords(const ScopedRepairWords&) = delete;
			ScopedRepairWords(ScopedRepairWords&&) = delete;
			ScopedRepairWords& operator=(const ScopedRepairWords&) = delete;
			ScopedRepairWords& operator=(ScopedRepairWords&&) = delete;

		private:
			RE::Setting* _build{ nullptr };
			const char*  _kept{ nullptr };
		};

		// Hides the CURRENT MODS heading, or shows it again. An item too worn
		// to modify has its slots closed and an item no mod fits has none, so
		// for either one the heading would be wrong. Hiding the field keeps the
		// translated words for when the item is back at full condition.
		void ShowModsLabel(RE::ExamineMenu* a_menu, bool a_shown)
		{
			Value panel;
			Value label;
			if (!a_menu || !a_menu->menuObj.IsObject() ||
				!a_menu->menuObj.GetMember("ModSlotBase_mc"sv, &panel) || !panel.IsObject() ||
				!panel.GetMember("SlotsLabel_tf"sv, &label) || !label.IsObject()) {
				return;
			}
			label.SetMember("visible"sv, Value(a_shown));
		}

		// An item no mod fits stays greyed once a repair brings it to full, so
		// the corner says why, after whatever the repair itself said. Told the
		// item repaired, its name from before the repair, and the level it
		// reached.
		void SayCannotModify(const Selection& a_item, std::string_view a_name, std::uint32_t a_level)
		{
			if (a_level < FULL || !NoModFits(a_item)) {
				return;
			}
			const auto said = Text::CannotModify();
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			TraceLog::Line("menu", "Workbench said {:s} can't be modified, sound and still greyed", a_name);
		}

		// Called by the bar's list of buttons once for each. The bench's REPAIR
		// button is private to its movie, so it is known by the word it shows,
		// either one, and given the word it should show.
		class Relabel final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				Value text;
				if (a_params.argCount < 1 || !a_params.args[0].IsObject() ||
					!a_params.args[0].GetMember("ButtonText"sv, &text) || !text.IsString()) {
					return;
				}
				const std::string_view shown = text.GetString();
				if (shown != word && (shown == REPAIR_WORD || shown == mend)) {
					a_params.args[0].SetMember("ButtonText"sv, Value(word.c_str()));
					changed = true;
				}
			}

			std::string mend;
			std::string word;
			bool        changed{ false };
		};

		// Lives as long as the plugin, the same way as the item card listener.
		Relabel g_relabel;
	}

	Job& InHand()
	{
		static Job job;
		return job;
	}

	void AskWhichLevel(const Selection& a_selection, const std::vector<std::uint32_t>& a_offered)
	{
		if (a_offered.empty()) {
			return;
		}

		// Every rank of the perk pricing the item says on its page that such
		// items need fewer components, and this is where that discount is
		// applied. Left off where no rank is held.
		const auto standing = PriceOf(a_selection).standing;
		const auto off = CraftingPerks::Discount(standing.rank, standing.ranks);
		const auto over = standing.perk && off > 0 ?
		                      Text::RepairDiscount(standing.Name(), standing.rank, off) :
		                      std::string{};

		std::vector<std::string> buttons;
		std::string              spelled;
		for (const auto level : a_offered) {
			buttons.emplace_back(Repair::NameOf(level));
			spelled += std::format("{:s}{:d}%", spelled.empty() ? "" : " ", level);
		}

		const auto credit = off > 0 ?
		                        std::format("{:s} {:d} for {:d}% fewer components", standing.Name(), standing.rank, off) :
		                        std::string{ "nobody" };
		TraceLog::Line("menu",
			"Workbench asks how far to repair {:s} at {:d}%, offering {:s}, crediting {:s}",
			a_selection.Name(), a_selection.percent, spelled, credit);

		// The bench is looked up again when the answer comes, in case the
		// player walked away in between.
		RepairPrompt::Ask(over, a_selection.Name(), a_selection.percent, std::move(buttons),
			[offered = a_offered](std::size_t a_index) {
				TraceLog::Line("menu", "Workbench repair set to {:d}%", offered[a_index]);
				Begin(OpenBench(), offered[a_index]);
			},
			[] { TraceLog::Line("menu", "Workbench repair called off"); });
	}

	void Begin(RE::ExamineMenu* a_menu, std::uint32_t a_level)
	{
		if (!Repairs()) {
			TraceLog::Line("menu", "Workbench dropped the repair, repairs are off");
			return;
		}

		const auto selection = Selected(a_menu);
		if (!a_menu || !selection.Worn() || a_level <= selection.percent) {
			TraceLog::Line("menu", "Workbench dropped the repair, the bench or the item is gone");
			return;
		}

		const auto priced = PriceOf(selection);
		const auto bill = CostOf(priced, selection.percent, a_level);

		TraceLog::Line("menu", "Workbench reads {:s} as built from {:s}",
			selection.Name(), Spell(priced.built));

		// Who is doing the work and what their perk takes off, before the
		// price.
		if (priced.standing.perk) {
			TraceLog::Line("menu",
				"Workbench puts {:s} to {:s}{:s}, the player is {:d} of {:d} ranks in, so a wreck costs {:.2f} rather than {:.2f}",
				selection.Name(), priced.standing.Name(),
				priced.standing.fromKind ? " by its kind" : "", priced.standing.rank,
				priced.standing.ranks, priced.multiple, Scaled(WRECK_MULTIPLE));
		} else {
			TraceLog::Line("menu", "Workbench found no crafting perk for {:s}, so a wreck costs the whole {:.2f}",
				selection.Name(), priced.multiple);
		}

		// How many component units count for each perk, only where the trace
		// file is taking it, since working it out walks the item again.
		if (TraceLog::IsOpen()) {
			TraceLog::Line("menu", "Workbench read that off the parts on {:s}: {:s}",
				selection.Name(), Vote(selection));
		}

		auto& job = InHand();
		job.level = a_level;
		job.parts.clear();
		for (const auto& part : bill) {
			RE::BGSTypedFormValuePair::SharedVal count{};
			count.i = part.count;
			job.parts.push_back({ const_cast<RE::BGSComponent*>(part.component), count });
		}

		job.choice = {};
		job.choice.recipe = &JobRecipe();
		job.choice.requiredItems = &job.parts;

		TraceLog::Line("menu",
			"Workbench priced {:s} from {:d}% to {:d}% at {:s}, {:.2f} of what it is built from",
			selection.Name(), selection.percent, a_level, Spell(bill),
			Debt(selection.percent, priced.multiple) - Debt(a_level, priced.multiple));

		// The game's own TryCreate by its ID, so a DLL over its slot never
		// sees the repair. The box's callback is the proof the box went up:
		// the game stores each new one in the global it reads here, and its
		// own panel says why it did not when it can. Only the global is read.
		// A job dropped while its box is up turns the yes into the game's own
		// build.
		a_menu->repairing = true;
		static REL::Relocation<bool (*)(RE::ExamineMenu*)> tryCreate{ RE::ID::ExamineMenu::TryCreate };
		const auto* before = RE::ExamineMenu::GetConfirmCallback();
		{
			const ScopedRepairWords words;
			tryCreate(a_menu);
		}
		const auto* after = RE::ExamineMenu::GetConfirmCallback();
		if (!after || after == before || after->thisMenu != a_menu) {
			TraceLog::Line("menu", "Workbench put up no confirmation for the repair to {:d}%, so it dropped it", a_level);
			Drop(a_menu);
		}
	}

	void Drop(RE::ExamineMenu* a_menu)
	{
		if (a_menu) {
			a_menu->repairing = false;
		}
		InHand().choice.recipe = nullptr;
	}

	void Finish(RE::ExamineMenu* a_menu)
	{
		const auto selection = Selected(a_menu);
		const auto level = InHand().level;
		auto*      player = RE::PlayerCharacter::GetSingleton();
		if (!a_menu || !selection.Worn() || !player || level <= selection.percent) {
			TraceLog::Line("menu", "Workbench had nothing left to repair");
			Drop(a_menu);
			return;
		}

		// The name is copied first. The write can merge the stack into an
		// identical one and free it, and paying can empty an entry of the
		// inventory, so the item and its extra data are not read after.
		const auto name = selection.Name();

		// The stack the bench shows, by its number, see Restore.h.
		RE::BGSInventoryItem::CheckStackIDFunctor find{ selection.stack };
		Restore::Write(*player, *selection.object, find, level);

		// What crafting a mod from the same components pays, the game counting
		// what they are worth as it counts a recipe. False is the path that
		// scales it by Intelligence and runs the experience perks, Idiot Savant
		// among them, as a craft does. True would pay it bare.
		const auto worth = RE::TESValueForm::GetFormValue(&JobRecipe(), nullptr);
		const auto gained = CraftExperience(worth);
		if (gained > 0.0F) {
			player->RewardExperience(gained, false, nullptr, nullptr);
		}

		// The bench's own spending, from every container it is linked to. Not
		// called for a job asking nothing, a repair in the free band.
		if (!InHand().parts.empty()) {
			a_menu->ConsumeSelectedItems(true, nullptr);
			a_menu->UpdateOptimizedAutoBuildInv();
		}

		TraceLog::Line("menu",
			"Workbench repaired {:s} from {:d}% to {:d}%, one of a stack of {:d}, worth {:d} for {:g} experience before Intelligence and perks",
			name, selection.percent, level, selection.count, worth, gained);

		Drop(a_menu);

		// Brings the bench up to date: the name in the list, the CND row on the
		// card, and the slot list ungreyed once the item reaches the floor.
		a_menu->UpdateItemList(static_cast<std::int32_t>(a_menu->GetSelectedIndex()));
		RebuildModdedItem(a_menu);
		a_menu->UpdateItemCard(false);
		// The game's rebuild of the slot list ends in the highlight, which
		// reads the model in the viewer without looking.
		if (a_menu->GetCurrent3D()) {
			a_menu->UpdateModSlotList();
		} else {
			TraceLog::Line("menu", "Workbench left the slot list to the next highlight, the viewer has no model yet");
		}
		const auto after = Selected(a_menu);
		Announce(a_menu, after);
		a_menu->menuObj.Invoke("UpdateButtons");

		// A repair of one out of a stack of several splits the stack, and the
		// row the list is left on holds one of the two.
		if (TraceLog::IsOpen() && after.object) {
			TraceLog::Line("menu", "Workbench now shows {:s} at {:d}%, a stack of {:d}",
				after.Name(), after.percent, after.count);
		}

		// The Pip-Boy lists the repaired item again by itself, see
		// ItemCards.h. Its kind of card is rebuilt as well, once per repair:
		// the apparel cards for a chest piece, the weapon cards for a gun.
		ItemCards::Refresh(selection.object->GetFormType());

		// Nothing else on the screen says a free repair was made, with no
		// confirmation, no components leaving and no experience. Then an item
		// no mod fits says why it stays greyed.
		if (InHand().parts.empty()) {
			const auto said = Text::Mended(selection.Armor());
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
		}
		SayCannotModify(selection, name, level);
	}

	void Mend(RE::ExamineMenu* a_menu)
	{
		auto& job = InHand();
		job.level = FULL;
		job.parts.clear();
		job.choice = {};
		job.choice.recipe = &JobRecipe();
		job.choice.requiredItems = &job.parts;

		if (FreeRepairs()) {
			TraceLog::Line("menu", "Workbench mended {:s} for nothing, fBenchCostMult is 0", Selected(a_menu).Name());
		} else {
			TraceLog::Line("menu", "Workbench mended {:s} for nothing, it was above {:d}%",
				Selected(a_menu).Name(), FreeAbove());
		}
		Finish(a_menu);
	}

	void Announce(RE::ExamineMenu* a_menu, const Selection& a_selection)
	{
		if (!a_menu || !a_menu->menuObj.IsObject()) {
			return;
		}
		a_menu->menuObj.SetMember("allowRepair"sv, Value(a_selection.Worn()));
		ShowModsLabel(a_menu, !a_selection.TooWorn() && !NoModFits(a_selection));
	}

	void Label(RE::ExamineMenu* a_menu, const Selection& a_selection)
	{
		auto* bar = a_menu ? a_menu->buttonHintBar.get() : nullptr;
		if (!a_selection.object || !bar || !a_menu->uiMovie || !bar->sourceButtons.IsObject()) {
			return;
		}

		const bool trifling = a_selection.Trifling();
		g_relabel.mend = Text::MendButton();
		g_relabel.word = trifling ? g_relabel.mend : std::string{ REPAIR_WORD };
		g_relabel.changed = false;

		Value visit;
		a_menu->uiMovie->CreateFunction(&visit, &g_relabel);
		bar->sourceButtons.Invoke("forEach", nullptr, &visit, 1);

		if (g_relabel.changed) {
			TraceLog::Line("menu", "Workbench button reads {:s} for {:s} at {:d}%",
				trifling ? "MEND" : "REPAIR", a_selection.Name(), a_selection.percent);
		}
	}
}
