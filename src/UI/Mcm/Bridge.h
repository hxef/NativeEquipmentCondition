#pragma once

#include "Core/Plugin.h"
#include "Core/Settings.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What the 6 files of the folder share. Private to this folder.
namespace Mcm
{
	using Params = Scaleform::GFx::FunctionHandler::Params;

	// config.json's modName, which MCM also puts on every entry of NEC's page.
	inline constexpr std::string_view MOD = "NEC"sv;

	// NEC's answers on root.mcm, in Bridge.cpp, each for a call that is NEC's.
	// a_id is a value's id, bJam:Features or the hidden bJam:Free, or for
	// GetFullName a line's id, bJam, bJam.help or bJam.note.
	void GetBool(const Params& a_params, std::string_view a_id);
	void GetInt(const Params& a_params, std::string_view a_id);
	void GetFloat(const Params& a_params, std::string_view a_id);
	void SetBool(const Params& a_params, std::string_view a_id);
	void SetInt(const Params& a_params, std::string_view a_id);
	void SetFloat(const Params& a_params, std::string_view a_id);
	void GetFullName(const Params& a_params, std::string_view a_id);

	// Whether a_id is a_setting's id on the page, its key and section as in
	// bJam:Features, in Bridge.cpp.
	[[nodiscard]] bool IsIdOf(const Settings::Named& a_setting, std::string_view a_id);

	// What the hidden switches of the page's grey rows say, and what those
	// rows read, in Notes.cpp. a_id is parts:Note, bJam:Free, bJam:Note or
	// fDamageFloor:Note for Shows, and parts.note, bJam.note, fDamageFloor.note
	// or parts.help for Note. Nothing for any other id.
	[[nodiscard]] std::optional<bool>        Shows(std::string_view a_id);
	[[nodiscard]] std::optional<std::string> Note(std::string_view a_id);

	// How tall a grey row may be, in Fit.cpp: LinesOf, the lines a_line takes
	// on the page in the player's language, and Capped, a_lines cut to fit.
	// a_shorter builds line a_line again in a_room lines or fewer, or nothing
	// where it has no such form.
	using Shorter = std::function<std::optional<std::string>(std::size_t a_line, std::size_t a_room)>;

	struct Kept
	{
		std::vector<std::string> lines;
		std::size_t              whole = 0;  // how many of the first lines are kept as they were
	};

	[[nodiscard]] std::size_t LinesOf(std::string_view a_line);
	[[nodiscard]] Kept        Capped(std::vector<std::string> a_lines, const Shorter& a_shorter = {});

	// Notes a change from the page that NEC_custom.ini does not have yet, in
	// Mcm.cpp. a_before is kept from the first change still owed, so a dragged
	// slider is 1 write and 1 change line. a_default is NEC.ini's value, which
	// takes the key's line out of NEC_custom.ini instead.
	void Owe(std::string_view a_section, std::string_view a_key, std::string_view a_before, std::string_view a_now, std::string_view a_default);

	// What the game keeps worked out from a setting, done again on the game's
	// task queue after a change, in Mcm.cpp.
	enum class FollowUp : std::size_t
	{
		kCards,        // the Pip-Boy's item cards
		kResistances,  // every loaded actor's damage type resistances
		kCarried,      // what the player carries, see SpawnCondition::KeepCarried
		kTotal,
	};

	void Queue(FollowUp a_followUp);

	// The HUD bar sliders, in HudBar.cpp: the bar's place in HUD units from
	// the screen's top left, from HudCondition::Measured. HudBarShows
	// answers bHudCondition:Placed, false until the HUD has measured the
	// bar. HudBarValue and HudBarShift answer the 2 slider ids and
	// HudBarDefault their help ids, each nothing for any other id or before
	// that measure. HudBarRanges writes the live ends and value on NEC's
	// entries, true when it wrote one. HudBarHidden says whether a_setting is
	// a slider the page hides until that measure.
	[[nodiscard]] bool                        IsHudBar(std::string_view a_id);
	[[nodiscard]] bool                        HudBarHidden(const Settings::Named& a_setting);
	[[nodiscard]] std::optional<bool>         HudBarShows(std::string_view a_id);
	[[nodiscard]] std::optional<double>       HudBarValue(std::string_view a_id);
	[[nodiscard]] std::optional<float>        HudBarShift(std::string_view a_id, float a_number);
	[[nodiscard]] std::optional<std::int32_t> HudBarDefault(std::string_view a_id);
	[[nodiscard]] bool                        HudBarRanges(Scaleform::GFx::Value& a_entries);

	// Puts MCM's settings list in None mode while NEC's page shows, and back
	// once another page shows, and keeps the HUD bar sliders' live ends, in
	// Layout.cpp. Called every frame of the main or pause menu.
	void Layout(Scaleform::GFx::Movie& a_movie);
}
