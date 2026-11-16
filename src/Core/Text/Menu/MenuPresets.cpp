#include "Core/Text/Menu/MenuPresets.h"

#include "Core/Settings/Settings.h"
#include "Core/Text/Lines.h"

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// The names and help lines of the presets rows on NEC's main MCM page, see
// MenuLine in Text.h, the list's own words and the bodies of NEC's boxes.
// Files and NEC.log name presets in English, see Core/Presets/Presets.h.
namespace Text
{
	namespace
	{
		// The 5 rows are no settings of NEC.ini, so each names its id on the
		// page itself, as config.json gives it.
		constexpr Settings::Named PRESET_ROW{ "Presets", "iPreset" };
		constexpr Settings::Named RESET_ROW{ "Presets", "ResetPreset" };
		constexpr Settings::Named DELETE_ROW{ "Presets", "DeletePreset" };
		constexpr Settings::Named SAVE_ROW{ "Presets", "sSavePreset" };
		constexpr Settings::Named DESCRIBE_ROW{ "Presets", "sDescribePreset" };

		constexpr MenuRow ROWS[]{
			{ &PRESET_ROW, PRESET_NAME, PRESET_HELP },
			{ &RESET_ROW, PRESET_RESET_NAME, PRESET_RESET_HELP },
			{ &DELETE_ROW, PRESET_DELETE_NAME, PRESET_DELETE_HELP },
			{ &SAVE_ROW, PRESET_SAVE_NAME, PRESET_SAVE_HELP },
			{ &DESCRIBE_ROW, PRESET_DESCRIBE_NAME, PRESET_DESCRIBE_HELP },
		};
	}

	std::span<const MenuRow> MenuPresets()
	{
		return ROWS;
	}

	std::string MenuPresetDefault()
	{
		return Pick(PRESET_DEFAULT);
	}

	std::string MenuPresetCustom(std::string_view a_name)
	{
		return Say(PRESET_CUSTOM, a_name);
	}

	std::string MenuPresetReset(std::string_view a_name)
	{
		return Say(PRESET_RESET_BODY, a_name);
	}

	std::string MenuPresetDelete(std::string_view a_name)
	{
		return Say(PRESET_DELETE_BODY, a_name);
	}

	std::string MenuPresetDeleteFailed(std::string_view a_name)
	{
		return Say(PRESET_DELETE_FAILED, a_name);
	}

	std::string MenuPresetSaveNoName()
	{
		return Pick(PRESET_SAVE_NO_NAME);
	}

	std::string MenuPresetSaveFailed(std::string_view a_name)
	{
		return Say(PRESET_SAVE_FAILED, a_name);
	}

	std::string MenuPresetDescribeFailed(std::string_view a_name)
	{
		return Say(PRESET_DESCRIBE_FAILED, a_name);
	}

	// A shipped file someone rewrote shows its own words, so the line is
	// translated only while the file holds en.ini's English exactly.
	std::string MenuPresetAbout(std::string_view a_stem, bool a_original, std::string_view a_text)
	{
		if (a_stem.empty()) {
			return Pick(PRESET_ABOUT_DEFAULT);
		}
		if (const auto key = a_original ? KeyOf(std::format("PRESET_ABOUT_{:s}", a_stem)) : std::nullopt;
			key && std::string_view{ InEnglish(*key) } == a_text) {
			return Pick(*key);
		}
		return std::string{ a_text };
	}
}
