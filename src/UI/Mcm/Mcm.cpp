#include "UI/Mcm/Mcm.h"

#include "Core/Presets/Presets.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Mcm/Bridge.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Mcm
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Clock = std::chrono::steady_clock;

		// How long a change stands before it is written and logged, so a
		// dragged slider is 1 write and 1 change line, not 1 for each step it
		// passes.
		constexpr auto WAIT = std::chrono::milliseconds{ 500 };

		// -------------------------------------------------------------------
		// Changes their file does not have yet
		// -------------------------------------------------------------------

		struct Change
		{
			std::string           section;
			std::string           key;
			std::string           before;
			std::string           now;
			std::string           fallback;  // where the line goes, see Presets::Base
			std::filesystem::path file;      // NEC_custom.ini or the picked preset's, see Presets::Target
		};

		// Noted by the page and taken by the frame listener.
		std::mutex          g_owedLock;
		std::vector<Change> g_owed;

		// When the last change still owed was made, 0 when none is.
		std::atomic<Clock::rep> g_changedAt{ 0 };

		// Held while owed changes are written, so a pick or a box's answer
		// waits for a write the frame listener began and never lands before
		// it.
		std::mutex g_writeLock;

		// Set when the page has to be drawn again, and spent by the frame
		// listener.
		std::atomic<bool> g_refresh{ false };

		bool Due()
		{
			const auto at = g_changedAt.load();
			return at != 0 && Clock::now() - Clock::time_point{ Clock::duration{ at } } >= WAIT;
		}

		// Writes every change owed into its file and logs a line for each,
		// then the Settings line, and the summary when a change made it read
		// differently. Returns how many it wrote.
		std::size_t Write()
		{
			const std::scoped_lock w{ g_writeLock };
			std::vector<Change>    owed;
			{
				const std::scoped_lock l{ g_owedLock };
				owed.swap(g_owed);
				g_changedAt.store(0);
			}

			std::size_t written = 0;
			for (const auto& [section, key, before, now, fallback, file] : owed) {
				// Changed and changed back.
				if (before == now) {
					continue;
				}
				written++;
				Settings::LogChange(key, before, now, "from the MCM page");
				// Back at the fallback, the key's line goes, so each file holds
				// only what the player changed. A file that is not there yet
				// starts from Presets::NewText.
				const auto saved = now == fallback ? Settings::Drop(section, key, file, Presets::NewText(file, true), Presets::Stands(file)) :
				                                     Settings::Keep(section, key, now, file, Presets::NewText(file));
				if (!saved) {
					REX::WARN("{:s} stays {:s} only until the game restarts.", key, now);
				}
				// A first change or the last one undone adds or takes away
				// the picked preset's (Custom), so the page draws again.
				if (Presets::Wrote(file)) {
					g_refresh.store(true);
				}
			}
			if (written > 0) {
				Settings::ReportLine(false);
			}
			return written;
		}

		// -------------------------------------------------------------------
		// NEC's functions on root.mcm
		// -------------------------------------------------------------------

		// The start of every line of text config.json asks NEC for.
		constexpr std::string_view LINE = "NEC|"sv;

		// The 10 members of root.mcm NEC answers for, in the order of MEMBERS.
		enum class Member : std::uint32_t
		{
			kGetModSettingBool,
			kGetModSettingInt,
			kGetModSettingFloat,
			kSetModSettingBool,
			kSetModSettingInt,
			kSetModSettingFloat,
			kGetFullName,
			kGetModSettingString,
			kSetModSettingString,
			kGetListFromForm,
			kTotal,
		};

		struct Wrapped
		{
			const char* name;
			bool        line;  // asked as "NEC|<id>", not as "NEC" and the id
			void (*answer)(const Params& a_params, std::string_view a_id);
		};

		constexpr Wrapped MEMBERS[]{
			{ "GetModSettingBool", false, GetBool },
			{ "GetModSettingInt", false, GetInt },
			{ "GetModSettingFloat", false, GetFloat },
			{ "SetModSettingBool", false, SetBool },
			{ "SetModSettingInt", false, SetInt },
			{ "SetModSettingFloat", false, SetFloat },
			{ "GetFullName", true, GetFullName },
			{ "GetModSettingString", false, GetString },
			{ "SetModSettingString", false, SetString },
			{ "GetListFromForm", true, GetList },
		};
		static_assert(std::size(MEMBERS) == std::to_underlying(Member::kTotal));

		// Where Wrap keeps MCM's own function.
		std::string Moved(const char* a_name)
		{
			return std::format("NEC_{:s}", a_name);
		}

		// The id of a call for NEC, or nothing for a call for another mod.
		std::optional<std::string_view> IdOf(const Params& a_params, bool a_line)
		{
			const auto text = [&a_params](std::uint32_t a_index) -> std::optional<std::string_view> {
				if (a_index >= a_params.argCount || !a_params.args[a_index].IsString() || !a_params.args[a_index].GetString()) {
					return std::nullopt;
				}
				return std::string_view{ a_params.args[a_index].GetString() };
			};
			if (a_line) {
				const auto id = text(0);
				return id && id->starts_with(LINE) ? std::optional{ id->substr(LINE.size()) } : std::nullopt;
			}
			const auto mod = text(0);
			return mod && *mod == MOD ? text(1) : std::nullopt;
		}

		// Hands a call for another mod to MCM's own function. The call comes
		// with root.mcm as self, which is where Wrap kept it.
		void Forward(const Params& a_params, const char* a_name)
		{
			const auto original = Moved(a_name);
			Value      mcm;
			if (a_params.self && a_params.self->IsObject() && a_params.self->HasMember(original)) {
				mcm = *a_params.self;
			} else if (!a_params.movie || !a_params.movie->GetVariable(&mcm, "_root.mcm") || !mcm.IsObject()) {
				return;
			}
			mcm.Invoke(original.c_str(), a_params.retVal, a_params.args, a_params.argCount);
		}

		// A value the page sent or got back, for the trace.
		std::string Shown(const Value& a_value)
		{
			if (a_value.IsBoolean()) {
				return a_value.GetBoolean() ? "true" : "false";
			}
			if (a_value.IsString()) {
				return std::format("\"{:s}\"", a_value.GetString() ? a_value.GetString() : "");
			}
			if (Flash::IsAnyNumber(a_value)) {
				return std::format("{}", Flash::AsNumber(a_value));
			}
			if (a_value.IsArray()) {
				std::string list;
				for (std::uint32_t i = 0; i < a_value.GetArraySize(); i++) {
					Value element;
					a_value.GetElement(i, &element);
					list += (i == 0 ? "" : ", ") + Shown(element);
				}
				return std::format("[{:s}]", list);
			}
			return "nothing";
		}

		// 1 line for each call NEC answers, so the trace shows what the page
		// asked for and got. A change brings its new value as a third argument.
		void Trace(const Params& a_params, const char* a_name, std::string_view a_id)
		{
			const auto sent = a_params.argCount > 2 ? std::format(" to {:s}", Shown(a_params.args[2])) : std::string{};
			const auto answer = a_params.retVal ? Shown(*a_params.retVal) : std::string{ "nothing" };
			TraceLog::Line("menu", "MCM {:s} {:s}{:s}, NEC answers {:s}", a_name, a_id, sent, answer);
		}

		// Set by every call that reaches NEC's GetModSettingBool, see Ours.
		std::atomic<bool> g_heard{ false };

		// One handler for all 10. CreateFunction gives each its place in
		// MEMBERS as userData.
		class Bridge final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const auto place = reinterpret_cast<std::uintptr_t>(a_params.userData);
				if (place >= std::size(MEMBERS)) {
					return;
				}
				if (place == std::to_underlying(Member::kGetModSettingBool)) {
					g_heard.store(true);
				}
				const auto& member = MEMBERS[place];
				if (const auto id = IdOf(a_params, member.line)) {
					member.answer(a_params, *id);
					if (TraceLog::IsOpen()) {
						Trace(a_params, member.name, *id);
					}
				} else {
					Forward(a_params, member.name);
				}
			}
		};

		Bridge g_bridge;

		// Said once a session, since Wrap tries again every frame until MCM's
		// GetModSettingBool is wrapped.
		std::atomic<bool> g_warned{ false };

		// Moves MCM's 10 functions on a_mcm to NEC_<name> and puts NEC's in
		// their place. False when a_mcm already has NEC's.
		bool Wrap(Scaleform::GFx::Movie& a_movie, Value& a_mcm)
		{
			if (a_mcm.HasMember(Moved(MEMBERS[std::to_underlying(Member::kGetModSettingBool)].name))) {
				return false;
			}

			for (std::uint32_t i = 0; i < std::size(MEMBERS); i++) {
				const auto* name = MEMBERS[i].name;
				const auto  moved = Moved(name);
				// A member is never moved twice, or NEC's function would be
				// kept as MCM's and hand every other mod's call to itself.
				Value original;
				if (a_mcm.HasMember(moved) || !a_mcm.GetMember(name, &original) || original.IsUndefined()) {
					continue;
				}
				Value bridge;
				a_movie.CreateFunction(&bridge, &g_bridge, reinterpret_cast<void*>(static_cast<std::uintptr_t>(i)));
				if ((!a_mcm.SetMember(moved, original) || !a_mcm.SetMember(name, bridge)) && !g_warned.exchange(true)) {
					REX::WARN("MCM's {:s} could not be wrapped, so NEC's page cannot use it.", name);
				}
			}
			return true;
		}

		// Whether a_mcm's GetModSettingBool is NEC's, found by calling it. A
		// call for no mod is passed on to MCM's own, which answers false.
		bool Ours(Value& a_mcm)
		{
			g_heard.store(false);
			Value answer;
			a_mcm.Invoke("GetModSettingBool", &answer, std::array{ Value(""), Value("") });
			return g_heard.load();
		}

		// -------------------------------------------------------------------
		// The main and pause menus
		// -------------------------------------------------------------------

		std::atomic<bool> g_said{ false };

		// Called every frame while the main or pause menu is open. MCM sets
		// root.mcm while the movie loads, and by the first frame every plugin
		// has had the movie, so the order NEC and MCM load in does not matter.
		// Without MCM there is never a root.mcm.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (Due()) {
					Write();
				}
				if (a_params.movie) {
					Layout(*a_params.movie);
					if (g_refresh.exchange(false)) {
						RefreshMcm(*a_params.movie);
					}
				}

				Value mcm;
				if (!a_params.movie || !a_params.movie->GetVariable(&mcm, "_root.mcm") || !mcm.IsObject()) {
					return;
				}
				if (Wrap(*a_params.movie, mcm) && !g_said.load() && Ours(mcm)) {
					g_said.store(true);
					REX::INFO("The MCM page reads and writes NEC's settings.");
				}
			}
		};

		FrameListener g_frameListener;
	}

	// A key changed again for another file, as after a pick, is a change of
	// its own.
	void Owe(std::string_view a_section, std::string_view a_key, std::string_view a_before, std::string_view a_now, std::string_view a_default)
	{
		auto                   file = Presets::Target(a_section, a_key);
		const std::scoped_lock l{ g_owedLock };
		const auto             it = std::ranges::find_if(g_owed, [&](const Change& a_change) { return a_change.key == a_key && a_change.file == file; });
		if (it == g_owed.end()) {
			g_owed.push_back({ std::string{ a_section }, std::string{ a_key }, std::string{ a_before }, std::string{ a_now }, std::string{ a_default }, std::move(file) });
		} else {
			it->now = a_now;
		}
		g_changedAt.store(Clock::now().time_since_epoch().count());
	}

	void WriteOwed(std::string_view a_what)
	{
		const auto written = Write();
		TraceLog::Line("menu", "MCM owed {:d} {:s}, written before the {:s}.", written, written == 1 ? "change" : "changes", a_what);
	}

	void RefreshSoon()
	{
		g_refresh.store(true);
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!MenuMovies::IsMovie(a_file, "MainMenu.swf"sv)) {
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The main or pause menu has no stage to listen on, so NEC's settings page cannot answer this time.");
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The main or pause menu refused the frame listener, so NEC's settings page cannot answer this time.");
		}
	}
}
