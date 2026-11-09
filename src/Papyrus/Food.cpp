#include "Papyrus/Natives.h"

#include "Condition/Equipped.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "Papyrus/Events.h"
#include "Papyrus/Items.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>
#include <variant>

// The food functions. A food's freshness is the stamp FoodSpoil keeps on its
// stack, and a write changes the whole stack, as running out of time does,
// never 1 piece split off. Only food NEC spoils is read or written, and a
// write is free.
namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// Reading
		// -------------------------------------------------------------------

		// Why NEC does not spoil a_found's food, null when it does.
		const char* WhyNotSpoiling(const Found& a_found)
		{
			const auto kind = FoodSpoil::KindNumber(*a_found.base);
			if (kind <= 0) {
				return kind < 0 ? "not food" : "this food never spoils";
			}
			return FoodSpoil::Spoils(*a_found.base) ? nullptr : "food spoiling is off";
		}

		// Whether NEC spoils a_found's food, else a trace line saying why not.
		bool Spoiling(const Found& a_found)
		{
			const auto* why = WhyNotSpoiling(a_found);
			if (why) {
				TraceLog::Papyrus::Line("untracked", "{:s}", why);
			}
			return !why;
		}

		// Whether a_found's stack keeps as in a fridge, by its holder alone,
		// as the sweep reads it. Whether the player shares the stack's list
		// during a take would need the player's lock under the holder's.
		bool Chilled(const Found& a_found)
		{
			return FoodSpoil::IsFridgeRef(a_found.holder.get());
		}

		// What is left of a food's time, a share from 0 to 1, and the days
		// its kind keeps.
		struct Fresh
		{
			float share{ 0.0F };
			float days{ 0.0F };
		};

		std::optional<Fresh> FreshOf(const Item& a_item)
		{
			Holding     held{ a_item };
			const auto* found = held.found();
			if (!found || !Spoiling(*found)) {
				return std::nullopt;
			}
			const auto share = FoodSpoil::StackFreshness(*found->base, found->extra, Chilled(*found));
			const auto days = FoodSpoil::DaysFor(*found->base);
			if (!share || !days) {
				return std::nullopt;
			}
			return Fresh{ *share, *days };
		}

		// -------------------------------------------------------------------
		// Writing
		// -------------------------------------------------------------------

		// Spoils a_extra's food with no share, else gives it a_share of its
		// time. False before the game has a calendar.
		bool Put(RE::ExtraDataList& a_extra, const RE::TESBoundObject& a_object, std::optional<float> a_share, bool a_fridge)
		{
			if (!a_share) {
				FoodSpoil::Spoil(a_extra);
				return true;
			}
			return FoodSpoil::SetFreshness(a_extra, a_object, *a_share, a_fridge);
		}

		// Writes the whole stack the engine hands it.
		class WholeStack final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			WholeStack(std::optional<float> a_share, bool a_fridge) noexcept :
				share(a_share),
				fridge(a_fridge)
			{
				shouldSplitStacks = false;
			}

			void WriteDataImpl(RE::TESBoundObject& a_object, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					ok = Put(*a_stack.extra, a_object, share, fridge);
				}
			}

			std::optional<float> share;
			bool                 fridge;
			bool                 ok{ false };
		};

		// What a write did, kept past the lock.
		struct Done
		{
			bool          ok{ false };  // written, or spoiled already
			bool          written{ false };
			std::uint32_t count{ 0 };

			RE::NiPointer<RE::TESObjectREFR> holder;
			RE::TESBoundObject*              base{ nullptr };
		};

		Done WriteInStack(const Found& a_found, std::optional<float> a_share)
		{
			Done done{ .holder = a_found.holder, .base = a_found.base };
			if (!a_share && FoodSpoil::IsSpoiled(a_found.extra)) {
				done.ok = true;
				TraceLog::Papyrus::Line("write", "none, it is spoiled already");
				return done;
			}
			const auto fridge = Chilled(a_found);
			const auto before = FoodSpoil::StackFreshness(*a_found.base, a_found.extra, fridge).value_or(-1.0F);
			Equipped::ListStack find{ a_found.extra };
			WholeStack          write{ a_share, fridge };
			const auto written = Equipped::WriteStack(*a_found.holder, *a_found.base, find, write);
			if (!written.list || !write.ok) {
				TraceLog::Papyrus::Line("refused", "the game has no calendar yet");
				return done;
			}

			// The stack closest to spoiling goes first, the one a row shows
			// and eats, as after an add.
			auto& inv = *a_found.holder->inventoryList;
			if (a_share) {
				FoodSpoil::Order(inv, *a_found.base);
			}
			done.ok = done.written = true;
			done.count = written.count;
			const auto after = FoodSpoil::StackFreshness(*a_found.base, written.held.get(), fridge).value_or(-1.0F);
			TraceLog::Papyrus::Line("write", "whole stack count={:d} fridge={:s} before={:.4f} after={:.4f}", done.count,
				fridge ? "yes" : "no", before * 100.0F, after * 100.0F);
			if (TraceLog::IsOpen()) {
				TraceLog::Papyrus::Line("rows", "{:s}", RowsOf(inv, *a_found.base));
			}
			return done;
		}

		// The change is recorded first, as a world item is marked, so a write
		// is never kept for this session alone. A pile shares its list, so
		// all of it changes.
		Done WriteInWorld(const Found& a_found, std::optional<float> a_share)
		{
			Done done;
			done.base = a_found.base;
			auto* extra = a_found.extra;
			if (!extra) {
				TraceLog::Papyrus::Line("refused", "the reference has no extra data list");
				return done;
			}
			if (!a_share && FoodSpoil::IsSpoiled(extra)) {
				done.ok = true;
				TraceLog::Papyrus::Line("write", "none, it is spoiled already");
				return done;
			}
			const auto before = FoodSpoil::StackFreshness(*a_found.base, extra, false).value_or(-1.0F);
			if (!a_found.world->AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
				TraceLog::Papyrus::Line("refused", "the game records no change right now");
				return done;
			}
			if (!Put(*extra, *a_found.base, a_share, false)) {
				TraceLog::Papyrus::Line("refused", "the game has no calendar yet");
				return done;
			}
			done.ok = done.written = true;
			done.count = a_found.count;
			const auto after = FoodSpoil::StackFreshness(*a_found.base, extra, false).value_or(-1.0F);
			TraceLog::Papyrus::Line("write", "world count={:d} before={:.4f} after={:.4f}", done.count, before * 100.0F, after * 100.0F);
			return done;
		}

		// After the write, with no lock held. The player's food cards follow
		// through the task, and a spoil raises FoodSpoiled for anybody's food,
		// with no holder for food lying in the world.
		void After(const Done& a_done, bool a_spoiled, std::uint32_t a_game)
		{
			const bool mine = a_done.holder && a_done.holder->IsPlayerRef();
			if (a_spoiled) {
				ScriptEvents::Spoiled(a_done.holder.get(), *a_done.base, a_done.count);
			}
			TraceLog::Papyrus::Line("after", "work={:s} notes={:s}", mine ? "cards" : "none", a_spoiled ? "FoodSpoiled" : "none");
			if (mine) {
				Queue({ .game = a_game,
					.holder = a_done.holder->GetHandle(),
					.base = a_done.base->GetFormID(),
					.cards = true,
					.armor = false,
					.putAway = false });
			} else if (a_spoiled) {
				ScriptEvents::Kick();
			}
		}

		bool Write(const Item& a_item, std::optional<float> a_share)
		{
			const auto game = ScriptEvents::Game();
			Done       done;
			{
				Holding     held{ a_item };
				const auto* found = held.found();
				if (!found) {
					return Answer(false);
				}
				if (!Spoiling(*found)) {
					held.WarnLater(Log::Warning::kUntracked, std::format("{:s}, which NEC does not spoil ({:s})", FormName(*found->base),
						WhyNotSpoiling(*found)));
					return Answer(false);
				}
				done = found->holder ? WriteInStack(*found, a_share) : WriteInWorld(*found, a_share);
			}
			if (done.written) {
				After(done, !a_share, game);
			}
			return Answer(done.ok);
		}
	}

	// -------------------------------------------------------------------
	// The 8 functions
	// -------------------------------------------------------------------

	std::int32_t GetFoodKind(std::monostate, RE::TESForm* a_food)
	{
		Log::Calling("GetFoodKind");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetFoodKind base={:s}", Hex(a_food));
		const auto kind = a_food ? FoodSpoil::KindNumber(*a_food) : -1;
		TraceLog::Papyrus::Line("result", "{:d}", kind);
		return kind;
	}

	float GetSpoilDays(std::monostate, RE::TESForm* a_food)
	{
		Log::Calling("GetSpoilDays");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetSpoilDays base={:s}", Hex(a_food));
		const auto days = a_food ? FoodSpoil::DaysFor(*a_food).value_or(-1.0F) : -1.0F;
		TraceLog::Papyrus::Line("result", "{:.4f}", days);
		return days;
	}

	float GetFreshness(std::monostate, Item a_item)
	{
		Log::Calling("GetFreshness");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetFreshness item={:s}", Shown(a_item));
		const auto fresh = FreshOf(a_item);
		const auto percent = fresh ? fresh->share * 100.0F : -1.0F;
		TraceLog::Papyrus::Line("result", "{:.4f}", percent);
		return percent;
	}

	float GetDaysLeft(std::monostate, Item a_item)
	{
		Log::Calling("GetDaysLeft");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetDaysLeft item={:s}", Shown(a_item));
		const auto fresh = FreshOf(a_item);
		const auto days = fresh ? fresh->share * fresh->days : -1.0F;
		TraceLog::Papyrus::Line("result", "{:.4f}", days);
		return days;
	}

	bool SetFreshness(std::monostate, Item a_item, float a_percent)
	{
		Log::Calling("SetFreshness");
		TraceLog::Papyrus::Begin("CALL", "NEC.SetFreshness item={:s} value={:.4f}", Shown(a_item), a_percent);
		if (!std::isfinite(a_percent) || a_percent <= 0.0F) {
			TraceLog::Papyrus::Line("refused", "the value is not a number, or 0 or below, which Spoil writes");
			Log::Warn(Log::Warning::kRefused, std::format("{:.4f}", a_percent));
			return Answer(false);
		}
		if (a_percent > 100.0F) {
			TraceLog::Papyrus::Line("clamped", "{:.4f} is kept at 100", a_percent);
			Log::Warn(Log::Warning::kClamped, std::format("{:.4f}, so it used 100", a_percent));
		}
		return Write(a_item, std::fmin(a_percent, 100.0F) / 100.0F);
	}

	bool Spoil(std::monostate, Item a_item)
	{
		Log::Calling("Spoil");
		TraceLog::Papyrus::Begin("CALL", "NEC.Spoil item={:s}", Shown(a_item));
		return Write(a_item, std::nullopt);
	}

	bool IsSpoiled(std::monostate, Item a_item)
	{
		Log::Calling("IsSpoiled");
		TraceLog::Papyrus::Begin("CALL", "NEC.IsSpoiled item={:s}", Shown(a_item));
		const auto fresh = FreshOf(a_item);
		return Answer(fresh && fresh->share <= 0.0F);
	}

	bool IsFridge(std::monostate, RE::TESObjectREFR* a_container)
	{
		Log::Calling("IsFridge");
		TraceLog::Papyrus::Begin("CALL", "NEC.IsFridge container={:s}", Hex(a_container));
		return Answer(FoodSpoil::IsFridgeRef(a_container));
	}
}
