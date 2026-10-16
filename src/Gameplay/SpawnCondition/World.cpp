#include "Gameplay/SpawnCondition/World.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Handover.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// The mark
		// -------------------------------------------------------------------

		// World marks get a block of their own, since they are not inventory
		// stacks. Works nothing out while no trace file is open.
		void Tell(const RE::TESObjectREFR& a_ref, const RE::TESBoundObject& a_base, std::string_view a_what)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}
			TraceLog::Group("WORLD", "weapons and armor lying in the world");
			TraceLog::Line("world", "{:<30s} [{:08X}] ref [{:08X}] {:s}", NameOf(a_base, "unnamed item"), a_base.formID,
				a_ref.formID, a_what);
		}

		[[nodiscard]] float HealthOfRef(RE::TESObjectREFR& a_ref)
		{
			auto* extra = a_ref.extraList.get();
			return extra ? extra->GetHealthPerc() : Condition::INVALID_HEALTH;
		}

		// The change is recorded first, and the mark is written only when the
		// game took it, so a mark is never kept for this session alone.
		bool Mark(RE::TESObjectREFR& a_ref, RE::ExtraDataList& a_extra)
		{
			if (!a_ref.AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
				return false;
			}
			a_extra.SetHealthPerc(Condition::UNROLLED);
			return true;
		}

		// Takes the mark off, so the item stacks with its copies again.
		// Written even when the game records nothing now, since the mark was
		// recorded as it went on, so the save sees it go.
		void Unmark(RE::TESObjectREFR& a_ref, RE::ExtraDataList& a_extra)
		{
			a_ref.AddChange(RE::CHANGE_TYPES::kObjectExtraItemData);
			a_extra.SetHealthPerc(Condition::INVALID_HEALTH);
		}

		// -------------------------------------------------------------------
		// Set 1: a new reference, and a drop
		// -------------------------------------------------------------------

		// The NEW_REFR_DATA the drop in progress on this thread hands to
		// CreateReferenceAtLocation, or null. A drop looks exactly like a
		// script placing 1 item, so this is what keeps the mark off it.
		thread_local const RE::NEW_REFR_DATA* t_drop = nullptr;

		// HandlePre3D, slot 0 of NEW_REFR_DATA's table, which does nothing in
		// the game. CreateReferenceAtLocation calls it once the new reference
		// has its base and its extra data, and records that extra data right
		// after, so the mark is saved with no AddChange here.
		REL::Relocation<void (*)(RE::NEW_REFR_DATA*, RE::TESObjectREFR*)> _HandlePre3D;
		CallPatch::Link<void (*)(RE::NEW_REFR_DATA*, RE::TESObjectREFR*)>  g_newRefLink;
		CallPatch::Held                                                    g_created;

		// Marks a weapon or piece of armor a script, the console or the game
		// places with no condition. Calls what it found first, so a health
		// another mod writes there wins. A save rebuilding a reference it holds
		// names that reference, and a drop is the item that left an
		// inventory, so both are left alone.
		void NewReferenceHk(RE::NEW_REFR_DATA* a_data, RE::TESObjectREFR* a_ref)
		{
			_HandlePre3D(a_data, a_ref);
			if (!Settings::bSpawnCondition.GetValue() || !g_created.Runs(g_newRefLink) || !a_data || !a_ref ||
				a_data->reference || a_data == t_drop || Loading() || NotRecording()) {
				return;
			}
			auto*       extra = a_ref->extraList.get();
			const auto* base = a_ref->GetObjectReference();
			if (!extra || !base || a_ref->IsTemporary() || a_ref->formID == 0 || a_ref->IsDeleted() ||
				!Condition::WearsOut(*base) || extra->GetHealthPerc() >= 0.0F) {
				return;
			}
			extra->SetHealthPerc(Condition::UNROLLED);
			Tell(*a_ref, *base, "marked at creation");
		}

		// The one call in TESObjectREFR::DropIntoWorld.
		constexpr CallPatch::CallSite DROP_SITE[] = {
			{ 2200922, 0x254, "drop" },
		};

		// The handle comes back through the 2nd argument, as in Upkeep.cpp's
		// BuildHk.
		using Create_t = RE::ObjectRefHandle* (*)(RE::TESDataHandler*, RE::ObjectRefHandle*, RE::NEW_REFR_DATA*);
		std::array<CallPatch::Link<Create_t>, 1> g_dropLink;

		// Runs is asked only so a set waiting on this place can end. The scope
		// goes up either way, since all it does is keep the mark off a drop.
		RE::ObjectRefHandle* DropHk(RE::TESDataHandler* a_this, RE::ObjectRefHandle* a_out, RE::NEW_REFR_DATA* a_data)
		{
			static_cast<void>(g_created.Runs(g_dropLink[0]));
			const Scoped<const RE::NEW_REFR_DATA*> drop{ t_drop, a_data };
			return g_dropLink[0](a_this, a_out, a_data);
		}

		void InstallCreated()
		{
			const CallPatch::Together created{ Part::kWorldLoot };
			const REL::Relocation<std::uintptr_t> table{ RE::NEW_REFR_DATA::VTABLE[0] };
			_HandlePre3D = CallPatch::PatchSlot(table, 0x00, NewReferenceHk, "new reference", Part::kNone, true, &g_newRefLink).value_or(0);
			const auto dropHooks = CallPatch::PerSite<std::size(DROP_SITE)>([]<std::size_t I>() { return &DropHk; });
			CallPatch::PatchAll(DROP_SITE, RE::ID::TESDataHandler::CreateReferenceAtLocation, dropHooks, g_dropLink,
				"Weapons and armor a script or the console places in the world roll for their first holder, and a dropped one keeps its condition");
			g_created = created.Set();
			if (!g_created) {
				REX::WARN("Weapons and armor a script or the console places in the world arrive at full condition.");
			}
		}

		// -------------------------------------------------------------------
		// Set 2: a world item picked again from its leveled list
		// -------------------------------------------------------------------

		// The calls to TESObjectREFR::RerollLeveledItemBase, which picks a world
		// item's base again from the leveled list that placed it. It returns at
		// once for an item no leveled list placed, a plain placed gun for one.
		enum class Road : std::uint8_t
		{
			kFirstAttach,  // a cell attaching for the first time, so every item there is new
			kRespawn,      // a reference whose respawn time ran out, put back where the editor placed it
			kReset,        // a cell resetting what nobody changed, only a real reroll counts
			kEnable,       // a reference enabled, only a real reroll counts
		};

		constexpr CallPatch::CallSite REROLL_SITES[] = {
			{ 2200352, 0x828, "first attach reroll" },
			{ 2200352, 0x638, "respawn reroll" },
			{ 2200547, 0x03C, "unchanged reroll", true },
			{ RE::ID::TESObjectREFR::Enable.id(), 0x1DB, "enable reroll" },
		};
		constexpr Road ROADS[] = { Road::kFirstAttach, Road::kRespawn, Road::kReset, Road::kEnable };
		static_assert(std::size(ROADS) == std::size(REROLL_SITES));

		// The first of the reroll's 2 calls to ExtraDataList::RemoveByType,
		// the one that clears the old mods. It runs once on every reroll and
		// never without one, so it tells a real reroll from a call that
		// changed nothing.
		constexpr CallPatch::CallSite REROLL_MODS_SITE[] = {
			{ RE::ID::TESObjectREFR::RerollLeveledItemBase.id(), 0x157, "reroll mods" },
		};

		thread_local bool t_rerolled = false;

		std::array<CallPatch::Link<void (*)(RE::TESObjectREFR*)>, std::size(REROLL_SITES)> g_rerollLinks;
		std::array<CallPatch::Link<bool (*)(RE::ExtraDataList*, std::uint8_t)>, 1>          g_rerollModsLink;
		CallPatch::Held                                                                     g_rerolls;

		// The type goes as a byte. The engine sets only the low byte, so a
		// 4 byte enum here would read 3 bytes of junk.
		bool RerollModsHk(RE::ExtraDataList* a_extra, std::uint8_t a_type)
		{
			if (g_rerolls.Runs(g_rerollModsLink[0])) {
				t_rerolled = true;
			}
			return g_rerollModsLink[0](a_extra, a_type);
		}

		// Enable writes only into a loaded cell, as Enable itself asks. A
		// reference whose saved changes the game has not read yet records
		// nothing, though AddChange says it did.
		[[nodiscard]] bool CellLoaded(const RE::TESObjectREFR& a_ref)
		{
			const auto* tes = RE::TES::GetSingleton();
			auto*       cell = a_ref.GetParentCell();
			return tes && cell && tes->IsCellLoaded(cell, false);
		}

		// What a world item gets after the call. a_before is its health before
		// it, and a_rerolled says whether the game really picked it again. A
		// rerolled item is a new item, even as the same weapon, so it rolls
		// again for its next holder.
		void AfterReroll(RE::TESObjectREFR& a_ref, float a_before, bool a_rerolled, Road a_road)
		{
			auto*       extra = a_ref.extraList.get();
			const auto* base = a_ref.GetObjectReference();
			if (!extra || !base) {
				return;
			}
			const bool takes = Condition::WearsOut(*base);
			const bool plainRoad = a_road == Road::kFirstAttach || a_road == Road::kRespawn;
			if (!a_rerolled && (!takes || !plainRoad)) {
				return;
			}

			const auto health = extra->GetHealthPerc();
			const bool unrolled = Condition::IsUnrolled(health);
			if (a_rerolled) {
				// Rerolled into a grenade, ammo or a hat that gives nothing,
				// the mark comes off. A health the list entry wrote is kept.
				if (!takes) {
					if (unrolled) {
						Unmark(a_ref, *extra);
						Tell(a_ref, *base, "unmarked after reroll");
					}
				} else if (health != a_before) {
					Tell(a_ref, *base, "kept after reroll");
				} else if (Mark(a_ref, *extra)) {
					Tell(a_ref, *base, "marked on reroll");
				}
				return;
			}

			// A plain placed item. A respawn puts back the very reference a
			// pickup copied, so it can still hold its old mark, which is
			// recorded again so it outlives a save and reload. A trader's
			// loose item gets its mark back once the trade that bought it is
			// done, see Handover.cpp.
			if (health >= 0.0F && !(unrolled && a_road == Road::kRespawn)) {
				return;
			}
			if (!Mark(a_ref, *extra)) {
				return;
			}
			if (a_road == Road::kFirstAttach) {
				Tell(a_ref, *base, "marked in the world");
			} else {
				Tell(a_ref, *base, unrolled ? "kept its mark on respawn" : "marked on respawn");
			}
		}

		// The game's own result while off or while a save is applied, since
		// the load puts back what the save holds. A created reference has
		// its mark from set 1.
		template <std::size_t I>
		void RerollHk(RE::TESObjectREFR* a_ref)
		{
			constexpr auto road = ROADS[I];
			if (!Settings::bSpawnCondition.GetValue() || !g_rerolls.Runs(g_rerollLinks[I]) || !a_ref || Loading() ||
				a_ref->IsCreated() || a_ref->IsDeleted() || (road == Road::kEnable && !CellLoaded(*a_ref))) {
				return g_rerollLinks[I](a_ref);
			}
			const auto before = HealthOfRef(*a_ref);
			bool       rerolled = false;
			{
				const Scoped watch{ t_rerolled, false };
				g_rerollLinks[I](a_ref);
				rerolled = t_rerolled;
			}
			AfterReroll(*a_ref, before, rerolled, road);
		}

		void InstallRerolls()
		{
			const CallPatch::Together rerolls{ Part::kWorldLoot };
			const auto                hooks = CallPatch::PerSite<std::size(REROLL_SITES)>([]<std::size_t I>() { return &RerollHk<I>; });
			CallPatch::PatchAll(REROLL_SITES, RE::ID::TESObjectREFR::RerollLeveledItemBase, hooks, g_rerollLinks,
				"Weapons and armor lying where the game placed them roll for their first holder, and again after a respawn");
			const auto modsHooks = CallPatch::PerSite<std::size(REROLL_MODS_SITE)>([]<std::size_t I>() { return &RerollModsHk; });
			CallPatch::PatchAll(REROLL_MODS_SITE, RE::ID::ExtraDataList::RemoveByType, modsHooks, g_rerollModsLink,
				"A leveled weapon or piece of armor the game picks again rolls again, even as the same item");
			g_rerolls = rerolls.Set();
			if (!g_rerolls) {
				REX::WARN("Weapons and armor lying where the game placed them arrive at full condition.");
			}
		}

		// -------------------------------------------------------------------
		// Set 3: an item placed to spawn inside a container
		// -------------------------------------------------------------------

		// TESObjectREFR::InitItemImpl calling the pickup road, for an item
		// placed to spawn inside a container, see ScopedSpawnContainer.
		constexpr CallPatch::CallSite SPAWN_CONTAINER_SITE[] = {
			{ 2200727, 0x218, "spawn container" },
		};

		std::array<CallPatch::Link<void (*)(RE::TESObjectREFR*, RE::TESObjectREFR*, std::uint32_t)>, 1> g_spawnContainerLink;
		CallPatch::Held                                                                                  g_spawnContainer;

		void SpawnContainerHk(RE::TESObjectREFR* a_container, RE::TESObjectREFR* a_item, std::uint32_t a_count)
		{
			if (!g_spawnContainer.Runs(g_spawnContainerLink[0])) {
				return g_spawnContainerLink[0](a_container, a_item, a_count);
			}
			const ScopedSpawnContainer born;
			g_spawnContainerLink[0](a_container, a_item, a_count);
		}

		void InstallSpawnContainer()
		{
			const CallPatch::Together spawnContainer{ Part::kWorldLoot };
			const auto                hooks = CallPatch::PerSite<std::size(SPAWN_CONTAINER_SITE)>([]<std::size_t I>() { return &SpawnContainerHk; });
			CallPatch::PatchAll(SPAWN_CONTAINER_SITE, RE::ID::TESObjectREFR::MoveRefIntoInventory, hooks, g_spawnContainerLink,
				"Weapons and armor placed to spawn inside a container roll as that container's own");
			g_spawnContainer = spawnContainer.Set();
			if (!g_spawnContainer) {
				REX::WARN("Weapons and armor placed to spawn inside a container can arrive at full condition.");
			}
		}

		// -------------------------------------------------------------------
		// Set 4: what scripts and the console read
		// -------------------------------------------------------------------

		// The 2 callers of TESObjectREFR::GetItemHealthPercent: the condition
		// and console function, and ObjectReference.GetItemHealthPercent for
		// scripts, a tail jump. In an unmodified game a loose weapon reads -1
		// there, so a marked one does too.
		constexpr CallPatch::CallSite ITEM_HEALTH_SITES[] = {
			{ 2203770, 0x018, "condition item health" },
			{ 2254255, 0x003, "script item health", true },
		};

		std::array<CallPatch::Link<float (*)(RE::TESObjectREFR*)>, std::size(ITEM_HEALTH_SITES)> g_itemHealthLinks;
		CallPatch::Held                                                                          g_itemHealth;

		template <std::size_t I>
		float ItemHealthHk(RE::TESObjectREFR* a_ref)
		{
			const auto health = g_itemHealthLinks[I](a_ref);
			if (!Settings::bSpawnCondition.GetValue() || !g_itemHealth.Runs(g_itemHealthLinks[I])) {
				return health;
			}
			return Condition::IsUnrolled(health) ? Condition::INVALID_HEALTH : health;
		}

		void InstallItemHealth()
		{
			const CallPatch::Together itemHealth{ Part::kWorldLoot };
			const auto                hooks = CallPatch::PerSite<std::size(ITEM_HEALTH_SITES)>([]<std::size_t I>() { return &ItemHealthHk<I>; });
			CallPatch::PatchAll(ITEM_HEALTH_SITES, RE::ID::TESObjectREFR::GetItemHealthPercent, hooks, g_itemHealthLinks,
				"Scripts and the console read a loose weapon or piece of armor nobody has rolled yet as having no condition");
			g_itemHealth = itemHealth.Set();
			if (!g_itemHealth) {
				REX::WARN("Scripts and the console read a loose weapon or piece of armor nobody has rolled yet as full.");
			}
		}
	}

	// Each set works whole or not at all, and a lost set leaves the rest
	// working, see CallPatch::Together.
	void InstallWorld()
	{
		InstallCreated();
		InstallRerolls();
		InstallSpawnContainer();
		InstallItemHealth();
	}
}
