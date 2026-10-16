#include "Gameplay/SpawnCondition/Handover.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Gameplay/SpawnCondition/Guards.h"
#include "Gameplay/SpawnCondition/Owners.h"
#include "Gameplay/SpawnCondition/Roll.h"
#include "Gameplay/SpawnCondition/Trace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace SpawnCondition
{
	namespace
	{
		// -------------------------------------------------------------------
		// The marks
		// -------------------------------------------------------------------

		// Each lives on the thread that raised it, for one call, so no lock is
		// needed and nothing outlives the call.
		thread_local const Handing* t_handing = nullptr;
		thread_local bool           t_spawnContainer = false;
		thread_local bool           t_spawnDupe = false;

		// -------------------------------------------------------------------
		// The handover calls
		// -------------------------------------------------------------------

		// The 3 calls to TESObjectREFR::AddInventoryItem that move an item
		// that already exists. Each is a main place: another mod holding one
		// leaves all of Worn loot to it.
		//
		// The pickup is MoveRefIntoInventory, the road for an item lying in
		// the world. A pickup by anyone, a purchase off a counter, workshop
		// store, a script adding a reference, console AddToContainer and a
		// quest alias placed in an inventory all take it.
		//
		// The other 2 are the slot 0x7A body of a reference, and
		// TESObjectREFR::AddItemIfContainer, which the actor's slot 0x7A body,
		// the player's too, adds through. They are the receiving half of
		// RemoveItem with a target: trades, followers and looting a body.
		// Death items, a perk's Add Leveled List and harvesting a plant come
		// through with no old container, and those are births. RemoveItem's
		// own call goes through a register form CallPatch cannot patch, so
		// these 2 are patched instead.
		constexpr CallPatch::CallSite HANDOVER_SITES[] = {
			{ RE::ID::TESObjectREFR::MoveRefIntoInventory.id(), 0x2E5, "pickup" },
			{ 2201031, 0x2A3, "handover" },
			{ 2200918, 0x06F, "actor handover" },
		};

		// The list goes by value. AddInventoryItem releases the copy it is
		// handed and its caller takes a reference for it, so a hook declared
		// the same way hands a copy on and drops its own, which balances.
		using AddInventoryItem_t = void (*)(RE::TESObjectREFR*, RE::TESBoundObject*, RE::BSTSmartPointer<RE::ExtraDataList>,
			std::uint32_t, RE::TESObjectREFR*, const RE::INSTANCE_FILTER*, RE::TESObjectREFR*);
		std::array<CallPatch::Link<AddInventoryItem_t>, std::size(HANDOVER_SITES)> g_handoverLinks;

		// A world item the trade on this thread bought, and how many copies.
		struct Bought
		{
			RE::ObjectRefHandle world;
			std::uint32_t       count{ 0 };
		};

		// Only TradeDone empties it, and nothing calls that when the barter
		// trade's place is not patched, so it keeps at most BOUGHT_MAX items.
		// A later one keeps its roll.
		constexpr std::size_t            BOUGHT_MAX = 16;
		thread_local std::vector<Bought> t_bought;

		// A trader's loose item the player just bought, see LooseItem. While
		// the barter screen is open, the player picks up from the world only
		// by buying, short of a script. The purchase takes the item through
		// the pickup road and the game then deletes the world item. The roll
		// it took at the listing now belongs to the bought copy, so the world
		// item gets its mark back once the trade is done, see TradeDone, and
		// a respawn puts it back to roll again like any placed item. Until
		// then the roll stays, since a screen that lists the item 1 copy at a
		// time has the trade pick up the same world item once per copy.
		//
		// A persistent item hands over its own list, so a_handed is that list
		// and nothing is noted. A created item never respawns. A health the
		// item's plugin gave it is its own, and the game records no change
		// for it, while NEC records one before each mark and roll it writes
		// into a world item.
		void NoteBought(const RE::TESObjectREFR& a_receiver, const RE::ExtraDataList* a_handed, RE::TESObjectREFR* a_world,
			std::uint32_t a_count)
		{
			if (!a_world || !a_receiver.IsPlayerRef() || a_world->IsCreated() || !RE::TESDataHandler::GetBarterContainer()) {
				return;
			}
			auto*       extra = a_world->extraList.get();
			const auto* base = a_world->GetObjectReference();
			if (!extra || extra == a_handed || !base || !Condition::WearsOut(*base)) {
				return;
			}
			const auto  held = extra->GetHealthPerc();
			const auto* save = RE::BGSSaveLoadGame::GetSingleton();
			if (held < 0.0F || Condition::IsUnrolled(held) || !save ||
				!save->HasChangeFlags(a_world, RE::CHANGE_TYPES::kObjectExtraItemData)) {
				return;
			}
			const auto handle = a_world->GetHandle();
			for (auto& bought : t_bought) {
				if (bought.world == handle) {
					bought.count += a_count;
					return;
				}
			}
			if (t_bought.size() < BOUGHT_MAX) {
				t_bought.push_back({ handle, a_count });
			}
		}

		// Raises the mark the AddStack hook reads. The pickup is from the
		// world unless it moves an item placed to spawn inside a container,
		// see ScopedSpawnContainer. The other 2 count only with an old
		// container. The handed list is kept as a bare pointer for
		// NoteBought, which only compares it.
		template <std::size_t I>
		void HandoverHk(RE::TESObjectREFR* a_receiver, RE::TESBoundObject* a_object, RE::BSTSmartPointer<RE::ExtraDataList> a_extra,
			std::uint32_t a_count, RE::TESObjectREFR* a_oldContainer, const RE::INSTANCE_FILTER* a_filter, RE::TESObjectREFR* a_worldRef)
		{
			constexpr bool fromWorld = I == 0;
			const bool     marks = Settings::bSpawnCondition.GetValue() && g_handoverLinks[I].Live() &&
			                   (fromWorld ? !t_spawnContainer : a_oldContainer != nullptr);
			const auto*    handed = a_extra.get();
			{
				const Handing handing{ a_object, a_receiver, fromWorld };
				const Scoped  mark{ t_handing, marks ? &handing : t_handing };
				g_handoverLinks[I](a_receiver, a_object, std::move(a_extra), a_count, a_oldContainer, a_filter, a_worldRef);
			}
			if (fromWorld && marks && a_receiver) {
				NoteBought(*a_receiver, handed, a_worldRef, a_count);
			}
		}

		// -------------------------------------------------------------------
		// SpawnDupe
		// -------------------------------------------------------------------

		// The console's SpawnDupe adds each stack of the character it copies
		// with that stack's own extra data list, so a write would reach the
		// character too. The command's other copy call cannot run and is left
		// alone.
		constexpr CallPatch::CallSite SPAWN_DUPE_SITE[] = {
			{ 2205763, 0x0D3, "spawndupe" },
		};

		std::array<CallPatch::Link<AddInventoryItem_t>, 1> g_spawnDupeLink;
		CallPatch::Held                                    g_spawnDupe;

		void SpawnDupeHk(RE::TESObjectREFR* a_receiver, RE::TESBoundObject* a_object, RE::BSTSmartPointer<RE::ExtraDataList> a_extra,
			std::uint32_t a_count, RE::TESObjectREFR* a_oldContainer, const RE::INSTANCE_FILTER* a_filter, RE::TESObjectREFR* a_worldRef)
		{
			const Scoped noWrite{ t_spawnDupe, t_spawnDupe || (Settings::bSpawnCondition.GetValue() && g_spawnDupe.Runs(g_spawnDupeLink[0])) };
			g_spawnDupeLink[0](a_receiver, a_object, std::move(a_extra), a_count, a_oldContainer, a_filter, a_worldRef);
		}
	}

	// -------------------------------------------------------------------
	// What a handover writes
	// -------------------------------------------------------------------

	const Handing* HandingOver(const RE::TESBoundObject* a_object, const RE::BGSInventoryList* a_list)
	{
		const auto* handing = t_handing;
		return handing && a_list && handing->object == a_object && handing->receiver->inventoryList == a_list ? handing : nullptr;
	}

	// Nothing here asks whether an item is older than the install. A stack
	// with no health is written full. A world item still marked as not rolled
	// gets its first roll when it is picked up from the world and goes to
	// full otherwise. A health of 0 or more is the item's own.
	std::optional<Aim> HandOver(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack,
		const Handing& a_handing)
	{
		if (!a_object || !a_stack) {
			return std::nullopt;
		}

		// A tin can only matters when it still carries the mark, from a world
		// item rerolled into something with no condition.
		const bool gear = Gear(*a_object);
		auto*      extra = a_stack->extra.get();
		if (!gear && !(extra && Condition::IsUnrolled(extra->GetHealthPerc()))) {
			return std::nullopt;
		}

		const auto name = NameOf(*a_object, "unnamed item");
		const auto id = a_object->formID;
		const auto count = a_stack->count;
		const auto  players = Players(a_list);
		const auto* restock = Restocking(a_list);
		const auto  listed = players || restock;
		OpenBlock(restock);

		// Gear that gives nothing keeps whatever an older NEC left on it. The
		// mark comes off, so the item stacks with its copies again.
		if (const auto* why = gear ? Condition::WhyNoCondition(*a_object) : "not a weapon or armor") {
			if (extra && Condition::IsUnrolled(extra->GetHealthPerc())) {
				extra->SetHealthPerc(Condition::INVALID_HEALTH);
				Report(listed, "mark taken off at a handover", "{:<30s} [{:08X}] x{:<3d} mark taken off at a handover, {:s}",
					name, id, count, why);
			} else {
				Report(listed, why, "{:<30s} [{:08X}] x{:<3d} left alone, {:s}", name, id, count, why);
			}
			return std::nullopt;
		}

		if (!extra) {
			Report(listed, "with no extradata", "{:<30s} [{:08X}] x{:<3d} left alone, no extradata to write to", name, id, count);
			return std::nullopt;
		}

		// A script's gift to the player arrives new, as in Roll.
		const auto before = extra->GetHealthPerc();
		if (players && GiftFromScript(*extra)) {
			extra->SetHealthPerc(Condition::MAX_HEALTH);
			Report(listed, "a gift", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a gift, was {:.4f}",
				name, id, count, Condition::MAX_HEALTH, before);
			return std::nullopt;
		}

		// A world item's first holder rolls it with every rule of Roll, in
		// that holder's band.
		const auto unrolled = Condition::IsUnrolled(before);
		if (unrolled && a_handing.fromWorld) {
			Report(listed, "first owner of a world item", "{:<30s} [{:08X}] x{:<3d} first owner of a world item", name, id, count);
			return Roll(a_list, a_object, a_stack);
		}

		// Old loot, or a marked item that left the world while Worn loot was
		// off, goes to full in 1 write, so the stack is never split.
		if (before < 0.0F || unrolled) {
			extra->SetHealthPerc(Condition::MAX_HEALTH);
			Report(listed, "handed over new", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, handed over new, was {:.4f}",
				name, id, count, Condition::MAX_HEALTH, before);
			return std::nullopt;
		}

		Report(listed, "already set", "{:<30s} [{:08X}] x{:<3d} left alone, already at {:.4f}", name, id, count, before);
		return std::nullopt;
	}

	// -------------------------------------------------------------------
	// A trader's loose item
	// -------------------------------------------------------------------

	// Compares the 2 pointers only. The global names the barter copy while the
	// barter menu is open, and other threads read it too, so it is read fresh
	// on every call and never read through.
	bool BarterCopy(const RE::BGSInventoryList* a_list)
	{
		const auto* barter = RE::TESDataHandler::GetBarterContainer();
		if (!barter || !a_list) {
			return false;
		}
		const auto owner = a_list->owner.get();
		return owner.get() == barter;
	}

	// The barter screen copies such an item with the world item's health and
	// a tag naming it, as 1 stack of its whole count, or 1 copy at a time
	// when the copy still needs its mods or its form asks for 1 at a time.
	// The first copy rolls, and the world item takes the same roll, so every
	// later copy, a rebuild of the screen and the item bought all read it.
	// The trade that buys it puts the mark back, see NoteBought. Lock
	// order: the barter copy's inventory, held by the caller, then the
	// change record, then the world item's extra data.
	bool LooseItem(RE::BGSInventoryList* a_list, RE::TESBoundObject* a_object, RE::BGSInventoryItem::Stack* a_stack)
	{
		if (!a_object || !a_stack || !a_stack->extra || !Gear(*a_object)) {
			return false;
		}
		auto& copy = *a_stack->extra;
		if (!Condition::IsUnrolled(copy.GetHealthPerc()) || Condition::WhyNoCondition(*a_object)) {
			return false;
		}
		const auto* tag = copy.GetByType<RE::ExtraOriginalReference>();
		const auto  world = tag ? tag->originalReference.get() : nullptr;
		auto*       worldExtra = world ? world->extraList.get() : nullptr;
		if (!worldExtra) {
			return false;
		}

		const auto name = NameOf(*a_object, "unnamed item");
		const auto id = a_object->formID;
		const auto count = a_stack->count;
		OpenBlock(nullptr);

		const auto held = worldExtra->GetHealthPerc();
		if (held >= 0.0F && !Condition::IsUnrolled(held)) {
			copy.SetHealthPerc(held);
			Report(true, "a trader's loose item, already rolled", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a trader's loose item, already rolled",
				name, id, count, held);
			return true;
		}
		if (!Condition::IsUnrolled(held)) {
			return false;
		}

		// Recorded first, so the roll written to the world item is saved. A
		// legendary arrives new, as a showpiece, since the copy is a chest.
		world->AddChange(RE::CHANGE_TYPES::kObjectExtraItemData);
		Roll(a_list, a_object, a_stack);
		const auto rolled = copy.GetHealthPerc();
		if (rolled >= 0.0F && !Condition::IsUnrolled(rolled)) {
			worldExtra->SetHealthPerc(rolled);
		}
		Report(true, "a trader's loose item", "{:<30s} [{:08X}] x{:<3d} starts at {:.4f}, a trader's loose item, and so does the one in the world",
			name, id, count, rolled);
		return true;
	}

	// Marks the way Mark in World.cpp does. The world item is deleted by
	// now, and the game still records a change for one that is not created.
	void TradeDone(bool a_mark)
	{
		const auto bought = std::exchange(t_bought, {});
		if (!a_mark || !Settings::bSpawnCondition.GetValue()) {
			return;
		}
		for (const auto& [handle, count] : bought) {
			const auto  world = handle.get();
			auto*       extra = world ? world->extraList.get() : nullptr;
			const auto* base = world ? world->GetObjectReference() : nullptr;
			if (!extra || !base) {
				continue;
			}
			const auto held = extra->GetHealthPerc();
			if (held < 0.0F || Condition::IsUnrolled(held) || !world->AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
				continue;
			}
			extra->SetHealthPerc(Condition::UNROLLED);
			OpenBlock(nullptr);
			Report(true, "bought, marked again in the world", "{:<30s} [{:08X}] x{:<3d} bought, and the one in the world is marked again for its respawn",
				NameOf(*base, "unnamed item"), base->formID, count);
		}
	}

	// -------------------------------------------------------------------
	// The scopes, and the install
	// -------------------------------------------------------------------

	bool UnderSpawnDupe()
	{
		return t_spawnDupe;
	}

	ScopedSpawnContainer::ScopedSpawnContainer() :
		_mark(t_spawnContainer, true)
	{}

	void InstallHandover()
	{
		const auto hooks = CallPatch::PerSite<std::size(HANDOVER_SITES)>([]<std::size_t I>() { return &HandoverHk<I>; });
		CallPatch::PatchAll(HANDOVER_SITES, RE::ID::TESObjectREFR::AddInventoryItem, hooks, g_handoverLinks,
			"Weapons and armor that change hands keep their condition, and loose loot rolls for its first holder");

		const CallPatch::Together spawnDupe{ Part::kConsoleNew };
		const auto                dupeHooks = CallPatch::PerSite<std::size(SPAWN_DUPE_SITE)>([]<std::size_t I>() { return &SpawnDupeHk; });
		CallPatch::PatchAll(SPAWN_DUPE_SITE, RE::ID::TESObjectREFR::AddInventoryItem, dupeHooks, g_spawnDupeLink,
			"SpawnDupe's copies keep the condition of the character they copy");
		g_spawnDupe = spawnDupe.Set();
		if (!g_spawnDupe) {
			REX::WARN("Weapons and armor with no condition that SpawnDupe copies can roll, and the character they copy gets the same roll.");
		}
	}
}
