#include "Core/Presets/Shared.h"

#include "Core/IniText.h"
#include "Core/Settings/Settings.h"
#include "Core/Utf8.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <mutex>
#include <string>
#include <string_view>

// A preset's description, the line NEC's main MCM page shows under Preset.
// It comes from a hand written file, so it is cleaned and cut before the page
// gets it, see AboutCut. Describe writes it for a preset of the player's own.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// The letters
		// -------------------------------------------------------------------

		// a_text up to its first byte that is not UTF-8, so Flash never gets
		// half a letter, each control character a space, no space at either
		// end.
		std::string CleanedLine(std::string_view a_text)
		{
			std::string clean;
			while (const auto size = Utf8::LetterSize(a_text)) {
				const auto letter = a_text.substr(0, size);
				clean += Utf8::IsControl(letter) ? " "sv : letter;
				a_text.remove_prefix(size);
			}
			return std::string{ IniText::Trimmed(clean) };
		}
	}

	std::size_t LettersOf(std::string_view a_text)
	{
		std::size_t letters = 0;
		while (const auto size = Utf8::LetterSize(a_text)) {
			a_text.remove_prefix(size);
			++letters;
		}
		return letters;
	}

	// -------------------------------------------------------------------
	// The pick's description
	// -------------------------------------------------------------------

	About PickedAbout()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		return { .stem = state.picked.stem, .kind = state.picked.kind, .text = state.about };
	}

	// SimpleIni takes 1 pair of quotes off a value that starts and ends with
	// ", so such a text gets 1 more pair to read back as it was.
	std::string AboutWritten(std::string_view a_text)
	{
		const auto quoted = a_text.size() >= 2 && a_text.front() == '"' && a_text.back() == '"';
		return quoted ? std::format("\"{:s}\"", a_text) : std::string{ a_text };
	}

	// -------------------------------------------------------------------
	// The line as the page shows it
	// -------------------------------------------------------------------

	std::string AboutCut(std::string_view a_text)
	{
		const auto clean = CleanedLine(a_text);
		const std::string_view text = clean;

		// The bytes of the first MOST_ABOUT_LETTERS letters, and the last
		// space among them past the half. text holds whole letters only, so
		// each step moves on.
		std::size_t end = 0;
		std::size_t space = std::string_view::npos;
		for (std::size_t letter = 0; letter < MOST_ABOUT_LETTERS && end < text.size(); ++letter) {
			if (text[end] == ' ' && letter >= MOST_ABOUT_LETTERS / 2) {
				space = end;
			}
			end += Utf8::LetterSize(text.substr(end));
		}
		if (end == text.size()) {
			return std::string{ text };
		}
		const auto kept = text.substr(0, space != std::string_view::npos ? space : end);
		return std::string{ IniText::Trimmed(kept) } + "...";
	}

	// -------------------------------------------------------------------
	// Describe preset
	// -------------------------------------------------------------------

	DescribeEnd Describe(std::string_view a_typed)
	{
		const auto text = CleanedLine(a_typed);
		auto& state = Now();
		std::filesystem::path file;
		std::string name;
		bool had = false;
		{
			const std::scoped_lock l{ state.lock };
			if (state.picked.kind != Kind::kOwn) {
				REX::WARN("MCM sent a description while {:s} is picked, which is no preset of your own, so NEC writes none.", state.picked.name);
				return DescribeEnd::kNotOwn;
			}
			file = state.picked.file;
			name = state.picked.name;
			had = !state.about.empty();
		}

		// Keep and Drop take the settings lock and NewText takes this one, so
		// neither is held here. A file removed by hand is written again whole.
		const auto saved = text.empty() ? Settings::Drop(ABOUT_KEY.section, ABOUT_KEY.key, file, NewText(file, true)) :
		                                  Settings::Keep(ABOUT_KEY.section, ABOUT_KEY.key, AboutWritten(text), file, NewText(file));
		Wrote(file);
		if (!saved) {
			return DescribeEnd::kNotWritten;
		}

		{
			// The pick may have moved on while the file was written.
			const std::scoped_lock l{ state.lock };
			if (state.picked.kind == Kind::kOwn && SameFile(state.picked.file.native(), file.native())) {
				state.about = text;
			}
		}
		if (text.empty()) {
			// A preset that had none gets no line about a key it never set.
			if (had) {
				REX::INFO("Preset {:s} has no description now, and {:s} no longer sets {:s}.", name, LogName(file), ABOUT_KEY.key);
			} else {
				REX::INFO("Preset {:s} has no description.", name);
			}
			return DescribeEnd::kRemoved;
		}
		REX::INFO("Preset {:s} has the description \"{:s}\" now, in {:s}.", name, text, LogName(file));
		return DescribeEnd::kKept;
	}
}
