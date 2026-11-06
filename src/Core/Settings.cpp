#include "Core/Settings.h"

#include "Core/IniText.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>

// Reading NEC.ini and NEC_custom.ini, and keeping a change from the MCM page
// in NEC_custom.ini. The Settings line and the lists of switches and numbers
// are in SettingsLine.cpp.
namespace Settings
{
	namespace
	{
		// -------------------------------------------------------------------
		// The 2 files
		// -------------------------------------------------------------------

		// Both files sit beside the DLL, and the game runs from its own folder.
		// Literals, since a store keeps only a view of what it is handed.
		constexpr const char* BASE_FILE = "Data/F4SE/Plugins/NEC.ini";
		constexpr const char* USER_FILE = "Data/F4SE/Plugins/NEC_custom.ini";

		// Keep and Drop write this first and rename it over USER_FILE, so a
		// write cut short never leaves the player's file half written.
		constexpr const char* NEW_FILE = "Data/F4SE/Plugins/NEC_custom.ini.new";

		// Changes from the page arrive one after another, and each reads the
		// file the last one wrote. Keep and Drop share this one lock.
		std::mutex g_userFileLock;

		bool Exists(const char* a_file)
		{
			std::error_code ec;
			return std::filesystem::exists(a_file, ec);
		}

		// The whole file, or false when any of it cannot be read or it
		// changes size while it is read.
		bool ReadWhole(const char* a_file, std::string& a_text)
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

		bool WriteWhole(const char* a_file, std::string_view a_text)
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

		// What an edit of NEC_custom.ini came to.
		enum class Edited
		{
			kFailed,
			kSame,  // the edit left the text as it was, so nothing is written
			kWritten,
		};

		// Edits NEC_custom.ini through a_edit, which is handed the whole text
		// and gives it back changed, or nothing when the game would not read
		// the edit.
		template <class Edit>
		Edited Rewrite(std::string_view a_key, Edit a_edit)
		{
			const std::scoped_lock l{ g_userFileLock };

			// A file that cannot be read whole is never written over, or the
			// player's other keys would be lost. With no file the text is
			// empty.
			std::error_code ec;
			const auto      exists = std::filesystem::exists(USER_FILE, ec);
			std::string     text;
			if (ec || (exists && !ReadWhole(USER_FILE, text))) {
				REX::WARN("NEC_custom.ini could not be read, so the change to {:s} is not saved in it.", a_key);
				return Edited::kFailed;
			}

			const std::optional<std::string> edited = a_edit(std::string_view{ text });
			if (!edited) {
				REX::WARN("NEC_custom.ini is not plain text or has a line that is only [, so the change to {:s} is not saved in it.", a_key);
				return Edited::kFailed;
			}
			if (*edited == text) {
				return Edited::kSame;
			}
			if (!WriteWhole(NEW_FILE, *edited)) {
				REX::WARN("NEC_custom.ini.new could not be written, so the change to {:s} is not saved in NEC_custom.ini.", a_key);
				std::filesystem::remove(NEW_FILE, ec);
				return Edited::kFailed;
			}
			std::filesystem::rename(NEW_FILE, USER_FILE, ec);
			if (ec) {
				REX::WARN("NEC_custom.ini could not be replaced, so the change to {:s} is not saved in it: {:s}", a_key, ec.message());
				std::filesystem::remove(NEW_FILE, ec);
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
	}

	void LogChange(std::string_view a_key, std::string_view a_old, std::string_view a_new, std::string_view a_from)
	{
		Say(std::format("{:s}: {:s} to {:s}, {:s}.", a_key, a_old, a_new, a_from));
	}

	bool Keep(std::string_view a_section, std::string_view a_key, std::string_view a_text)
	{
		return Rewrite(a_key, [&](std::string_view a_file) {
			return IniText::WithKey(a_file, a_section, a_key, a_text);
		}) != Edited::kFailed;
	}

	bool Drop(std::string_view a_section, std::string_view a_key)
	{
		const auto edited = Rewrite(a_key, [&](std::string_view a_file) {
			return IniText::WithoutKey(a_file, a_section, a_key);
		});
		if (edited == Edited::kWritten) {
			Say(std::format("NEC_custom.ini no longer sets {:s}, so NEC.ini's value stands.", a_key));
		}
		return edited != Edited::kFailed;
	}

	void Report()
	{
		const auto base = Exists(BASE_FILE);
		const auto user = Exists(USER_FILE);

		// Where the reader stops early, see IniText.h.
		std::string text;
		const auto  stopped = user && ReadWhole(USER_FILE, text) && text.find('\0') != std::string::npos;
		REX::INFO("{:s}{:s}",
			base ? "NEC.ini read" : "No NEC.ini beside the DLL, so the defaults stand",
			!user ? ", and there is no NEC_custom.ini." : stopped ? ", and NEC_custom.ini on top of it up to its first NUL byte." : ", and NEC_custom.ini on top of it.");
		if (stopped) {
			REX::WARN("NEC_custom.ini holds a NUL byte, as a file saved as UTF-16 does, and nothing after that byte counts. Save it as UTF-8.");
		}

		const auto level = sLogLevel.GetValue();
		if (!IsLogLevel(level)) {
			REX::WARN("sLogLevel \"{:s}\" is not a level, so the log is at info.", level);
		}
		ReportLine();
	}
}
