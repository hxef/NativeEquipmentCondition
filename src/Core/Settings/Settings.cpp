#include "Core/Settings/Settings.h"

#include "Core/IniText.h"
#include "Core/Presets/Presets.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <mutex>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// Reading NEC.ini and NEC_custom.ini, and keeping a change from the MCM page
// in NEC_custom.ini or in a file of the presets' Custom folder. The Settings
// line and the lists of switches and numbers are in Line.cpp.
namespace Settings
{
	namespace
	{
		// -------------------------------------------------------------------
		// The 2 files
		// -------------------------------------------------------------------

		// Both files sit in NEC's Config folder, beside the presets' folders of
		// Presets/Shared.h, and the game runs from its own folder.
		// Literals, since a store keeps only a view of what it is handed.
		constexpr const char* BASE_FILE = "Data/F4SE/Plugins/NEC/Config/NEC.ini";
		constexpr const char* USER_FILE = "Data/F4SE/Plugins/NEC/Config/NEC_custom.ini";

		// Changes from the page arrive one after another, and each reads the
		// file the last one wrote. Keep and Drop share this one lock for
		// NEC_custom.ini and every file of the presets' Custom folder.
		std::mutex g_userFileLock;

		bool Exists(const std::filesystem::path& a_file)
		{
			std::error_code ec;
			return std::filesystem::exists(a_file, ec);
		}

		// The whole file, or false when any of it cannot be read or it
		// changes size while it is read.
		bool ReadWhole(const std::filesystem::path& a_file, std::string& a_text)
		{
			std::error_code ec;
			const auto      size = std::filesystem::file_size(a_file, ec);
			if (ec) {
				return false;
			}
			std::ifstream file{ a_file, std::ios::binary };
			a_text.resize(size);
			return file && file.read(a_text.data(), static_cast<std::streamsize>(size)) &&
			       file.gcount() == static_cast<std::streamsize>(size) && file.peek() == std::ifstream::traits_type::eof();
		}

		bool WriteWhole(const std::filesystem::path& a_file, std::string_view a_text)
		{
			std::ofstream file{ a_file, std::ios::binary | std::ios::trunc };
			file.write(a_text.data(), static_cast<std::streamsize>(a_text.size()));
			file.close();
			return !file.fail();
		}

		// At info, or at the log's own level when that is higher, so no level
		// hides the line.
		void Say(const std::string& a_line, std::source_location a_where = std::source_location::current())
		{
			REX::Impl::Log(a_where, std::max(LogLevelNow(), REX::ELogLevel::Info), a_line);
		}

		// What an edit of a file came to.
		enum class Edited
		{
			kFailed,
			kSame,  // the edit left the text as it was, so nothing is written
			kWritten,
		};

		// Edits a_file through a_edit, which is handed the whole text and
		// gives it back changed, or nothing when the game would not read the
		// edit. A file that is not there starts as a_first. a_what names the
		// change in a warning, such as "the change to fDamageFloor".
		template <class Edit>
		Edited Rewrite(const std::filesystem::path& a_file, std::string_view a_first, std::string_view a_what, Edit a_edit)
		{
			const std::scoped_lock l{ g_userFileLock };
			const auto             name = Presets::LogName(a_file);

			// A file that cannot be read whole is never written over, or the
			// player's other keys would be lost.
			std::error_code ec;
			const auto      exists = std::filesystem::exists(a_file, ec);
			std::string     text;
			if (ec || (exists && !ReadWhole(a_file, text))) {
				REX::WARN("{:s} could not be read, so {:s} is not saved in it.", name, a_what);
				return Edited::kFailed;
			}

			// With no file nothing is on disk, so a_first alone is new text.
			const std::string before = text;
			if (!exists) {
				text = a_first;
			}
			const std::optional<std::string> edited = a_edit(std::string_view{ text });
			if (!edited) {
				REX::WARN("{:s} is not plain text or has a line that is only [, so {:s} is not saved in it.", name, a_what);
				return Edited::kFailed;
			}
			if (*edited == before) {
				return Edited::kSame;
			}

			// Written beside the file first and renamed over it, so a write
			// cut short never leaves the player's file half written. A new
			// file's folder is made first, since no presets' Custom folder
			// ships. A folder that is there stays as it is.
			auto fresh = a_file;
			fresh += L".new";
			if (!exists) {
				std::filesystem::create_directories(a_file.parent_path(), ec);
			}
			if (!WriteWhole(fresh, *edited)) {
				REX::WARN("{:s} could not be written, so {:s} is not saved in {:s}.", Presets::LogName(fresh), a_what, name);
				std::filesystem::remove(fresh, ec);
				return Edited::kFailed;
			}
			std::filesystem::rename(fresh, a_file, ec);
			if (ec) {
				REX::WARN("{:s} could not be replaced, so {:s} is not saved in it: {:s}", name, a_what, ec.message());
				std::filesystem::remove(fresh, ec);
				return Edited::kFailed;
			}
			return Edited::kWritten;
		}
	}

	// -------------------------------------------------------------------
	// Reading, changing and reporting
	// -------------------------------------------------------------------

	void Load()
	{
		// The base file first and the custom file over it, read into one
		// table, so the second pass sees every key and sets each value once.
		// With no custom file the base file is read twice for that pass.
		auto* store = REX::FIniSettingStore::GetSingleton();
		store->Init(BASE_FILE, Exists(USER_FILE) ? USER_FILE : BASE_FILE);
		store->Load();
		Presets::Load();
	}

	void LogChange(std::string_view a_key, std::string_view a_old, std::string_view a_new, std::string_view a_from)
	{
		Say(std::format("{:s}: {:s} to {:s}, {:s}.", a_key, a_old, a_new, a_from));
	}

	const std::filesystem::path& UserFile()
	{
		static const std::filesystem::path file{ USER_FILE };
		return file;
	}

	const std::filesystem::path& BaseFile()
	{
		static const std::filesystem::path file{ BASE_FILE };
		return file;
	}

	bool Keep(std::string_view a_section, std::string_view a_key, std::string_view a_text, const std::filesystem::path& a_file,
		std::string_view a_first)
	{
		return Rewrite(a_file, a_first, std::format("the change to {:s}", a_key), [&](std::string_view a_old) {
			return IniText::WithKey(a_old, a_section, a_key, a_text);
		}) != Edited::kFailed;
	}

	bool Drop(std::string_view a_section, std::string_view a_key, const std::filesystem::path& a_file, std::string_view a_first,
		std::string_view a_stands)
	{
		const auto edited = Rewrite(a_file, a_first, std::format("the change to {:s}", a_key), [&](std::string_view a_old) {
			return IniText::WithoutKey(a_old, a_section, a_key);
		});
		// The presets' own lines say what a pick or a description did to
		// [Presets].
		const auto pick = a_section == PRESET_KEY.section;
		if (edited == Edited::kWritten && !pick) {
			Say(std::format("{:s} no longer sets {:s}, so {:s} value stands.", Presets::LogName(a_file), a_key, a_stands));
		}
		return edited != Edited::kFailed;
	}

	std::optional<std::size_t> DropAll(const std::filesystem::path& a_file, std::span<const Named* const> a_keys)
	{
		std::size_t dropped = 0;
		const auto  edited = Rewrite(a_file, {}, "the reset", [&](std::string_view a_old) -> std::optional<std::string> {
			std::string text{ a_old };
			for (const auto* setting : a_keys) {
				auto without = IniText::WithoutKey(text, setting->section, setting->key);
				if (!without) {
					return std::nullopt;
				}
				if (*without != text) {
					++dropped;
					text = std::move(*without);
				}
			}
			return text;
		});
		if (edited == Edited::kFailed) {
			return std::nullopt;
		}
		return dropped;
	}

	bool ReadKept(const std::filesystem::path& a_file, std::string& a_text)
	{
		const std::scoped_lock l{ g_userFileLock };
		a_text.clear();
		std::error_code ec;
		const auto      exists = std::filesystem::exists(a_file, ec);
		return !ec && (!exists || ReadWhole(a_file, a_text));
	}

	void Report()
	{
		const auto base = Exists(BASE_FILE);
		const auto user = Exists(USER_FILE);

		// Where the reader stops early, see IniText.h.
		std::string text;
		const auto  stopped = user && ReadWhole(USER_FILE, text) && text.find('\0') != std::string::npos;
		REX::INFO("{:s}{:s}",
			base ? "NEC.ini read" : "No NEC.ini in Data\\F4SE\\Plugins\\NEC\\Config, so the defaults stand",
			!user ? ", and there is no NEC_custom.ini." : stopped ? ", and NEC_custom.ini on top of it up to its first NUL byte." : ", and NEC_custom.ini on top of it.");
		if (stopped) {
			REX::WARN("NEC_custom.ini holds a NUL byte, as a file saved as UTF-16 does, and nothing after that byte counts. Save it as UTF-8.");
		}
		Presets::Report();

		const auto level = sLogLevel.GetValue();
		if (!IsLogLevel(level)) {
			REX::WARN("sLogLevel \"{:s}\" is not a level, so the log is at info.", level);
		}
		ReportLine();
	}
}
