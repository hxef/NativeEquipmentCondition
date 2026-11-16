#include "UI/Mcm/Bridge.h"

#include "Core/Presets/Presets.h"
#include "Core/Text/Menu/MenuPresets.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"

#include <Scaleform/G/GFx_ASMovieRootBase.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

// What MCM asks of the presets rows on NEC's main page: the Preset list and
// its pick, the grey line under it, and the Save as new preset and Describe
// preset text fields. MCM asks for the list as GetListFromForm("NEC|presets")
// and for the pick's place in it at every build and refresh of the page, the
// place first. Each action writes what the page still owes first, so a
// setting made a moment before lands in the file it was made for. Reset
// preset and Delete preset are in PresetButtons.cpp.
namespace Mcm
{
	namespace
	{
		// The last line under Preset and what it was built from, since the
		// page asks for it every frame.
		struct BuiltAbout
		{
			std::mutex lock;
			bool built = false;
			Presets::About about;
			std::string code;  // the player's language
			std::string line;
		};
		BuiltAbout g_about;
	}

	// -------------------------------------------------------------------
	// The Preset list
	// -------------------------------------------------------------------

	// Default in the player's language, and Default or a shipped preset the
	// player changed with (Custom). MCM draws each as plain text and shrinks
	// a long one to fit its field.
	void GetList(const Params& a_params, std::string_view a_id)
	{
		if (!a_params.retVal || !a_params.movie || !a_params.movie->asMovieRoot) {
			return;
		}
		a_params.movie->CreateArray(a_params.retVal);
		if (a_id != PRESET_LIST) {
			Unknown(std::format("NEC|{:s}", a_id));
			return;
		}
		for (const auto& entry : Presets::Entries()) {
			const auto name = entry.kind == Presets::Kind::kDefault ? Text::MenuPresetDefault() : entry.name;
			const auto shown = entry.kind != Presets::Kind::kOwn && entry.changed > 0 ? Text::MenuPresetCustom(name) : name;
			Scaleform::GFx::Value text;
			a_params.movie->asMovieRoot->CreateString(&text, shown.c_str());
			a_params.retVal->PushBack(text);
		}
	}

	// MCM sends the place of the entry picked in the last list. The row's
	// refreshMenu draws the page again 0.05 s later.
	void PickPreset(const Params& a_params)
	{
		// Also false for NaN.
		const auto place = a_params.argCount > 2 ? Flash::AsNumber(a_params.args[2]) : -1.0;
		if (!(place >= 0.0 && place <= static_cast<double>(std::numeric_limits<std::uint32_t>::max()))) {
			Answer(a_params, false);
			return;
		}
		WriteOwed("pick");
		for (const auto* setting : Presets::Pick(static_cast<std::size_t>(std::lround(place)))) {
			Followed(*setting);
		}
		Answer(a_params, true);
	}

	// -------------------------------------------------------------------
	// The line under Preset
	// -------------------------------------------------------------------

	// MCM asks a text row's words once as it builds the page and never at a
	// refresh, so AboutRow writes the line on the row each time it changes.
	// It changes with a pick, a reset, a delete, a save or the language.
	std::string AboutLine()
	{
		auto about = Presets::PickedAbout();
		const std::string_view code = Text::LayoutNow().code;
		const std::scoped_lock l{ g_about.lock };
		auto& last = g_about.about;
		if (g_about.built && last.stem == about.stem && last.kind == about.kind && last.text == about.text && g_about.code == code) {
			return g_about.line;
		}
		const auto text = Presets::AboutCut(Text::MenuPresetAbout(Presets::Unprefixed(about.stem), about.kind == Presets::Kind::kOriginal, about.text));
		g_about.line = text.empty() ? std::string{} : std::format("<font color=\"{:s}\">{:s}</font>", GREY, Flash::HtmlEscaped(text));
		g_about.about = std::move(about);
		g_about.code = code;
		g_about.built = true;
		return g_about.line;
	}

	std::string AboutWords()
	{
		auto line = AboutLine();
		if (line.empty()) {
			line = " ";
		}
		return line;
	}

	// The row keeps the words Note answers, AboutWords, while it hides.
	bool AboutRow(Scaleform::GFx::Value& a_entries)
	{
		const auto count = a_entries.GetArraySize();
		for (std::uint32_t i = 0; i < count; i++) {
			Scaleform::GFx::Value entry;
			Scaleform::GFx::Value id;
			if (!a_entries.GetElement(i, &entry) || !entry.IsObject() || !entry.GetMember("id"sv, &id) || !id.IsString() || !id.GetString() ||
				id.GetString() != PRESET_ABOUT_NOTE) {
				continue;
			}
			const auto line = AboutWords();
			if (Flash::String(entry, "text"sv) == line) {
				return false;
			}
			entry.SetMember("text"sv, Scaleform::GFx::Value(std::string_view{ line }));
			TraceLog::Line("menu", "MCM row {:s} now reads {:s}", PRESET_ABOUT_NOTE, line);
			return true;
		}
		return false;
	}

	// -------------------------------------------------------------------
	// Describe preset
	// -------------------------------------------------------------------

	namespace
	{
		// The field shows only while an own preset is picked, and its
		// refreshMenu draws the page again, so the line under Preset follows
		// at once. A line that could not be written says so in NEC's box.
		void Describe(const Params& a_params, std::string_view a_typed)
		{
			WriteOwed("description");
			const auto end = Presets::Describe(a_typed);
			if (end == Presets::DescribeEnd::kNotWritten) {
				SayFailed(Text::MenuPresetDescribeFailed(Presets::PickedName()));
			}
			Answer(a_params, end == Presets::DescribeEnd::kKept || end == Presets::DescribeEnd::kRemoved);
		}
	}

	// -------------------------------------------------------------------
	// Save as new preset
	// -------------------------------------------------------------------

	// Both text fields answer empty, so after a save or a description each
	// reads empty again and the next text is sent, since MCM sends only a
	// text that differs from the one the field opened with.
	void GetString(const Params& a_params, std::string_view a_id)
	{
		if (a_id != PRESET_SAVE_ID && a_id != PRESET_DESCRIBE_ID) {
			Unknown(a_id);
		}
		AnswerText(a_params, {});
	}

	// MCM sends the text typed once Enter is pressed, a line for Describe
	// preset or a name. The new preset is picked with the settings in play,
	// so no value changes, and the row's refreshMenu draws the page again. A
	// save that failed says why in NEC's box, and the row empties all the
	// same.
	void SetString(const Params& a_params, std::string_view a_id)
	{
		const auto known = a_id == PRESET_SAVE_ID || a_id == PRESET_DESCRIBE_ID;
		if (!known) {
			Unknown(a_id);
		}
		if (!known || a_params.argCount < 3 || !a_params.args[2].IsString() || !a_params.args[2].GetString()) {
			Answer(a_params, false);
			return;
		}
		if (a_id == PRESET_DESCRIBE_ID) {
			Describe(a_params, a_params.args[2].GetString());
			return;
		}
		WriteOwed("save");
		const auto saved = Presets::Save(a_params.args[2].GetString(), Text::MenuPresetDefault());
		if (saved.end == Presets::SaveEnd::kNoName) {
			SayFailed(Text::MenuPresetSaveNoName());
		} else if (saved.end == Presets::SaveEnd::kNotWritten) {
			SayFailed(Text::MenuPresetSaveFailed(saved.name));
		}
		Answer(a_params, saved.end == Presets::SaveEnd::kSaved);
	}
}
