#include "Papyrus/Items.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Condition/UniqueID.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Papyrus/Natives.h"
#include "Papyrus/Waiting.h"

#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <utility>
#include <variant>

// The resolver every item function goes through, and the 3 functions that
// read back what an item names.
namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// The 3 ways to name an item
		// -------------------------------------------------------------------

		// The holder of a persistent reference that sits in a container: the
		// container the handle names, or the one the reference's own list
		// says it is in. Null for an item lying in the world.
		RE::NiPointer<RE::TESObjectREFR> HolderOf(const Item& a_item, RE::TESObjectREFR& a_ref)
		{
			if (auto* container = a_item.Container()) {
				return RE::NiPointer<RE::TESObjectREFR>{ container };
			}
			const auto* handles = a_ref.extraList ? a_ref.extraList->GetByType<RE::ExtraReferenceHandles>() : nullptr;
			return handles ? handles->containerRef.get() : nullptr;
		}

		// The stack a_item names in a_inv: the first in list order that holds
		// its number, or the one whose list is the reference's own, since a
		// pickup hands the stack that very list. Under the lock.
		bool FindStack(const RE::BGSInventoryList& a_inv, const RE::ExtraDataList* a_list, Found& a_found)
		{
			for (const auto& entry : a_inv.data) {
				std::uint32_t place = 0;
				for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get(), place++) {
					auto* extra = stack->extra.get();
					if (!extra || (a_list ? extra != a_list : UniqueID::Of(extra) != a_found.uid)) {
						continue;
					}
					if (a_found.matches++ == 0) {
						a_found.base = entry.object;
						a_found.extra = extra;
						a_found.entry = &entry;
						a_found.stack = place;
						a_found.count = stack->count;
						a_found.equipped = stack->IsEquipped();
					}
				}
			}
			return a_found.matches != 0;
		}

		void TraceFound(const Found& a_found)
		{
			if (!TraceLog::IsOpen()) {
				return;
			}
			const auto health = a_found.extra ? Condition::HealthOf(a_found.extra) : Condition::INVALID_HEALTH;
			TraceLog::Papyrus::Line("found", "holder={:s} base={:s} uid={:d} matches={:d} count={:d} health={:s}{:s}{:s}",
				Hex(a_found.holder.get()), Hex(a_found.base), a_found.uid, a_found.matches, a_found.count, ShownHealth(health),
				Condition::IsUnrolled(health) ? " mark=unrolled" : "", a_found.equipped ? " equipped" : "");
		}
	}

	// -------------------------------------------------------------------
	// Holding
	// -------------------------------------------------------------------

	bool Answering()
	{
		const auto why = ScriptEvents::Closed();
		if (why) {
			TraceLog::Papyrus::Line("closed", "{:s}", *why);
			Log::Warn(ScriptEvents::Loading() ? Log::Warning::kLoading : Log::Warning::kFrozen);
		}
		return !why;
	}

	Holding::Holding(const Item& a_item)
	{
		if (!Answering()) {
			return;
		}
		auto* ref = a_item.Reference();
		if (a_item.Container() && a_item.UniqueID() != 0) {
			_found.holder.reset(a_item.Container());
			_found.uid = a_item.UniqueID();
		} else if (ref) {
			_found.holder = HolderOf(a_item, *ref);
		} else {
			TraceLog::Papyrus::Line("none", "no item");
			Log::Warn(Log::Warning::kNone);
			return;
		}

		if (!_found.holder) {
			// An item lying in the world, with its own list and its own lock.
			auto* base = ref->GetObjectReference();
			if (ref->IsDeleted() || !base) {
				TraceLog::Papyrus::Line("gone", "ref {:08X} is deleted or has no base", ref->GetFormID());
				Log::Warn(Log::Warning::kGone, Shown(a_item));
				return;
			}
			_found.world.reset(ref);
			_found.base = base;
			_found.extra = ref->extraList.get();
			_found.count = _found.extra ? static_cast<std::uint32_t>(_found.extra->GetCount()) : 1;
			_found.matches = 1;
			TraceFound(_found);
			_ok = true;
			return;
		}

		auto* inv = _found.holder->inventoryList;
		if (!inv) {
			TraceLog::Papyrus::Line("gone", "{:s} has no inventory loaded", Hex(_found.holder.get()));
			return;
		}
		if (!inv->rwLock.try_lock_write()) {
			Waiting::Locked(*_found.holder);
			return;
		}
		_locked = inv;

		const auto* list = _found.uid == 0 ? ref->extraList.get() : nullptr;
		if (!FindStack(*inv, list, _found)) {
			if (list) {
				TraceLog::Papyrus::Line("gone", "no stack of {:s} holds ref {:08X}", Hex(_found.holder.get()), ref->GetFormID());
			} else {
				TraceLog::Papyrus::Line("gone", "no stack of {:s} holds uid {:d}", Hex(_found.holder.get()), _found.uid);
			}
			WarnLater(Log::Warning::kGone, Shown(a_item));
			return;
		}
		TraceFound(_found);
		_ok = true;
	}

	Holding::~Holding()
	{
		if (_locked) {
			_locked->rwLock.unlock_write();
		}
		if (_warning) {
			Log::Warn(*_warning, _detail);
		}
		// Events pushed under an inventory lock wait for a kick, and a
		// script's call is one, see ScriptEvents.h.
		ScriptEvents::KickIfWaiting();
	}

	bool Holding::Writable()
	{
		const auto* why = Condition::WhyNoCondition(*_found.base);
		if (!why) {
			return true;
		}
		TraceLog::Papyrus::Line("untracked", "{:s}", why);
		WarnLater(Log::Warning::kUntracked, std::format("{:s}, which NEC gives no condition ({:s})", FormName(*_found.base), why));
		return false;
	}

	void Holding::WarnLater(Log::Warning a_warning, std::string a_detail)
	{
		_warning = a_warning;
		_detail = std::move(a_detail);
	}

	// -------------------------------------------------------------------
	// The trace log's words
	// -------------------------------------------------------------------

	std::string Shown(const Item& a_item)
	{
		if (a_item.Container() && a_item.UniqueID() != 0) {
			return std::format("item {:d} in container {:08X}", a_item.UniqueID(), a_item.Container()->GetFormID());
		}
		if (const auto* ref = a_item.Reference()) {
			return std::format("ref {:08X}", ref->GetFormID());
		}
		return "none";
	}

	std::string Hex(const RE::TESForm* a_form)
	{
		return a_form ? std::format("{:08X}", a_form->GetFormID()) : "none";
	}

	std::string FormName(const RE::TESForm& a_form)
	{
		return std::format("{:s} [{:08X}]", RE::TESFullName::GetFullName(a_form), a_form.GetFormID());
	}

	std::string ShownHealth(float a_health)
	{
		return std::format("{:.9g}", a_health);
	}

	std::string RowsOf(const RE::BGSInventoryList& a_inv, const RE::TESBoundObject& a_base)
	{
		std::string   text;
		std::uint32_t row = 0;
		for (const auto& entry : a_inv.data) {
			if (entry.object != &a_base) {
				continue;
			}
			for (auto* stack = entry.stackData.get(); stack; stack = stack->nextStack.get()) {
				std::format_to(std::back_inserter(text), "{:s}[{:d}] uid={:d} count={:d} health={:s}{:s}", text.empty() ? "" : " ", row++,
					UniqueID::Of(stack->extra.get()), stack->count, ShownHealth(Condition::HealthOf(stack)),
					stack->IsEquipped() ? " equipped" : "");
			}
		}
		return text.empty() ? "no stacks" : text;
	}

	float PercentOf(float a_health)
	{
		if (a_health < 0.0F || Condition::IsUnrolled(a_health) || a_health >= Condition::MAX_HEALTH) {
			return 100.0F;
		}
		return a_health * 100.0F;
	}

	bool Tracked(const Found& a_found)
	{
		if (const auto* why = Condition::WhyNoCondition(*a_found.base)) {
			TraceLog::Papyrus::Line("untracked", "{:s}", why);
			return false;
		}
		return true;
	}

	bool Answer(bool a_ok)
	{
		TraceLog::Papyrus::Line("result", "{:s}", a_ok ? "true" : "false");
		return a_ok;
	}

	float HealthAt(float a_percent)
	{
		// Not a number counts as 100, as a new item.
		if (!(a_percent < 100.0F)) {
			return Condition::MAX_HEALTH;
		}
		return a_percent > 0.0F ? a_percent / 100.0F : Condition::MIN_HEALTH;
	}

	bool HeldByNpc(const Found& a_found)
	{
		return a_found.holder && a_found.holder->IsActor() && !a_found.holder->IsPlayerRef();
	}

	float PerUseOf(const RE::TESBoundObject& a_base, const RE::ExtraDataList* a_extra, bool a_theirs)
	{
		if (Condition::KindOf(a_base) == Condition::Kind::kArmor) {
			return ArmorWear::PerHit(static_cast<const RE::TESObjectARMO&>(a_base), a_extra, a_theirs);
		}
		return WeaponWear::PerUse(static_cast<const RE::TESObjectWEAP&>(a_base), a_extra, a_theirs);
	}

	// -------------------------------------------------------------------
	// Reading back
	// -------------------------------------------------------------------

	RE::TESObjectREFR* GetHolder(std::monostate, Item a_item)
	{
		Log::Calling("GetHolder");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetHolder item={:s}", Shown(a_item));
		RE::TESObjectREFR* holder = nullptr;
		{
			Holding held{ a_item };
			if (auto* found = held.found()) {
				holder = found->holder.get();
			}
		}
		TraceLog::Papyrus::Line("result", "{:s}", Hex(holder));
		return holder;
	}

	RE::TESForm* GetBase(std::monostate, Item a_item)
	{
		Log::Calling("GetBase");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetBase item={:s}", Shown(a_item));
		RE::TESForm* base = nullptr;
		{
			Holding held{ a_item };
			if (auto* found = held.found()) {
				base = found->base;
			}
		}
		TraceLog::Papyrus::Line("result", "{:s}", Hex(base));
		return base;
	}

	std::int32_t GetCount(std::monostate, Item a_item)
	{
		Log::Calling("GetCount");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetCount item={:s}", Shown(a_item));
		std::int32_t count = 0;
		{
			Holding held{ a_item };
			if (auto* found = held.found()) {
				count = static_cast<std::int32_t>(found->count);
			}
		}
		TraceLog::Papyrus::Line("result", "{:d}", count);
		return count;
	}
}
