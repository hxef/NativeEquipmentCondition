#pragma once

#include <string>
#include <string_view>

// The words of the presets rows on NEC's main MCM page and of the UI page's
// Reset UI settings button in the player's language, kept apart from Text.h,
// which is at its size limit. Their names and help lines are MenuLine's, see
// Text.h.
namespace Text
{
	// Default, the list's first entry.
	[[nodiscard]] std::string MenuPresetDefault();

	// "New Vegas (Custom)", a preset the player changed.
	[[nodiscard]] std::string MenuPresetCustom(std::string_view a_name);

	// The bodies of NEC's boxes after Reset preset and Delete preset and
	// after a delete that failed, "Reset New Vegas? ...". a_name is a
	// preset's name, never translated, or MenuPresetDefault.
	[[nodiscard]] std::string MenuPresetReset(std::string_view a_name);
	[[nodiscard]] std::string MenuPresetDelete(std::string_view a_name);
	[[nodiscard]] std::string MenuPresetDeleteFailed(std::string_view a_name);

	// The body of NEC's box after Reset UI settings.
	[[nodiscard]] std::string MenuUiReset();

	// The bodies of NEC's box after Save as new preset saved nothing: for a
	// typed name that kept no letter or digit, and for a file that could not
	// be written, a_name the new preset's name as the list would show it.
	[[nodiscard]] std::string MenuPresetSaveNoName();
	[[nodiscard]] std::string MenuPresetSaveFailed(std::string_view a_name);

	// The body of NEC's box after Describe preset could not write its line,
	// a_name the picked preset's name.
	[[nodiscard]] std::string MenuPresetDescribeFailed(std::string_view a_name);

	// The grey line under Preset. For Default, a_stem empty, Default's line.
	// For a shipped or downloaded preset, a_original, whose a_text is en.ini's
	// PRESET_ABOUT_ line of its file name, a_stem without NEC_, that line in
	// the player's language. Else a_text as written.
	[[nodiscard]] std::string MenuPresetAbout(std::string_view a_stem, bool a_original, std::string_view a_text);
}
