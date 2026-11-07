#include "UI/Repair/Workbench/Job.h"

#include "Condition/CraftingPerks/CraftingPerks.h"
#include "Condition/Repair.h"
#include "Core/ItemCards.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/Columns/Columns.h"
#include "UI/Repair/Gains.h"
#include "UI/Repair/RepairPrompt.h"
#include "UI/Repair/Restore.h"
#include "UI/Repair/Workbench/Box.h"
#include "UI/Repair/Workbench/Cost.h"
#include "UI/Repair/Workbench/Display.h"
#include "UI/Repair/Workbench/Label.h"
#include "UI/Repair/Workbench/Missing.h"
#include "UI/Repair/Workbench/SpareBox.h"
#include "UI/Repair/Workbench/Workbench.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Workbench
{
	namespace
	{
		using Repair::Debt;
		using Repair::FULL;

		// -------------------------------------------------------------------
		// The job's recipe, its experience and its corner message
		// -------------------------------------------------------------------

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

		// What a craft worth this much pays, by Fallout 4's settings, so a
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

		// An item no mod fits stays greyed once a repair lifts it to the mod
		// lock, so the corner says why, after whatever the repair itself said. Told the
		// item repaired, its name from before the repair, and the level it
		// reached.
		void SayCannotModify(const Selection& a_item, std::string_view a_name, std::uint32_t a_level)
		{
			if (a_level < ModifyFloor() || !NoModFits(a_item)) {
				return;
			}
			const auto said = Text::CannotModify();
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
			TraceLog::Line("menu", "Workbench said {:s} can't be modified, sound and still greyed", a_name);
		}

		// -------------------------------------------------------------------
		// The row the repair box opens on
		// -------------------------------------------------------------------

		// What iRepairBoxRow picks, see Settings.h. Any other number reads as
		// 2, the highest level.
		[[nodiscard]] RepairPrompt::Start BoxStart()
		{
			switch (Settings::iRepairBoxRow.GetValue()) {
			case 0:
				return RepairPrompt::Start::kNone;
			case 1:
				return RepairPrompt::Start::kTop;
			case 3:
				return RepairPrompt::Start::kLowest;
			default:
				return RepairPrompt::Start::kHighest;
			}
		}
	}

	// -------------------------------------------------------------------
	// Pricing a repair and asking how far
	// -------------------------------------------------------------------

	Job& InHand()
	{
		static Job job;
		return job;
	}

	void AskWhichLevel(const Selection& a_selection, const Priced& a_priced, const std::vector<std::uint32_t>& a_offered,
		bool a_spare)
	{
		if (a_offered.empty()) {
			return;
		}

		// Every rank of the perk pricing the item says on its page that such
		// items need fewer components, so the line above the question names the
		// perk and the share it takes off. PriceOf applies it to the price.
		// Left off where no rank is held.
		const auto& standing = a_priced.standing;
		const auto off = CraftingPerks::Discount(standing.rank, standing.ranks);
		const auto over = standing.perk && off > 0 ?
		                      Text::RepairDiscount(standing.Name(), standing.rank, off) :
		                      std::string{};

		// Each level's row ends with what the item's card gains there.
		const auto     gains = Gains::Read(a_selection, a_offered);
		Columns::Table table;
		std::string    spelled;
		for (std::size_t i = 0; i < a_offered.size(); i++) {
			Gains::AddRow(table, { Repair::NameOf(a_offered[i]) }, gains, i);
			spelled += std::format("{:s}{:d}%", spelled.empty() ? "" : " ", a_offered[i]);
			TraceLog::Line("menu", "Workbench level {:d}% of {:s}, its card {:s}", a_offered[i], a_selection.Name(), Gains::Spelled(gains, i));
		}
		auto* bench = OpenBench();
		auto  buttons = Columns::Lay(bench ? bench->uiMovie.get() : nullptr, table, "Workbench repair box");

		const auto credit = off > 0 ?
		                        std::format("{:s} {:d} for {:d}% fewer components", standing.Name(), standing.rank, off) :
		                        std::string{ "nobody" };
		TraceLog::Line("menu",
			"Workbench asks how far to repair {:s} at {:d}%, offering {:s}{:s}, crediting {:s}",
			a_selection.Name(), a_selection.percent, spelled, a_spare ? " under USE A SPARE" : "", credit);

		// The item is found again by its stack's list, which keeps no
		// number that moves.
		std::optional<RepairPrompt::Top> top;
		if (a_spare) {
			top = RepairPrompt::Top{ Text::SpareButton(),
				[object = a_selection.object, extra = a_selection.extra, priced = a_priced] {
					TraceLog::Line("menu", "Workbench USE A SPARE pressed");
					AskWhichSpare(*object, extra, priced);
				} };
		}

		// The bench is looked up again when the answer comes, in case the
		// player walked away in between.
		RepairPrompt::Ask(over, a_selection.Name(), a_selection.percent, std::move(buttons),
			[offered = a_offered, priced = a_priced](std::size_t a_index) {
				TraceLog::Line("menu", "Workbench repair set to {:d}%", offered[a_index]);
				Begin(OpenBench(), offered[a_index], priced);
			},
			[] { TraceLog::Line("menu", "Workbench repair called off"); }, std::move(top), BoxStart());
	}

	void Begin(RE::ExamineMenu* a_menu, std::uint32_t a_level, const Priced& a_priced)
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

		const auto bill = CostOf(a_priced, selection.percent, a_level);

		TraceLog::Line("menu", "Workbench reads {:s} as built from {:s}",
			selection.Name(), Spell(a_priced.built));

		// Who is doing the work and what their perk takes off, before the
		// price.
		if (a_priced.standing.perk) {
			TraceLog::Line("menu",
				"Workbench puts {:s} to {:s}{:s}, the player is {:d} of {:d} ranks in, so a wreck costs {:.2f} rather than {:.2f}",
				selection.Name(), a_priced.standing.Name(),
				a_priced.standing.fromKind ? " by its kind" : "", a_priced.standing.rank,
				a_priced.standing.ranks, a_priced.multiple, Scaled(WRECK_MULTIPLE));
		} else {
			TraceLog::Line("menu", "Workbench found no crafting perk for {:s}, so a wreck costs the whole {:.2f}",
				selection.Name(), a_priced.multiple);
		}

		// How many component units count for each perk, only where the trace
		// file is taking it, since working it out walks the item again.
		if (TraceLog::IsOpen()) {
			TraceLog::Line("menu", "Workbench read that off the parts on {:s}: {:s}",
				selection.Name(), Vote(selection));
		}

		auto& job = InHand();
		job.level = a_level;
		SetParts(bill);

		job.choice = {};
		job.choice.recipe = &JobRecipe();
		job.choice.requiredItems = &job.parts;

		TraceLog::Line("menu",
			"Workbench priced {:s} from {:d}% to {:d}% at {:s}, {:.2f} of what it is built from",
			selection.Name(), selection.percent, a_level, Spell(bill),
			Debt(selection.percent, a_priced.multiple) - Debt(a_level, a_priced.multiple));

		// A repair the bench cannot pay for shows what it lacks, see Missing.h.
		if (ShowMissing(a_menu, job)) {
			Drop();
			return;
		}
		ShowRepairBox(a_menu, selection.Name());
	}

	void Drop()
	{
		// The parts stay for the box of missing parts, see Missing.h.
		auto& job = InHand();
		job.choice.recipe = nullptr;
		job.box = nullptr;
	}

	// -------------------------------------------------------------------
	// Finishing a repair and bringing the bench up to date
	// -------------------------------------------------------------------

	void Finish(RE::ExamineMenu* a_menu)
	{
		const auto selection = Selected(a_menu);
		const auto level = InHand().level;
		auto*      player = RE::PlayerCharacter::GetSingleton();
		if (!a_menu || !selection.Worn() || !player || level <= selection.percent) {
			TraceLog::Line("menu", "Workbench had nothing left to repair");
			Drop();
			return;
		}

		// The name is copied first. The write can merge the stack into an
		// identical one and free it, and paying can empty an entry of the
		// inventory, so the item and its extra data are not read after.
		const auto name = selection.Name();

		// The stack the bench shows, by its number, see Restore.h.
		RE::BGSInventoryItem::CheckStackIDFunctor find{ selection.stack };
		Restore::Write(*player, *selection.object, find, level);

		const auto paid = PayExperience(*player);

		// Spends the job's parts, but not for a job asking nothing, a repair
		// in the free band. That one queues the item itself, since the
		// crafting loop plays only with something queued.
		if (!InHand().parts.empty()) {
			Spend(a_menu);
		} else {
			a_menu->QueueCraftingComponent(selection.object);
			PlayRepairSound(a_menu);
		}

		TraceLog::Line("menu",
			"Workbench repaired {:s} from {:d}% to {:d}%, one of a stack of {:d}, worth {:d} for {:g} experience before Intelligence and perks",
			name, selection.percent, level, selection.count, paid.worth, paid.experience);

		Drop();
		Refresh(a_menu, selection.object->GetFormType());

		// Nothing else on the screen says a free repair was made, with no
		// confirmation, no components leaving and no experience. Then an item
		// no mod fits says why it stays greyed.
		if (InHand().parts.empty()) {
			const auto said = Text::Mended(selection.Armor());
			RE::SendHUDMessage::ShowHUDMessage(said.c_str(), nullptr, true, true);
		}
		SayCannotModify(selection, name, level);
	}

	void SetParts(const std::vector<Materials::Part>& a_parts)
	{
		auto& parts = InHand().parts;
		parts.clear();
		for (const auto& part : a_parts) {
			RE::BGSTypedFormValuePair::SharedVal count{};
			count.i = static_cast<std::int32_t>(part.count);
			parts.push_back({ const_cast<RE::BGSComponent*>(part.component), count });
		}
	}

	Paid PayExperience(RE::PlayerCharacter& a_player)
	{
		// What crafting a mod from the same components pays, the game counting
		// what they are worth as it counts a recipe. False is the path that
		// scales it by Intelligence and runs the experience perks, Idiot Savant
		// among them, as a craft does. True would pay it bare.
		Paid paid;
		paid.worth = RE::TESValueForm::GetFormValue(&JobRecipe(), nullptr);
		paid.experience = CraftExperience(paid.worth);
		if (paid.experience > 0.0F) {
			a_player.RewardExperience(paid.experience, false, nullptr, nullptr);
		}
		return paid;
	}

	void Refresh(RE::ExamineMenu* a_menu, RE::ENUM_FORM_ID a_type)
	{
		a_menu->UpdateItemList(static_cast<std::int32_t>(a_menu->GetSelectedIndex()));
		RebuildModdedItem(a_menu);
		a_menu->UpdateItemCard(false);
		// The vanilla rebuild of the slot list ends in the highlight, which
		// reads the model in the viewer without looking.
		if (a_menu->GetCurrent3D()) {
			a_menu->UpdateModSlotList();
		} else {
			TraceLog::Line("menu", "Workbench left the slot list to the next highlight, the viewer has no model yet");
		}
		const auto after = Selected(a_menu);
		Announce(a_menu, after);
		Flash::Call(a_menu->menuObj, "UpdateButtons");

		// A repair of one out of a stack of several splits the stack, and the
		// row the list is left on holds one of the two.
		if (TraceLog::IsOpen() && after.object) {
			TraceLog::Line("menu", "Workbench now shows {:s} at {:d}%, a stack of {:d}",
				after.Name(), after.percent, after.count);
		}

		// The Pip-Boy lists the repaired item again by itself, see
		// ItemCards.h. Its kind of card is rebuilt as well, once per repair:
		// the apparel cards for a chest piece, the weapon cards for a gun.
		ItemCards::Refresh(a_type);
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
}
