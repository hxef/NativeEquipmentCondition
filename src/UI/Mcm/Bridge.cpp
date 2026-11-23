#include "UI/Mcm/Bridge.h"

#include "Condition/Provenance/Provenance.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Presets/Presets.h"
#include "Core/Settings/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Mcm/Hides.h"

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

// MCM.swf asks root.mcm for a value as
// GetModSettingBool("NEC", "bSpawnCondition:Features"), changes it with the
// new value as a third argument, and asks for a line of text as
// GetFullName("NEC|bSpawnCondition"). These answer the calls Mcm.cpp finds are
// NEC's, from Settings, Core/Text, through Notes.cpp what CallPatch left to
// other mods, through HudBar.cpp the HUD bar's place on screen, and through
// PresetRows.cpp the presets rows.
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
			// bNecOn changes every card, resistance and perk line NEC adds.
			{ &Settings::bNecOn, FollowUp::kCards },
			{ &Settings::bNecOn, FollowUp::kResistances },
			{ &Settings::bNecOn, FollowUp::kPerks },
			{ &Settings::bNecOn, FollowUp::kFoodCards },
			{ &Settings::bPlainApparelNeverWears, FollowUp::kCards },
			{ &Settings::fDamageFloor, FollowUp::kCards },
			{ &Settings::fWeaponPenaltyStart, FollowUp::kCards },
			{ &Settings::fArmorFloor, FollowUp::kCards },
			{ &Settings::fArmorFloor, FollowUp::kResistances },
			{ &Settings::fArmorPenaltyStart, FollowUp::kCards },
			{ &Settings::fArmorPenaltyStart, FollowUp::kResistances },
			{ &Settings::fValueExponent, FollowUp::kCards },
			{ &Settings::fFireRateFloor, FollowUp::kCards },
			{ &Settings::fAccuracyFloor, FollowUp::kCards },
			{ &Settings::fRangeFloor, FollowUp::kCards },
			{ &Settings::fAutoWearRateMult, FollowUp::kCards },
			{ &Settings::fSuppressedWearRateMult, FollowUp::kCards },
			{ &Settings::bPowerArmorCardCnd, FollowUp::kCards },
			// The Pip-Boy keeps its cards, so the marks beside CND go or come
			// with the rate only once they are built again.
			{ &Settings::bCardWearRate, FollowUp::kCards },
			// Item card CND adds or drops the CND row of every card, food's
			// freshness row among them.
			{ &Settings::iItemCardCnd, FollowUp::kCards },
			{ &Settings::iItemCardCnd, FollowUp::kFoodCards },
			// iCndDecimals changes every CND row and a perk line's start point.
			{ &Settings::iCndDecimals, FollowUp::kCards },
			{ &Settings::iCndDecimals, FollowUp::kFoodCards },
			{ &Settings::iCndDecimals, FollowUp::kPerks },
			// These change a line NEC adds to a perk description. bLockSpares
			// changes none.
			{ &Settings::bWorkbenchRepair, FollowUp::kPerks },
			{ &Settings::bSpareRepair, FollowUp::kPerks },
			{ &Settings::iPerkComponentDiscount, FollowUp::kPerks },
			{ &Settings::iRepairLock, FollowUp::kPerks },
			{ &Settings::iPerkLock, FollowUp::kPerks },
			{ &Settings::fBenchCostMult, FollowUp::kPerks },
			{ &Settings::fSuppliesOnlyFrom, FollowUp::kPerks },
			{ &Settings::iRepairSupplies, FollowUp::kPerks },
			{ &Settings::fFreeMendFrom, FollowUp::kPerks },
		};

		std::string_view Word(bool a_value)
		{
			return a_value ? "true"sv : "false"sv;
		}
	}

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

	// A plain Value keeps only a pointer to the text, so the movie is handed
	// a copy of its own.
	void AnswerText(const Params& a_params, const std::string& a_text)
	{
		if (a_params.retVal && a_params.movie && a_params.movie->asMovieRoot) {
			a_params.movie->asMovieRoot->CreateString(a_params.retVal, a_text.c_str());
		}
	}

	// -------------------------------------------------------------------
	// The answers
	// -------------------------------------------------------------------

	void Followed(const Settings::Named& a_setting)
	{
		for (const auto& [setting, followUp] : OWES) {
			if (setting == &a_setting) {
				Queue(followUp);
			}
		}
		// What arrived while Worn loot was off has no condition, so it goes
		// to full now and not only at the next load.
		if (&a_setting == &Settings::bSpawnCondition && Settings::bSpawnCondition.GetValue()) {
			Queue(FollowUp::kCarried);
		}
		// Food carried while food spoiling was off starts fresh now.
		if (&a_setting == &Settings::bFoodSpoils && Settings::bFoodSpoils.GetValue()) {
			Queue(FollowUp::kFood);
		}
		// bNecOn back on does what Worn loot and food spoiling each do as they
		// turn on. No preset holds bNecOn, so only its switch gets here.
		if (&a_setting == &Settings::bNecOn && Settings::bNecOn.GetValue()) {
			if (Settings::bSpawnCondition.GetValue()) {
				Queue(FollowUp::kCarried);
			}
			if (Settings::bFoodSpoils.GetValue()) {
				Queue(FollowUp::kFood);
			}
		}
		// Built or dropped at once, so the next roll finds the areas the
		// slider says. Only a move to or from off changes them.
		if (&a_setting == &Settings::fLootAreaEffect) {
			Provenance::FollowAreaEffect();
		}
	}

	void GetBool(const Params& a_params, std::string_view a_id)
	{
		if (HideOf(a_id) || a_id == PRESET_CHANGES_ID || a_id == PRESET_OWN_ID || a_id == PRESET_ABOUT_ID) {
			KeepOpenPage(a_params.movie, a_id);
		}
		if (const auto shows = Shows(a_id)) {
			Answer(a_params, *shows);
			return;
		}
		if (const auto shows = HudBarShows(a_id)) {
			Answer(a_params, *shows);
			return;
		}
		const auto* on = Settings::SwitchById(a_id);
		if (!on) {
			Unknown(a_id);
		}
		Answer(a_params, on && on->GetValue());
	}

	void GetInt(const Params& a_params, std::string_view a_id)
	{
		if (a_id == PRESET_ID) {
			Answer(a_params, static_cast<std::int32_t>(Presets::Picked()));
			return;
		}
		if (const auto* whole = Settings::NumberById<std::int32_t>(a_id)) {
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
		if (const auto shown = HudBarValue(a_id)) {
			Answer(a_params, *shown);
			return;
		}
		const auto* number = Settings::NumberById<float>(a_id);
		if (!number) {
			Unknown(a_id);
		}
		Answer(a_params, number ? static_cast<double>(number->GetValue()) : 0.0);
	}

	// A switch left to another mod stays off while the game runs, see
	// CallPatch.h, so the page cannot turn it on.
	void SetBool(const Params& a_params, std::string_view a_id)
	{
		auto* on = Settings::SwitchById(a_id);
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
		Owe(on->section, on->key, Word(before), Word(value), Presets::Base(*on));
		if (value != before) {
			Followed(*on);
		}
		Answer(a_params, true);
	}

	// A CND stepper left to another mod stays at Off, as a switch stays off,
	// see SetBool.
	void SetInt(const Params& a_params, std::string_view a_id)
	{
		if (a_id == PRESET_ID) {
			PickPreset(a_params);
			return;
		}
		auto* whole = Settings::NumberById<std::int32_t>(a_id);
		if (!whole && a_id != LOG_LEVEL_ID) {
			Unknown(a_id);
		}
		if ((!whole && a_id != LOG_LEVEL_ID) || a_params.argCount < 3) {
			Answer(a_params, false);
			return;
		}
		if (whole && CallPatch::IsYielded(*whole)) {
			REX::INFO("{:s} stays 0. {:s}", whole->key, CallPatch::OffLine(*whole));
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
			Owe(whole->section, whole->key, std::format("{:d}", before), std::format("{:d}", value), Presets::Base(*whole));
			if (value != before) {
				Followed(*whole);
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
		auto* number = Settings::NumberById<float>(a_id);
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
		if (IsHudBar(a_id)) {
			const auto shift = HudBarShift(a_id, value);
			if (!shift) {
				Answer(a_params, false);
				return;
			}
			value = *shift;
		}
		// A slider dragged back to 0, or a HUD bar number at the bar's own
		// place, can give -0, which prints as -0 and would never match
		// NEC.ini's 0.
		if (value == 0.0F) {
			value = 0.0F;
		}

		const auto before = number->GetValue();
		number->SetValue(value);
		Owe(number->section, number->key, std::format("{:g}", before), std::format("{:g}", value), Presets::Base(*number));
		if (value != before) {
			Followed(*number);
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
			auto line = Text::MenuLine(a_id);
			if (const auto number = HudBarDefault(a_id); number && !line.empty()) {
				line = Text::MenuHudBarDefault(line, *number);
			}
			text = Flash::HtmlEscaped(line);
			if (text.empty()) {
				Unknown(std::format("NEC|{:s}", a_id));
			}
		}
		AnswerText(a_params, text);
	}
}
