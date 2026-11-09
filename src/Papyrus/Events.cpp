#include "Papyrus/Events.h"

#include "Core/ScriptEvents.h"
#include "Core/TraceLog.h"
#include "Papyrus/Items.h"
#include "Papyrus/Log.h"
#include "Papyrus/Pack.h"
#include "Papyrus/Papyrus.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The 1 task that runs what scripts' writes left and hands every waiting
// event to the scripts registered for it through F4SE. It runs where F4SE
// runs its queue: a worker thread or the frame's own thread during play, the
// main thread in a menu. The VM queues each callback for its next update, so
// no script runs inside NEC, and a script that writes from its callback only
// pushes another event.
namespace Papyrus
{
	namespace
	{
		using ScriptEvents::Kind;
		using ScriptEvents::Note;

		// A busy inventory's events wait until the game lets go of it, or go
		// with akItem None once 2 s and 100 tries have passed, as a busy NPC
		// wear waits for both, see WeaponWear's Theirs.cpp.
		constexpr std::uint32_t                  FEWEST_TRIES = 100;
		constexpr std::chrono::seconds           MOST_WAIT{ 2 };
		constexpr std::array<std::string_view, 4> NAMES{ "NEC:ConditionChanged", "NEC:Broke", "NEC:Jammed", "NEC:FoodSpoiled" };

		// The events dropped for a full queue that NEC.papyrus.log has
		// counted, see ScriptEvents::Overflowed.
		std::atomic<std::uint32_t> g_dropped{ 0 };

		enum class From
		{
			kTask,
			kSave,
		};

		// -------------------------------------------------------------------
		// Who hears an event
		// -------------------------------------------------------------------

		struct Registration
		{
			std::uint64_t handle{ 0 };
			std::string   script;
			std::string   callback;
		};

		// F4SE calls this once per registration with its own lock held, so it
		// copies and does nothing else.
		void F4SEAPI Collect(std::uint64_t a_handle, const char* a_script, const char* a_callback, void* a_data)
		{
			if (a_script && a_callback) {
				static_cast<std::vector<Registration>*>(a_data)->push_back({ a_handle, a_script, a_callback });
			}
		}

		// The scripts registered for each kind, asked once a drain, the first
		// time a note of that kind comes up.
		class Listeners
		{
		public:
			const std::vector<Registration>& Of(Kind a_kind)
			{
				auto& got = _of[static_cast<std::size_t>(a_kind)];
				if (!got) {
					got.emplace();
					if (const auto* papyrus = F4SE::GetPapyrusInterface()) {
						papyrus->GetExternalEventRegistrations(NAMES[static_cast<std::size_t>(a_kind)], &*got, Collect);
					}
				}
				return *got;
			}

		private:
			std::array<std::optional<std::vector<Registration>>, 4> _of;
		};

		// -------------------------------------------------------------------
		// Packing an event
		// -------------------------------------------------------------------

		// An event's arguments, packed.
		struct Raise
		{
			std::array<RE::BSScript::Variable, 6> args;
			std::uint32_t                         size{ 0 };
			std::string                           item{ "none" };  // for the trace log
		};

		RE::NiPointer<RE::TESObjectREFR> HolderOf(const Note& a_note)
		{
			auto* form = a_note.holder ? RE::TESForm::GetFormByID(a_note.holder) : nullptr;
			return RE::NiPointer<RE::TESObjectREFR>{ form ? form->As<RE::TESObjectREFR>() : nullptr };
		}

		// Packs every argument but the item, then the item a_item.
		bool Fill(const Note& a_note, RE::TESObjectREFR* a_holder, const RE::TESForm* a_base, const Item& a_item,
			const RE::BSScript::Variable* a_world, Raise& a_raise)
		{
			auto& args = a_raise.args;
			args[0] = a_note.kind == Kind::kJammed ? PackAs(a_holder, RE::BSScript::kActor) : PackAs(a_holder, RE::BSScript::kObjectReference);
			args[1] = PackAs(a_base, RE::BSScript::kForm);
			if (a_note.kind == Kind::kFoodSpoiled) {
				args[2] = static_cast<std::int32_t>(a_note.count);
				a_raise.size = 3;
				return true;
			}
			args[2] = a_world ? *a_world : PackItem(a_item);
			a_raise.size = 3;
			if (!a_world) {
				a_raise.item = Shown(a_item);
			}
			if (a_note.kind == Kind::kConditionChanged) {
				args[3] = PercentOf(a_note.before);
				args[4] = PercentOf(a_note.after);
				args[5] = static_cast<std::int32_t>(a_note.cause);
				a_raise.size = 6;
			}
			return !args[2].is<std::nullptr_t>();
		}

		// The note's arguments. A stack is found again by its list under its
		// holder's lock, tried once, and its handle is packed before the lock
		// goes: packing reads the same inventory, which inside its own write
		// lock never waits. Nothing while the lock is busy, unless a_late,
		// which gives the item as None.
		std::optional<Raise> Pack(const Note& a_note, bool a_late)
		{
			Raise      raise;
			const auto holder = HolderOf(a_note);
			auto*      base = RE::TESForm::GetFormByID(a_note.base);
			if (a_note.kind == Kind::kFoodSpoiled) {
				Fill(a_note, holder.get(), base, Item{}, nullptr, raise);
				return raise;
			}
			if (!a_note.holder) {
				const auto world = a_note.world.get();
				const auto item = PackAs(world.get(), RE::BSScript::kObjectReference);
				Fill(a_note, nullptr, base, Item{}, &item, raise);
				raise.item = world ? std::format("ref {:08X}", world->GetFormID()) : "none";
				return raise;
			}
			auto* bound = base && base->IsBoundObject() ? static_cast<RE::TESBoundObject*>(base) : nullptr;
			if (a_late || !holder || !bound || !holder->inventoryList) {
				Fill(a_note, holder.get(), base, Item{}, nullptr, raise);
				return raise;
			}
			const std::function<bool(const Item&)> inside = [&](const Item& a_item) {
				return Fill(a_note, holder.get(), base, a_item, nullptr, raise);
			};
			if (!NameStack(*holder, a_note.list.get(), *bound, inside)) {
				return std::nullopt;
			}
			return raise;
		}

		// The EVENT line's event, holder and base, all a note shows when it
		// is dropped or kept waiting.
		std::string Head(const Note& a_note)
		{
			return std::format("{:s} holder={:08X} base={:08X}", NAMES[static_cast<std::size_t>(a_note.kind)], a_note.holder, a_note.base);
		}

		std::string Header(const Note& a_note, const Raise& a_raise)
		{
			auto text = Head(a_note);
			if (a_note.kind == Kind::kFoodSpoiled) {
				std::format_to(std::back_inserter(text), " count={:d}", a_note.count);
				return text;
			}
			std::format_to(std::back_inserter(text), " item={:s}", a_raise.item);
			if (a_note.kind == Kind::kConditionChanged) {
				std::format_to(std::back_inserter(text), " before={:.4f} after={:.4f} cause={:d}", PercentOf(a_note.before),
					PercentOf(a_note.after), static_cast<std::int32_t>(a_note.cause));
			}
			return text;
		}

		// Queues the callback of every registration. The VM calls each at its
		// next update, with a copy of the arguments.
		void Dispatch(const Note& a_note, const Raise& a_raise, bool a_late, const std::vector<Registration>& a_to)
		{
			const auto event = NAMES[static_cast<std::size_t>(a_note.kind)];
			TraceLog::Papyrus::Begin("EVENT", "{:s}", Header(a_note, a_raise));
			if (a_late) {
				TraceLog::Papyrus::Line("busy", "{:08X} stayed busy for {:d} tries, so the item goes as None", a_note.holder, a_note.tries);
				Log::WarnFor(event, Log::Warning::kEventBusy, std::format("{:08X}", a_note.holder));
			} else if (a_raise.item == "none" && a_note.kind != Kind::kFoodSpoiled) {
				TraceLog::Papyrus::Line("gone", "item=none");
			}
			const auto* game = RE::GameVM::GetSingleton();
			const auto  vm = game ? game->GetVM() : nullptr;
			if (!vm) {
				TraceLog::Papyrus::Line("drop", "no script VM");
				return;
			}
			std::string reached;
			for (const auto& to : a_to) {
				// The registration names the script by its object's own type,
				// which the handle has bound.
				RE::BSTSmartPointer<RE::BSScript::Object> object;
				bool                                      queued = false;
				if (vm->FindBoundObject(to.handle, to.script.c_str(), false, object, false) && object) {
					const RE::BSScript::DispatchArguments arguments{ [&](RE::BSScrapArray<RE::BSScript::Variable>& a_args) {
						a_args = RE::BSScrapArray<RE::BSScript::Variable>{ a_raise.size };
						for (std::uint32_t i = 0; i < a_raise.size; ++i) {
							a_args[i] = a_raise.args[i];
						}
						return true;
					} };
					queued = vm->DispatchMethodCall(object, to.callback.c_str(), arguments.get(),
						RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{});
				}
				TraceLog::Papyrus::Line("reach", "{:s}.{:s} handle=0x{:016X} queued={:s}{:s}", to.script, to.callback, to.handle,
					queued ? "true" : "false", object ? "" : ", the script is not bound to its handle");
				if (queued) {
					std::format_to(std::back_inserter(reached), "{:s}{:s}.{:s}", reached.empty() ? "" : ", ", to.script, to.callback);
				}
			}
			if (!reached.empty()) {
				Log::Reached(event, reached);
			}
		}

		// The events a full queue dropped since the last drain, counted in
		// NEC.papyrus.log. 2 drains at once each count only what the other
		// has not.
		void CountDropped()
		{
			const auto dropped = ScriptEvents::Overflowed();
			auto       seen = g_dropped.load();
			while (seen < dropped && !g_dropped.compare_exchange_weak(seen, dropped)) {}
			if (seen < dropped) {
				Log::WarnFor({}, Log::Warning::kDropped, {}, dropped - seen);
			}
		}

		// -------------------------------------------------------------------
		// The drain
		// -------------------------------------------------------------------

		void Drain(From a_from, std::size_t a_works)
		{
			CountDropped();
			auto notes = ScriptEvents::Take();
			if (notes.empty() && a_works == 0) {
				return;
			}
			TraceLog::Papyrus::Begin("DRAIN", "notes={:d} works={:d} from={:s}", notes.size(), a_works,
				a_from == From::kSave ? "presave" : "task");

			const auto          game = ScriptEvents::Game();
			const auto          now = ScriptEvents::Clock::now();
			Listeners           listeners;
			std::vector<Note>   back;
			std::set<RE::TESFormID> busy;
			for (auto& note : notes) {
				// A load or a new game began while this ran: the rest belong
				// to the game before.
				if (note.game != game || ScriptEvents::Game() != game) {
					TraceLog::Papyrus::Begin("EVENT", "{:s}", Head(note));
					TraceLog::Papyrus::Line("old", "from the last game");
					continue;
				}
				// A change too small to show in a percent is no change.
				if (note.kind == Kind::kConditionChanged && PercentOf(note.before) == PercentOf(note.after)) {
					continue;
				}
				const auto& to = listeners.Of(note.kind);
				if (to.empty()) {
					TraceLog::Papyrus::Begin("EVENT", "{:s}", Head(note));
					TraceLog::Papyrus::Line("drop", "no registration");
					continue;
				}
				// Events of 1 holder keep their order, so once its inventory
				// is busy the rest of its events wait too.
				std::optional<Raise> raise;
				const bool           late = note.tries >= FEWEST_TRIES && now >= note.dropAt;
				if (!busy.contains(note.holder) || note.holder == 0) {
					raise = Pack(note, late);
				}
				if (!raise) {
					if (note.tries++ == 0) {
						note.dropAt = now + MOST_WAIT;
					}
					busy.insert(note.holder);
					TraceLog::Papyrus::Begin("EVENT", "{:s}", Head(note));
					TraceLog::Papyrus::Line("busy", "tries={:d}", note.tries);
					back.push_back(std::move(note));
					continue;
				}
				Dispatch(note, *raise, late, to);
			}
			if (!back.empty()) {
				ScriptEvents::PutBack(std::move(back));
			}
			// The notes and their lists go here, with no lock held. A list's
			// last hold can free it on this thread.
		}

		void RunWorks(From a_from)
		{
			// Inside a load or a quit's stop of the scripts, everything waits
			// for the counter to drop it or the game to run again. A save
			// stops the scripts too, and its run hands the events on, which
			// the scripts hear once they run again.
			if (a_from == From::kTask && ScriptEvents::Closed()) {
				PutWorksBack(TakeWorks());
				return;
			}
			const auto works = TakeWorks();
			for (const auto& work : works) {
				RunWork(work);
			}
			Drain(a_from, works.size());
		}
	}

	void Unload()
	{
		ScriptEvents::Unload();
		TakeWorks();
		Log::EndGame();
	}

	void EndGame()
	{
		Log::EndGame();
	}

	void BeforeSave()
	{
		RunWorks(From::kSave);
	}

	void RunTask()
	{
		RunWorks(From::kTask);
	}
}
