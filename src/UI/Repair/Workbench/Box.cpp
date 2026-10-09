#include "UI/Repair/Workbench/Box.h"

#include "Core/TraceLog.h"
#include "UI/Repair/Workbench/Job.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace Workbench
{
	namespace
	{
		// -------------------------------------------------------------------
		// The box and what it does on yes or no
		// -------------------------------------------------------------------

		// What the box asks, in words the game already translates.
		constexpr const char* QUESTION = "$Repair";

		// The items the bench takes for the job's parts, an item and a count
		// each, junk standing in for the components it holds.
		using Taken = RE::BSScrapArray<RE::BSTTuple<RE::TESBoundObject*, std::uint32_t>>;

		// NEC's callback behind the box. The bench calls OnAccept on yes and
		// then frees it, and on no only frees it, on its first frame after
		// the box closes. Its constructor writes NEC's function table over the
		// game's one the base class wrote, so it is never marked novtable.
		class RepairBox final : public RE::ExamineConfirmMenu::ICallback
		{
		public:
			explicit RepairBox(RE::ExamineMenu* a_menu) :
				ICallback(a_menu)
			{}

			// Also runs inside the bench's own destructor when it closes with
			// the box up, so it touches nothing but the job.
			~RepairBox() override
			{
				const auto& job = InHand();
				if (job.box != this) {
					return;
				}
				TraceLog::Line("menu", "Workbench repair to {:d}% turned down at the confirmation", job.level);
				Drop();
			}

			void OnAccept() override
			{
				if (InHand().box != this) {
					TraceLog::Line("menu", "Workbench confirmation came back with no repair in hand");
					return;
				}
				Finish(thisMenu);
			}
		};
	}

	void ShowRepairBox(RE::ExamineMenu* a_menu)
	{
		auto& job = InHand();

		// What the bench would take, which the box reads as it opens, read
		// off the bench the way TryCreate reads it for a mod.
		Taken taken;
		a_menu->GetRequiredComponents(&job.parts, &a_menu->optimizedAutoBuildInv, &taken);

		// The game frees the callback and the data with its own allocator,
		// which CommonLibF4's operator new and Create use. The data copies
		// the list, so the list goes with this function.
		auto* box = new RepairBox(a_menu);
		auto* data = RE::ExamineConfirmMenu::InitDataBuild::Create(QUESTION, REPAIR_WORD, &job.parts, &taken);
		if (!data) {
			delete box;
			TraceLog::Line("menu", "Workbench put up no confirmation for the repair to {:d}%, so it dropped it", job.level);
			Drop();
			return;
		}

		// Set first, since putting the box up frees the callback of any box
		// before it.
		job.box = box;
		a_menu->ShowConfirmMenu(data, box);

		// The game keeps the callback of the box it put up last.
		if (RE::ExamineMenu::GetConfirmCallback() != box) {
			TraceLog::Line("menu", "Workbench put up no confirmation for the repair to {:d}%, so it dropped it", job.level);
			Drop();
		}
	}

	// -------------------------------------------------------------------
	// Taking the parts and playing the sound
	// -------------------------------------------------------------------

	void Spend(RE::ExamineMenu* a_menu)
	{
		const auto& parts = InHand().parts;
		Taken       taken;
		a_menu->GetRequiredComponents(&parts, &a_menu->optimizedAutoBuildInv, &taken);

		// What junk holds beyond the job goes to the bench's own container, or
		// to the player at a bench with none. Finish has made sure of the
		// player.
		RE::TESObjectREFR* leftovers = a_menu->workbenchContainerRef.get();
		if (!leftovers) {
			leftovers = RE::PlayerCharacter::GetSingleton();
		}
		RE::ExamineMenu::ResolveRecipeComponents(&parts, &taken, leftovers, false);

		std::string spent;
		for (const auto& item : taken) {
			a_menu->RemoveObjectFromContainers(item.first, item.second);
			if (TraceLog::IsOpen() && item.first) {
				spent += std::format("{:s}{:d} {:s}", spent.empty() ? "" : ", ", item.second,
					RE::TESFullName::GetFullName(*item.first));
			}
		}

		// Each part's crafting sound plays under the crafting loop, queued and
		// played the way the game's ConsumeSelectedItems does it for a mod.
		for (const auto& part : parts) {
			a_menu->QueueCraftingComponent(part.first);
		}
		PlayRepairSound(a_menu);
		a_menu->UpdateOptimizedAutoBuildInv();

		TraceLog::Line("menu", "Workbench took {:s} from what the bench can reach, the player included",
			spent.empty() ? "nothing"sv : std::string_view{ spent });
	}

	void PlayRepairSound(RE::ExamineMenu* a_menu)
	{
		// The loop the game plays for a mod built at a weapons or armor bench,
		// looked up by its name each time.
		auto*      store = RE::TESForm::GetFormByEditorID<RE::BGSDefaultObject>("CraftingLoopSound_Examine_DO");
		auto*      loop = store ? store->GetForm<RE::BGSSoundDescriptorForm>() : nullptr;
		const auto queued = a_menu->queuedCraftingComponents.size();
		if (!loop) {
			TraceLog::Line("menu", "Workbench played no crafting sound, the game has none set for a mod built at a bench");
			return;
		}
		a_menu->PlayCraftSound(loop);
		TraceLog::Line("menu", "Workbench played the crafting sound [{:08X}] over {:d} queued sounds",
			loop->GetFormID(), queued);
	}
}
