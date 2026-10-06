#include "UI/Mcm/Bridge.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <Scaleform/G/GFx_ASMovieRootBase.h>

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>

// MCM.swf asks root.mcm for a value as GetModSettingBool("NEC", "bJam:Features"),
// changes it with the new value as a third argument, and asks for a line of
// text as GetFullName("NEC|bJam"). These answer the calls Mcm.cpp finds are
// NEC's, from Settings, Core/Text and, through Notes.cpp, what CallPatch left
// to other mods.
namespace Mcm
{
	namespace
	{
		// The log level is a number on the page, its place in the stepper from
		// 0 for trace, and a word in NEC.ini.
		constexpr std::string_view LOG_LEVEL_ID = "iLogLevel:Log"sv;
		constexpr std::string_view LOG_SECTION = "Log"sv;
		constexpr std::string_view LOG_KEY = "sLogLevel"sv;

		// What the game keeps worked out from a setting, done again once the
		// setting changes.
		struct Owes
		{
			const Settings::Named* setting;
			FollowUp               followUp;
		};

		constexpr Owes OWES[]{
			{ &Settings::bFireRate, FollowUp::kCards },
			{ &Settings::fDamageFloor, FollowUp::kCards },
			{ &Settings::fArmorFloor, FollowUp::kCards },
			{ &Settings::fArmorFloor, FollowUp::kResistances },
			{ &Settings::fValueExponent, FollowUp::kCards },
			{ &Settings::fFireRateFloor, FollowUp::kCards },
		};

		// Said once for each id, since the page asks again every time it is
		// drawn. An unknown id means config.json and NEC.dll are from different
		// versions.
		void Unknown(std::string_view a_id)
		{
			static std::mutex                      lock;
			static std::unordered_set<std::string> said;
			const std::scoped_lock                 l{ lock };
			if (said.emplace(a_id).second) {
				REX::WARN("The MCM page asks for {:s}, which NEC does not know.", a_id);
			}
		}

		template <class T>
		void Answer(const Params& a_params, T a_value)
		{
			if (a_params.retVal) {
				*a_params.retVal = a_value;
			}
		}

		// Whether a_id is a_setting's id on the page, its key and section as
		// in bJam:Features.
		bool IsIdOf(const Settings::Named& a_setting, std::string_view a_id)
		{
			return a_id.size() == a_setting.key.size() + 1 + a_setting.section.size() &&
			       a_id.starts_with(a_setting.key) && a_id[a_setting.key.size()] == ':' &&
			       a_id.ends_with(a_setting.section);
		}

		Settings::Live<bool>* SwitchFor(std::string_view a_id)
		{
			for (auto* on : Settings::Switches()) {
				if (IsIdOf(*on, a_id)) {
					return on;
				}
			}
			return IsIdOf(Settings::bTraceLogs, a_id) ? &Settings::bTraceLogs : nullptr;
		}

		// A number by its id on the page, a float or a whole number as T asks.
		template <class T>
		Settings::Live<T>* NumberFor(std::string_view a_id)
		{
			for (const auto& number : Settings::Numbers()) {
				auto* const* live = std::get_if<Settings::Live<T>*>(&number);
				if (live && IsIdOf(**live, a_id)) {
					return *live;
				}
			}
			return nullptr;
		}

		void QueueFollowUps(const Settings::Named& a_setting)
		{
			for (const auto& [setting, followUp] : OWES) {
				if (setting == &a_setting) {
					Queue(followUp);
				}
			}
		}

		std::string_view Word(bool a_value)
		{
			return a_value ? "true"sv : "false"sv;
		}
	}

	// -------------------------------------------------------------------------
	// The answers
	// -------------------------------------------------------------------------

	void GetBool(const Params& a_params, std::string_view a_id)
	{
		if (const auto shows = Shows(a_id)) {
			Answer(a_params, *shows);
			return;
		}
		const auto* on = SwitchFor(a_id);
		if (!on) {
			Unknown(a_id);
		}
		Answer(a_params, on && on->GetValue());
	}

	void GetInt(const Params& a_params, std::string_view a_id)
	{
		if (const auto* whole = NumberFor<std::int32_t>(a_id)) {
			Answer(a_params, whole->GetValue());
			return;
		}
		if (a_id != LOG_LEVEL_ID) {
			Unknown(a_id);
			Answer(a_params, std::int32_t{ 0 });
			return;
		}
		Answer(a_params, static_cast<std::int32_t>(Settings::LogLevelIndex()));
	}

	void GetFloat(const Params& a_params, std::string_view a_id)
	{
		const auto* number = NumberFor<float>(a_id);
		if (!number) {
			Unknown(a_id);
		}
		Answer(a_params, number ? static_cast<double>(number->GetValue()) : 0.0);
	}

	// A switch left to another mod stays off while the game runs, see
	// CallPatch.h, so the page cannot turn it on.
	void SetBool(const Params& a_params, std::string_view a_id)
	{
		auto* on = SwitchFor(a_id);
		if (!on) {
			Unknown(a_id);
		}
		if (!on || a_params.argCount < 3) {
			Answer(a_params, false);
			return;
		}
		if (CallPatch::IsYielded(*on)) {
			REX::INFO("{:s} stays false. {:s}", on->key, CallPatch::OffLine(*on));
			Answer(a_params, false);
			return;
		}

		const auto& arg = a_params.args[2];
		const auto  value = arg.IsBoolean() ? arg.GetBoolean() : Flash::AsNumber(arg) != 0.0;
		const auto  before = on->GetValue();
		on->SetValue(value);
		if (on == &Settings::bTraceLogs) {
			TraceLog::Switch(value);
		}
		Owe(on->section, on->key, Word(before), Word(value), Word(on->GetValueDefault()));

		if (value != before) {
			QueueFollowUps(*on);
			// What arrived while Worn loot was off has no condition, and the
			// next save load would roll it as loot.
			if (on == &Settings::bSpawnCondition && value) {
				Queue(FollowUp::kCarried);
			}
		}
		Answer(a_params, true);
	}

	void SetInt(const Params& a_params, std::string_view a_id)
	{
		auto* whole = NumberFor<std::int32_t>(a_id);
		if (!whole && a_id != LOG_LEVEL_ID) {
			Unknown(a_id);
		}
		if ((!whole && a_id != LOG_LEVEL_ID) || a_params.argCount < 3) {
			Answer(a_params, false);
			return;
		}
		// Also false for NaN.
		const auto place = Flash::AsNumber(a_params.args[2]);
		if (whole) {
			if (!(place >= static_cast<double>(std::numeric_limits<std::int32_t>::min()) &&
					place <= static_cast<double>(std::numeric_limits<std::int32_t>::max()))) {
				Answer(a_params, false);
				return;
			}
			const auto value = static_cast<std::int32_t>(std::lround(place));
			const auto before = whole->GetValue();
			whole->SetValue(value);
			Owe(whole->section, whole->key, std::format("{:d}", before), std::format("{:d}", value), std::format("{:d}", whole->GetValueDefault()));
			if (value != before) {
				QueueFollowUps(*whole);
			}
			Answer(a_params, true);
			return;
		}
		if (!(place >= 0.0 && place <= static_cast<double>(std::numeric_limits<std::uint32_t>::max()))) {
			Answer(a_params, false);
			return;
		}

		const std::string before{ Settings::LogLevelWord() };
		Settings::SetLogLevel(static_cast<std::uint32_t>(place));
		Owe(LOG_SECTION, LOG_KEY, before, Settings::LogLevelWord(), Settings::LogLevelDefaultWord());
		Answer(a_params, true);
	}

	void SetFloat(const Params& a_params, std::string_view a_id)
	{
		auto* number = NumberFor<float>(a_id);
		if (!number) {
			Unknown(a_id);
		}
		if (!number || a_params.argCount < 3) {
			Answer(a_params, false);
			return;
		}
		auto value = static_cast<float>(Flash::AsNumber(a_params.args[2]));
		if (!std::isfinite(value)) {
			Answer(a_params, false);
			return;
		}
		// A slider dragged back to 0 can send -0, which prints as -0 and
		// would never match NEC.ini's 0.
		if (value == 0.0F) {
			value = 0.0F;
		}

		const auto before = number->GetValue();
		number->SetValue(value);
		Owe(number->section, number->key, std::format("{:g}", before), std::format("{:g}", value), std::format("{:g}", number->GetValueDefault()));
		if (value != before) {
			QueueFollowUps(*number);
		}
		Answer(a_params, true);
	}

	// An empty answer leaves MCM's own text, which is the id itself. MCM
	// draws every line as htmlText, names and section titles too, so each is
	// escaped.
	void GetFullName(const Params& a_params, std::string_view a_id)
	{
		std::string text;
		if (auto note = Note(a_id)) {
			text = std::move(*note);
		} else {
			text = Flash::HtmlEscaped(Text::MenuLine(a_id));
			if (text.empty()) {
				Unknown(std::format("NEC|{:s}", a_id));
			}
		}
		// A plain Value keeps only a pointer to the text, so the movie is
		// handed a copy of its own.
		if (a_params.retVal && a_params.movie && a_params.movie->asMovieRoot) {
			a_params.movie->asMovieRoot->CreateString(a_params.retVal, text.c_str());
		}
	}
}
