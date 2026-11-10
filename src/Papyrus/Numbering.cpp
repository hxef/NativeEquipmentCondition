#include "Papyrus/Numbering.h"

#include "Condition/Equipped.h"
#include "Condition/UniqueID.h"
#include "Core/TraceLog.h"
#include "Papyrus/Waiting.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// The numbers of 1 inventory
		// -------------------------------------------------------------------

		// Writes 1 number onto the whole stack, with no split.
		class NumberStack final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			NumberStack(std::uint16_t a_uid, const RE::TESObjectREFR& a_holder) noexcept :
				uid(a_uid),
				holder(a_holder)
			{
				shouldSplitStacks = false;
			}

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					UniqueID::Give(*a_stack.extra, uid, holder);
					given = true;
				}
			}

			std::uint16_t             uid;
			const RE::TESObjectREFR& holder;
			bool                      given{ false };
		};

		// The engine gives a container's numbers from 1 and stops at 65534.
		constexpr std::uint32_t MOST_NUMBERS = 0xFFFE;

		// The handle of a reference, as the engine builds it for a form that
		// is no stack: the form ID with 0xFFFF in the 16 bits above it.
		constexpr std::uint64_t REFERENCE_HANDLE = 0xFFFF00000000;

		// The rounds HandlesFor lets go of the lock in to ask the VM, and the
		// numbers each round asks about past the stacks waiting, for those
		// the VM turns down.
		constexpr std::uint32_t ROUNDS = 4;
		constexpr std::size_t   EXTRA_ASKS = 4;

		// Whether the VM still has a script object for this number in
		// a_holder. A stack that is gone leaves its object bound to the
		// number, so a stack given that number would answer for the old
		// handle. A VM call, so never under an inventory lock.
		bool StillBound(std::uint16_t a_uid, const RE::TESObjectREFR& a_holder)
		{
			const auto* game = RE::GameVM::GetSingleton();
			const auto  vm = game ? game->GetVM() : nullptr;
			if (!vm) {
				return false;
			}
			const auto handle = RE::GameScript::HandlePolicy::GetHandleForInventoryID(a_uid, a_holder.GetFormID());
			RE::BSTSmartPointer<RE::BSScript::Object> object;
			return vm->FindBoundObject(handle, "ObjectReference", false, object, false);
		}

		// What the numbers of 1 inventory are, read under its lock.
		struct Numbers
		{
			// The first stack in list order holding each number.
			std::map<std::uint16_t, const RE::ExtraDataList*> first;

			// The lowest number from a_from that no stack holds and the VM
			// has no object for, or 0 when none is left.
			[[nodiscard]] std::uint16_t Free(std::uint32_t a_from, const std::set<std::uint16_t>& a_bound) const
			{
				for (auto n = a_from; n <= MOST_NUMBERS; ++n) {
					const auto uid = static_cast<std::uint16_t>(n);
					if (!first.contains(uid) && !a_bound.contains(uid)) {
						return uid;
					}
				}
				return 0;
			}
		};

		Numbers NumbersOf(const RE::BGSInventoryList& a_inv)
		{
			Numbers numbers;
			for (const auto& entry : a_inv.data) {
				for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
					if (const auto uid = UniqueID::Of(stack->extra.get()); uid != 0) {
						numbers.first.try_emplace(uid, stack->extra.get());
					}
				}
			}
			return numbers;
		}
	}

	// -------------------------------------------------------------------
	// Numbering
	// -------------------------------------------------------------------

	std::optional<std::vector<Item>> HandlesFor(RE::TESObjectREFR& a_holder, const Pick& a_pick, Hold& a_hold)
	{
		std::set<std::uint16_t>           asked;
		std::set<std::uint16_t>           bound;
		std::set<const RE::ExtraDataList*> gave;  // the stacks this call numbered
		for (std::uint32_t round = 0; round < ROUNDS; ++round) {
			auto* inv = a_holder.inventoryList;
			if (!inv) {
				TraceLog::Papyrus::Line("gone", "{:s} has no inventory loaded", Hex(&a_holder));
				return std::nullopt;
			}
			if (!inv->rwLock.try_lock_write()) {
				Waiting::Locked(a_holder);
				a_hold.busy = true;
				return std::nullopt;
			}

			std::vector<Target> targets;
			a_pick(a_holder.As<RE::Actor>(), *inv, targets);
			auto          numbers = NumbersOf(*inv);
			std::uint32_t from = 1;
			std::size_t   waiting = 0;
			for (auto& target : targets) {
				const auto held = numbers.first.find(target.uid);
				if (target.ref != 0 || (held != numbers.first.end() && held->second == target.extra)) {
					continue;
				}
				const auto uid = numbers.Free(from, bound);
				if (uid == 0) {
					TraceLog::Papyrus::Line("number", "{:s} has all {:d} numbers taken", Hex(&a_holder), MOST_NUMBERS);
					target.uid = 0;
					continue;
				}
				if (!asked.contains(uid)) {
					++waiting;
					target.uid = 0;
					continue;
				}
				Equipped::ListStack find{ target.extra };
				NumberStack         write{ uid, a_holder };
				a_holder.FindAndWriteStackDataForInventoryItem(target.base, find, write);
				TraceLog::Papyrus::Line("number", "base={:s} uid={:d} {:s}{:s} skip={:d}", Hex(target.base), uid,
					write.given ? "given" : "refused", target.uid != 0 ? std::format(" was={:d}", target.uid) : "", bound.size());
				target.uid = write.given ? uid : 0;
				if (write.given) {
					gave.insert(target.extra);
					numbers.first.try_emplace(uid, target.extra);
					from = uid + 1u;
				}
			}

			// The handles are made before the lock goes, so no stack can
			// change in between. Making one reads the same inventory, and
			// a persistent reference's names its container and number.
			std::vector<Item> items;
			if (waiting == 0 || round + 1 == ROUNDS) {
				for (const auto& target : targets) {
					if (target.ref != 0) {
						items.emplace_back(target.ref | REFERENCE_HANDLE);
					} else if (target.uid != 0) {
						items.emplace_back(RE::GameScript::HandlePolicy::GetHandleForInventoryID(target.uid, a_holder.GetFormID()));
					} else {
						continue;
					}
					if (a_hold.inside && !(*a_hold.inside)(items.back()) && gave.contains(target.extra)) {
						UniqueID::Clear(*target.extra);
						TraceLog::Papyrus::Line("number", "base={:s} uid={:d} taken back, its handle was not used", Hex(target.base),
							target.uid);
					}
				}
				inv->rwLock.unlock_write();
				if (waiting != 0) {
					TraceLog::Papyrus::Line("number", "{:d} stacks left without a number", waiting);
				}
				return items;
			}

			// Free numbers no stack holds and the VM was not asked about yet,
			// asked about once the lock goes.
			std::vector<std::uint16_t> ask;
			for (std::uint32_t n = 1; n <= MOST_NUMBERS && ask.size() < waiting + EXTRA_ASKS; ++n) {
				const auto uid = static_cast<std::uint16_t>(n);
				if (!numbers.first.contains(uid) && !asked.contains(uid)) {
					ask.push_back(uid);
				}
			}
			inv->rwLock.unlock_write();
			for (const auto uid : ask) {
				asked.insert(uid);
				if (StillBound(uid, a_holder)) {
					bound.insert(uid);
				}
			}
		}
		return std::nullopt;
	}
}
