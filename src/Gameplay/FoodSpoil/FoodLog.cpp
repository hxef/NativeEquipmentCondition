#include "Gameplay/FoodSpoil/Food.h"

#include "Core/LogFiles.h"
#include "Core/Runtime.h"
#include "Core/Settings.h"

#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <utility>

// NEC.food.log, beside NEC.log, with food spoiling on: the settings in force,
// what each fridge file and food file does, where spoiled food's rads come
// from, then every food and its kind. It is written whole, never added to, at
// the data load with food spoiling on and each time it is turned on, see
// FoodSpoil.cpp. What happens to food in play goes to NEC.food.trace.log, see
// Core/TraceLog.h.
namespace FoodSpoil
{
	namespace
	{
		// -------------------------------------------------------------------
		// The parts
		// -------------------------------------------------------------------

		struct Part
		{
			std::string_view heading;

			// Said when no line came, as when no food file is installed.
			std::string_view none;

			std::string text{};

			// Where the lines of the file being read begin, for its summary.
			std::size_t file{ 0 };
		};

		// In the order of LogPart and of the file.
		std::array<Part, 4> g_parts{ {
			{ "[Fridges]", "No fridge file, so NEC counts no container as a fridge." },
			{ "[Food files]", "No food file, so every food keeps the kind the rules give it." },
			{ "[Rads]", "" },
			{ "[Foods]", "" },
		} };

		bool g_written{ false };

		Part& PartOf(LogPart a_part)
		{
			return g_parts[std::to_underlying(a_part)];
		}

		// -------------------------------------------------------------------
		// The file
		// -------------------------------------------------------------------

		constexpr std::string_view FOOD_LOG = "NEC.food.log";

		constexpr std::string_view HEAD =
			"; What each setting, fridge file and food file does, then every food and its\n"
			"; kind. NEC.food.trace.log adds, with Log on in MCM, every stack stamped,\n"
			"; spoiled or slowed by a fridge.\n";

		// The settings paragraph is cut into lines no longer than the head's.
		constexpr std::size_t WIDTH = 77;

		// a_text cut at its spaces into lines of WIDTH at most, each ending in
		// a line break. A word longer than WIDTH gets a line of its own.
		std::string Wrapped(std::string_view a_text)
		{
			std::string out;
			std::size_t line = 0;
			while (!a_text.empty()) {
				const auto space = a_text.find(' ');
				const auto word = a_text.substr(0, space);
				if (line > 0 && line + 1 + word.size() > WIDTH) {
					out += '\n';
					line = 0;
				} else if (line > 0) {
					out += ' ';
					line++;
				}
				out += word;
				line += word.size();
				a_text.remove_prefix(space == std::string_view::npos ? a_text.size() : space + 1);
			}
			out += '\n';
			return out;
		}

		// The settings as they read now, in words. Each is shown as it is set.
		std::string SettingsText()
		{
			auto text = std::format(
				"Raw food and crops keep {:.2f} days, cooked food {:.2f}. A cooler or fridge spoils food at {:.2f} of the speed. "
				"Spoiled food heals {:d}% and gives {:.1f} times the extra rads. In Survival it fills {:d}% of the fresh food's hunger.",
				Settings::fRawFoodDays.GetValue(), Settings::fCookedFoodDays.GetValue(), Settings::fFridgeSpoilSpeed.GetValue(),
				Settings::iSpoiledHealing.GetValue(), Settings::fSpoiledRadsMult.GetValue(), Settings::iSpoiledHunger.GetValue());
			if (const auto fertilizer = FertilizerSentence(); !fertilizer.empty()) {
				text += ' ';
				text += fertilizer;
			}
			return Wrapped(text);
		}
	}

	// -------------------------------------------------------------------
	// Gathering
	// -------------------------------------------------------------------

	void Note(LogPart a_part, std::string_view a_line)
	{
		auto& part = PartOf(a_part);
		part.text += a_line;
		part.text += '\n';
	}

	void NoteFile(LogPart a_part, std::string_view a_line)
	{
		auto& part = PartOf(a_part);
		std::string line{ a_line };
		line += '\n';
		part.text.insert(part.file, line);
		part.file = part.text.size();
	}

	void ForgetFoodLog()
	{
		for (auto& part : g_parts) {
			part.text.clear();
			part.file = 0;
		}
		g_written = false;
	}

	// -------------------------------------------------------------------
	// Writing
	// -------------------------------------------------------------------

	bool WriteFoodLog()
	{
		auto path = LogFiles::MainFile();
		if (path.empty()) {
			return false;
		}
		path.replace_filename(FOOD_LOG);

		const auto nec = F4SE::GetPluginVersion();
		auto text = std::format("; NEC {:d}.{:d}.{:d} on Fallout 4 {:s} with F4SE {:s}. Food spoiling is on.\n", nec.major(),
			nec.minor(), nec.patch(), Runtime::GameVersion(), Runtime::F4seVersion());
		text += HEAD;
		text += "\n[Settings]\n";
		text += SettingsText();
		for (const auto& part : g_parts) {
			text += '\n';
			text += part.heading;
			text += '\n';
			if (part.text.empty() && !part.none.empty()) {
				text += part.none;
				text += '\n';
			} else {
				text += part.text;
			}
		}

		std::ofstream file{ path, std::ios::binary | std::ios::trunc };
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		file.close();
		if (file.fail()) {
			REX::WARN("Could not write {:s}.", LogFiles::Shown(path));
			return false;
		}
		g_written = true;
		return true;
	}

	bool FoodLogWritten()
	{
		return g_written;
	}
}
