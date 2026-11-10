#include "Papyrus/Items.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"
#include "Papyrus/Events.h"
#include "Papyrus/Natives.h"

#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <variant>

// The 1 writer every condition change of a script ends in. A write is free,
// asks no MCM wear switch, and works only on gear NEC gives a condition, an
// item on a list in NEC/Exclude never. A stack is written as the bench writes
// one, through Equipped::WriteStack.
namespace Papyrus
{
	namespace
	{
		// -------------------------------------------------------------------
		// The health a write leaves
		// -------------------------------------------------------------------

		enum class Mode
		{
			kSet,     // to a percent
			kDamage,  // down by percent points
			kRepair,  // up by percent points
			kWear,    // down by what that many shots, swings or hits cost
		};

		struct Ask
		{
			Mode  mode;
			float value;
		};

		// A health as a write starts from. No health and the mark of loot that
		// has not rolled yet both read as full.
		float StartOf(float a_health)
		{
			return a_health < 0.0F || a_health >= Condition::MAX_HEALTH ? Condition::MAX_HEALTH : a_health;
		}

		// The health a_ask leaves on a copy at a_health, kept between 0 and
		// MAX_HEALTH, so a write to 100 never removes the health.
		float TargetOf(const Ask& a_ask, float a_health, const RE::TESBoundObject& a_base, const RE::ExtraDataList* a_extra,
			bool a_theirs, float a_crew)
		{
			auto target = a_health;
			switch (a_ask.mode) {
			case Mode::kSet:
				target = a_ask.value / 100.0F;
				break;
			case Mode::kDamage:
				target = a_health - a_ask.value / 100.0F;
				break;
			case Mode::kRepair:
				target = a_health + a_ask.value / 100.0F;
				break;
			case Mode::kWear:
				target = a_health - PerUseOf(a_base, a_extra, a_theirs, a_crew) * a_ask.value;
				break;
			}
			if (target <= Condition::MIN_HEALTH) {
				return Condition::MIN_HEALTH;
			}
			return target >= Condition::MAX_HEALTH ? Condition::MAX_HEALTH : target;
		}

		// -------------------------------------------------------------------
		// The write
		// -------------------------------------------------------------------

		// Picks the stack whose list is a_extra and works out its new health,
		// picking none when the health would not move. Under the lock.
		class NewHealth final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			NewHealth(const Ask& a_ask, const RE::TESBoundObject& a_base, const RE::ExtraDataList* a_extra, bool a_theirs, float a_crew) noexcept :
				ask(a_ask),
				base(a_base),
				extra(a_extra),
				theirs(a_theirs),
				crew(a_crew)
			{}

			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra.get() != extra) {
					return false;
				}
				const auto health = StartOf(Condition::HealthOf(&a_stack));
				target = TargetOf(ask, health, base, extra, theirs, crew);
				return target != health;
			}

			const Ask&                ask;
			const RE::TESBoundObject& base;
			const RE::ExtraDataList*  extra;
			bool                      theirs;
			float                     crew;
			float                     target{ Condition::MAX_HEALTH };
		};

		// Writes the health NewHealth worked out on the 1 copy the engine
		// splits off.
		class Put final :
			public RE::BGSInventoryItem::StackDataWriteFunctor
		{
		public:
			explicit Put(const NewHealth& a_pick) noexcept :
				pick(a_pick)
			{}

			void WriteDataImpl(RE::TESBoundObject&, RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (a_stack.extra) {
					a_stack.extra->SetHealthPerc(pick.target);
				}
			}

			const NewHealth& pick;
		};

		// What a write did, kept past the lock.
		struct Done
		{
			bool  ok{ false };  // written, or at that condition already
			bool  written{ false };
			float before{ -1.0F };  // percents
			float after{ -1.0F };
			float healthBefore{ Condition::INVALID_HEALTH };
			float healthAfter{ Condition::INVALID_HEALTH };

			RE::NiPointer<RE::TESObjectREFR>       holder;
			RE::ObjectRefHandle                    world{};  // the item lying in the world
			RE::BSTSmartPointer<RE::ExtraDataList> list{};   // the written stack's list, held for its event
			RE::TESBoundObject*                    base{ nullptr };
			bool                                   equipped{ false };
		};

		Done WriteInStack(const Found& a_found, const Ask& a_ask)
		{
			auto&      holder = *a_found.holder;
			NewHealth  pick{ a_ask, *a_found.base, a_found.extra, HeldByNpc(a_found), a_found.crew };
			Put        put{ pick };
			const auto written = Equipped::WriteStack(holder, *a_found.base, pick, put);

			Done done{ .ok = true, .holder = a_found.holder, .base = a_found.base };
			if (!written.list) {
				done.before = done.after = PercentOf(Condition::HealthOf(a_found.extra));
				TraceLog::Papyrus::Line("write", "none, it is at {:.4f} already", done.after);
				return done;
			}
			done.written = true;
			done.before = PercentOf(written.before);
			done.after = PercentOf(written.after);
			done.healthBefore = written.before;
			done.healthAfter = written.after;
			done.list = written.held;
			done.equipped = written.equipped;
			TraceLog::Papyrus::Line("write", "split={:s} written=1 remainder={:d} remainder_uid={:s} before={:s} after={:s}",
				written.split ? "yes" : "no", written.split ? written.count - 1 : 0, written.cleared ? "cleared" : "none",
				ShownHealth(written.before), ShownHealth(written.after));
			if (TraceLog::IsOpen() && holder.inventoryList) {
				TraceLog::Papyrus::Line("rows", "{:s}", RowsOf(*holder.inventoryList, *a_found.base));
			}
			return done;
		}

		// The change is recorded first and the health written only once the
		// game took it, as a world item is marked, so a write is never kept
		// for this session alone. A pile shares its list, so all of it changes.
		Done WriteInWorld(const Found& a_found, const Ask& a_ask)
		{
			Done done;
			done.base = a_found.base;
			auto* extra = a_found.extra;
			if (!extra) {
				TraceLog::Papyrus::Line("refused", "the reference has no extra data list");
				return done;
			}
			const auto had = Condition::HealthOf(extra);
			const auto health = StartOf(had);
			const auto target = TargetOf(a_ask, health, *a_found.base, extra, false, 1.0F);
			done.before = PercentOf(had);
			if (target == health) {
				done.ok = true;
				done.after = done.before;
				TraceLog::Papyrus::Line("write", "none, it is at {:.4f} already", done.after);
				return done;
			}
			if (!a_found.world->AddChange(RE::CHANGE_TYPES::kObjectExtraItemData)) {
				TraceLog::Papyrus::Line("refused", "the game records no change right now");
				return done;
			}
			extra->SetHealthPerc(target);
			done.ok = done.written = true;
			done.after = PercentOf(target);
			done.healthBefore = had;
			done.healthAfter = target;
			done.world = a_found.world->GetHandle();
			TraceLog::Papyrus::Line("write", "world before={:s} after={:s}", ShownHealth(health), ShownHealth(target));
			return done;
		}

		// -------------------------------------------------------------------
		// After the write, with no lock held
		// -------------------------------------------------------------------

		// The events of the change, which the task hands to scripts. The
		// player's items only for ConditionChanged, anybody's for Broke.
		std::string Note(const Done& a_done)
		{
			std::string pushed;
			if (a_done.holder) {
				ScriptEvents::Changed(*a_done.holder, *a_done.base, a_done.list.get(), a_done.healthBefore, a_done.healthAfter,
					ScriptEvents::Cause::kScript);
				if (a_done.holder.get() == RE::PlayerCharacter::GetSingleton()) {
					pushed = "ConditionChanged";
				}
			}
			if (Condition::Breaks(a_done.healthBefore, a_done.healthAfter)) {
				ScriptEvents::Broke(a_done.holder.get(), *a_done.base, a_done.list.get(), a_done.world);
				pushed += pushed.empty() ? "Broke" : ",Broke";
			}
			return pushed.empty() ? "none" : pushed;
		}

		void After(const Done& a_done, std::uint32_t a_game)
		{
			auto*      actor = a_done.holder ? a_done.holder->As<RE::Actor>() : nullptr;
			const bool mine = actor && actor->IsPlayerRef();
			const Work work{
				.game = a_game,
				.holder = actor ? actor->GetHandle() : RE::ObjectRefHandle{},
				.base = a_done.base->GetFormID(),
				.cards = mine,
				.armor = actor && a_done.equipped && a_done.base->Is(RE::ENUM_FORM_ID::kARMO),
				.putAway = mine && a_done.equipped && a_done.base->IsWeapon() && a_done.before > 0.0F && a_done.after <= 0.0F,
			};
			const auto notes = Note(a_done);
			std::string what;
			const auto  add = [&what](bool a_on, std::string_view a_name) {
				if (a_on) {
					what += what.empty() ? "" : ",";
					what += a_name;
				}
			};
			add(work.cards, "cards");
			add(work.armor, "armor");
			add(work.putAway, "putaway");
			TraceLog::Papyrus::Line("after", "work={:s} notes={:s}", what.empty() ? "none" : what, notes);

			// The task also hands on the events, so it runs for a write with
			// no work too.
			if (what.empty()) {
				ScriptEvents::Kick();
			} else {
				Queue(work);
			}
		}

		bool Write(std::string_view a_name, const Item& a_item, const Ask& a_ask)
		{
			Log::Calling(a_name);
			TraceLog::Papyrus::Begin("CALL", "NEC.{:s} item={:s} value={:.4f}", a_name, Shown(a_item), a_ask.value);
			const auto game = ScriptEvents::Game();
			if (!std::isfinite(a_ask.value) || (a_ask.mode != Mode::kSet && a_ask.value < 0.0F)) {
				TraceLog::Papyrus::Line("refused", "the value is not a number, or below 0");
				Log::Warn(Log::Warning::kRefused, std::format("{:.4f}", a_ask.value));
				return Answer(false);
			}
			if (a_ask.mode == Mode::kSet && (a_ask.value < 0.0F || a_ask.value > 100.0F)) {
				TraceLog::Papyrus::Line("clamped", "{:.4f} is kept between 0 and 100", a_ask.value);
				Log::Warn(Log::Warning::kClamped, std::format("{:.4f}, so it used {:d}", a_ask.value, a_ask.value < 0.0F ? 0 : 100));
			}

			Done done;
			{
				Holding held{ a_item };
				const auto* found = held.found();
				if (!found || !held.Writable()) {
					return Answer(false);
				}
				done = found->holder ? WriteInStack(*found, a_ask) : WriteInWorld(*found, a_ask);
			}
			if (done.written) {
				After(done, game);
			}
			return Answer(done.ok);
		}
	}

	// -------------------------------------------------------------------
	// The 5 functions
	// -------------------------------------------------------------------

	bool SetCondition(std::monostate, Item a_item, float a_percent)
	{
		return Write("SetCondition", a_item, { Mode::kSet, a_percent });
	}

	bool Damage(std::monostate, Item a_item, float a_percent)
	{
		return Write("Damage", a_item, { Mode::kDamage, a_percent });
	}

	bool Repair(std::monostate, Item a_item, float a_percent)
	{
		return Write("Repair", a_item, { Mode::kRepair, a_percent });
	}

	bool Wear(std::monostate, Item a_item, float a_uses)
	{
		return Write("Wear", a_item, { Mode::kWear, a_uses });
	}

	bool RollCondition(std::monostate, Item a_item)
	{
		Log::Calling("RollCondition");
		TraceLog::Papyrus::Begin("CALL", "NEC.RollCondition item={:s}", Shown(a_item));
		RE::NiPointer<RE::TESObjectREFR> world;
		RE::TESBoundObject*              base = nullptr;
		{
			Holding     held{ a_item };
			const auto* found = held.found();
			if (!found) {
				return Answer(false);
			}
			if (found->holder) {
				TraceLog::Papyrus::Line("refused", "only an item lying in the world rolls");
				return Answer(false);
			}
			if (!held.Writable()) {
				return Answer(false);
			}
			if (!Condition::IsUnrolled(Condition::HealthOf(found->extra))) {
				TraceLog::Papyrus::Line("refused", "it rolled already");
				return Answer(false);
			}
			world = found->world;
			base = found->base;
		}

		// The script's call holds no lock, and the roll writes only the world
		// reference's own list, so it may run here as on F4SE's queue.
		const bool rolled = SpawnCondition::RollOnLook(*world);
		if (rolled) {
			Done done;
			done.healthBefore = Condition::UNROLLED;
			done.healthAfter = Condition::HealthOf(world->extraList.get());
			done.world = world->GetHandle();
			done.base = base;
			TraceLog::Papyrus::Line("after", "work=none notes={:s}", Note(done));
			ScriptEvents::Kick();
		} else {
			TraceLog::Papyrus::Line("refused", "Worn loot is off, it waits for its taker, or the game records no change right now");
		}
		return Answer(rolled);
	}
}
