#include "Core/Presets/Shared.h"

#include "Core/IniText.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// After CommonLibF4, which stops a build that includes it before. Without its
// min and max macros, which would break std::min and std::max.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef ERROR

// The presets' 2 folders: what they hold, and NEC's own file calls in them.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// Names
		// -------------------------------------------------------------------

		// The mark Notepad puts before a file saved as UTF-8, skipped.
		constexpr std::string_view BOM = "\xEF\xBB\xBF";

		// The first lines NEC writes, see OwnTextHeld and NewText. A file with
		// neither is a changes file when a shipped preset has its name.
		constexpr std::string_view OWN_MARK = "; NEC preset:";
		constexpr std::string_view CHANGES_MARK = "; Your changes to the NEC preset ";

		// The folder NEC.log names files from, which holds NEC.ini,
		// NEC_custom.ini and MCM_Presets.
		constexpr std::wstring_view CONFIG_FOLDER = L"Data/F4SE/Plugins/NEC/Config/";

		// What the file name of each preset NEC ships starts with, which the
		// list leaves out.
		constexpr std::string_view SHIPPED_PREFIX = "NEC_";
		constexpr std::wstring_view SHIPPED_PREFIX_WIDE = L"NEC_";

		// Before 0 or after 0 in name order, in any case as Windows folds it,
		// and 0 for names that match in any case.
		int Folded(std::wstring_view a_lhs, std::wstring_view a_rhs)
		{
			const auto folded = ::CompareStringOrdinal(a_lhs.data(), static_cast<int>(a_lhs.size()), a_rhs.data(),
				static_cast<int>(a_rhs.size()), TRUE);
			return folded != CSTR_EQUAL && folded != 0 ? folded - CSTR_EQUAL : 0;
		}

		// Before 0 or after 0 in the list's order, for 2 file names without
		// .ini: by the name the list shows, in any case, then a name with
		// NEC_ first when a_shippedFirst and last when not, then the exact
		// name as the tie break, so the order is the same at every start.
		int Order(std::wstring_view a_lhs, std::wstring_view a_rhs, bool a_shippedFirst)
		{
			const auto lhs = Unprefixed(a_lhs);
			const auto rhs = Unprefixed(a_rhs);
			if (const auto shown = Folded(lhs, rhs); shown != 0) {
				return shown;
			}
			const bool lhsPrefixed = lhs.size() != a_lhs.size();
			const bool rhsPrefixed = rhs.size() != a_rhs.size();
			if (lhsPrefixed != rhsPrefixed) {
				return lhsPrefixed == a_shippedFirst ? -1 : 1;
			}
			return a_lhs.compare(a_rhs);
		}

		std::wstring WideStem(const std::filesystem::path& a_file)
		{
			return a_file.stem().native();
		}

		// Whether a_lhs comes before a_rhs in the list's order, with the whole
		// file name as the last tie break, for 2 names that differ only in the
		// case of .ini.
		bool ListsBefore(const std::filesystem::path& a_lhs, const std::filesystem::path& a_rhs, bool a_shippedFirst)
		{
			const auto order = Order(WideStem(a_lhs), WideStem(a_rhs), a_shippedFirst);
			return (order != 0 ? order : a_lhs.filename().native().compare(a_rhs.filename().native())) < 0;
		}

		// Whether 2 files list under 1 name, with NEC_ or without, in any
		// case, such as NEC_Casual.ini and Casual.ini.
		bool ListedAlike(const std::filesystem::path& a_lhs, const std::filesystem::path& a_rhs)
		{
			return SameFile(Unprefixed(WideStem(a_lhs)), Unprefixed(WideStem(a_rhs)));
		}

		bool StartsWith(std::string_view a_text, std::string_view a_start)
		{
			return a_text.size() >= a_start.size() && IniText::SameName(a_text.substr(0, a_start.size()), a_start);
		}

		// -------------------------------------------------------------------
		// 1 folder
		// -------------------------------------------------------------------

		// The .ini files of a_folder in the list's order, without subfolders,
		// and none when there is no folder. a_label is how NEC.log names the
		// folder. Of 2 files that list under 1 name, the 1 with NEC_ stays
		// when a_shippedFirst, so a download never hides a preset NEC ships,
		// and the 1 without NEC_ when not, so a player's own file wins. The
		// other goes into a_out's alike.
		std::vector<std::filesystem::path> Listed(std::wstring_view a_folder, std::string_view a_label, bool a_shippedFirst, Scanned& a_out)
		{
			std::vector<std::filesystem::path> files;
			const std::filesystem::path folder{ a_folder };
			std::error_code ec;
			// A missing folder still sets ec, so it returns here with no line.
			const auto state = std::filesystem::status(folder, ec);
			if (state.type() == std::filesystem::file_type::not_found) {
				return files;
			}
			if (!ec && std::filesystem::is_directory(state)) {
				std::filesystem::directory_iterator it{ folder, ec };
				for (; !ec && it != std::filesystem::directory_iterator{}; it.increment(ec)) {
					std::error_code kind;
					if (it->is_regular_file(kind) && SameFile(it->path().extension().native(), L".ini")) {
						files.push_back(it->path());
					}
				}
			}
			// The error's number, since its message comes in the system's
			// language and NEC.log is in English.
			if (ec) {
				a_out.lines.push_back({ true, std::format("{:s} could not be read (error {:d}), so its presets are not in the list.", a_label, ec.value()) });
				return {};
			}
			std::ranges::sort(files, [&](const auto& a_lhs, const auto& a_rhs) { return ListsBefore(a_lhs, a_rhs, a_shippedFirst); });

			// Default.ini and NEC_Default.ini, a file over 1 MB and the second
			// of 2 files that list under 1 name go, each with its line. The
			// order puts such files side by side, the 1 that stays first.
			std::vector<std::filesystem::path> kept;
			for (const auto& file : files) {
				const auto name = LogName(file);
				if (SameFile(Unprefixed(WideStem(file)), L"Default")) {
					a_out.lines.push_back({ true, std::format("{:s} is not read, since Default is NEC.ini's own values. Rename it to use it.", name) });
					continue;
				}
				const auto size = std::filesystem::file_size(file, ec);
				if (!ec && size > MOST_BYTES) {
					a_out.lines.push_back({ true, std::format("{:s} is bigger than 1 MB, so NEC skips it.", name) });
					continue;
				}
				if (!kept.empty() && SameFile(WideStem(kept.back()), WideStem(file))) {
					a_out.lines.push_back({ true, std::format("{:s} has the name of {:s} in another case, so NEC reads only the first.", name, LogName(kept.back())) });
					continue;
				}
				if (!kept.empty() && ListedAlike(kept.back(), file)) {
					auto line = std::format("{:s} and {:s} both list as {:s}, so NEC reads only {:s}.", name, LogName(kept.back()), Shown(StemOf(file)),
						LogName(kept.back()));
					if (a_shippedFirst) {
						line += std::format(" Rename {:s} to see both.", name);
					}
					a_out.lines.push_back({ true, std::move(line) });
					a_out.alike.push_back(file);
					continue;
				}
				kept.push_back(file);
			}
			return kept;
		}

		// What a Custom file's first line that is not empty says it is: an
		// own preset, a changes file, or nothing for a file NEC did not write.
		std::optional<Kind> Marked(const std::filesystem::path& a_file)
		{
			std::string text;
			if (!Settings::ReadKept(a_file, text)) {
				return std::nullopt;
			}
			std::string_view rest{ text };
			if (rest.starts_with(BOM)) {
				rest.remove_prefix(BOM.size());
			}
			for (const auto line : IniText::Lines(rest)) {
				const auto first = IniText::Trimmed(line);
				if (first.empty()) {
					continue;
				}
				if (StartsWith(first, OWN_MARK)) {
					return Kind::kOwn;
				}
				if (StartsWith(first, CHANGES_MARK)) {
					return Kind::kOriginal;
				}
				return std::nullopt;
			}
			return std::nullopt;
		}

		// The file of a_files with a_like's name, in any case, as a shipped
		// preset's changes file has.
		const std::filesystem::path* Matching(const std::vector<std::filesystem::path>& a_files, const std::filesystem::path& a_like)
		{
			const auto it = std::ranges::find_if(a_files, [&](const auto& a_file) { return SameFile(WideStem(a_file), WideStem(a_like)); });
			return it != a_files.end() ? &*it : nullptr;
		}

		// The file of a_files that lists under a_like's name, with NEC_ or
		// without.
		const std::filesystem::path* Alike(const std::vector<std::filesystem::path>& a_files, const std::filesystem::path& a_like)
		{
			const auto it = std::ranges::find_if(a_files, [&](const auto& a_file) { return ListedAlike(a_file, a_like); });
			return it != a_files.end() ? &*it : nullptr;
		}
	}

	// -------------------------------------------------------------------
	// Names
	// -------------------------------------------------------------------

	bool SameFile(std::wstring_view a_lhs, std::wstring_view a_rhs)
	{
		return ::CompareStringOrdinal(a_lhs.data(), static_cast<int>(a_lhs.size()), a_rhs.data(), static_cast<int>(a_rhs.size()), TRUE) == CSTR_EQUAL;
	}

	std::filesystem::path PathOf(std::wstring_view a_folder, std::string_view a_name)
	{
		std::filesystem::path file{ a_folder };
		std::wstring wide;
		if (REX::UTF8_TO_UTF16(a_name, wide)) {
			file += wide;
		}
		return file;
	}

	std::string StemOf(const std::filesystem::path& a_file)
	{
		std::string text;
		return REX::UTF16_TO_UTF8(WideStem(a_file), text) ? text : "?";
	}

	std::string_view Unprefixed(std::string_view a_stem)
	{
		return a_stem.size() > SHIPPED_PREFIX.size() && a_stem.starts_with(SHIPPED_PREFIX) ? a_stem.substr(SHIPPED_PREFIX.size()) : a_stem;
	}

	std::wstring_view Unprefixed(std::wstring_view a_stem)
	{
		return a_stem.size() > SHIPPED_PREFIX_WIDE.size() && a_stem.starts_with(SHIPPED_PREFIX_WIDE) ? a_stem.substr(SHIPPED_PREFIX_WIDE.size()) :
		                                                                                                a_stem;
	}

	std::string Shown(std::string_view a_stem)
	{
		std::string name{ Unprefixed(a_stem) };
		std::ranges::replace(name, '_', ' ');
		return name;
	}

	std::string LogName(const std::filesystem::path& a_file)
	{
		std::wstring_view wide{ a_file.native() };
		if (wide.size() > CONFIG_FOLDER.size() && SameFile(wide.substr(0, CONFIG_FOLDER.size()), CONFIG_FOLDER)) {
			wide.remove_prefix(CONFIG_FOLDER.size());
		}
		std::string text;
		if (!REX::UTF16_TO_UTF8(wide, text)) {
			return "?";
		}
		std::ranges::replace(text, '\\', '/');
		return text;
	}

	// -------------------------------------------------------------------
	// Both folders
	// -------------------------------------------------------------------

	Scanned Scan()
	{
		Scanned out;
		// Subfolders are not read, so MCM_Presets never lists Custom's files.
		const auto originals = Listed(ORIGINALS, "MCM_Presets", true, out);
		const auto custom = Listed(CUSTOM, "MCM_Presets/Custom", false, out);
		for (const auto* folder : { &originals, &custom }) {
			for (const auto& file : *folder) {
				out.names.push_back(WideStem(file));
			}
		}

		// A Custom file is an own preset or the changes to the shipped preset
		// of its name, as its first line says, else by its name.
		std::vector<std::filesystem::path> own;
		std::vector<std::filesystem::path> changes;
		for (const auto& file : custom) {
			const auto marked = Marked(file);
			const auto kind = marked ? *marked : Matching(originals, file) ? Kind::kOriginal :
			                                                                 Kind::kOwn;
			(kind == Kind::kOwn ? own : changes).push_back(file);
		}

		for (const auto& file : originals) {
			// The player's own preset keeps its name, and the shipped one
			// is left out while they list under 1 name.
			if (const auto* mine = Alike(own, file)) {
				out.lines.push_back({ true, std::format("Your own preset {:s} in MCM_Presets/Custom has the name of {:s}, so that file is not in the list. Rename one of them to see both.",
												Shown(StemOf(*mine)), LogName(file)) });
				out.alike.push_back(file);
				continue;
			}
			const auto* changed = Matching(changes, file);
			out.entries.push_back({ .name = Shown(StemOf(file)),
				.stem = StemOf(file),
				.kind = Kind::kOriginal,
				.file = file,
				.changes = changed ? *changed : std::filesystem::path{ CUSTOM } / file.filename() });
		}
		for (const auto& file : own) {
			out.entries.push_back({ .name = Shown(StemOf(file)), .stem = StemOf(file), .kind = Kind::kOwn, .file = file, .changes = {} });
		}
		for (const auto& file : changes) {
			if (!Matching(originals, file)) {
				const auto name = Shown(StemOf(file));
				// Only a file of MCM_Presets can match here, as a file Custom
				// leaves out never has the name of a file Custom keeps.
				if (const auto* left = Matching(out.alike, file)) {
					out.lines.push_back({ true, std::format("{:s} holds your changes to {:s}, but {:s} is left out of the list, since another file lists under its name, so NEC keeps the file.",
													LogName(file), name, LogName(*left)) });
				} else {
					out.lines.push_back({ true, std::format("{:s} holds your changes to {:s}, but MCM_Presets/{:s}.ini is not there, so NEC keeps the file and leaves {:s} out of the list. A preset you download goes in MCM_Presets.",
													LogName(file), name, StemOf(file), name) });
				}
				out.orphans.push_back(file);
			}
		}
		std::ranges::sort(out.entries, Before);
		return out;
	}

	bool Before(const Entry& a_lhs, const Entry& a_rhs)
	{
		// No 2 entries are ListedAlike, so the NEC_ step of the order never
		// decides here.
		return ListsBefore(a_lhs.file, a_rhs.file, false);
	}

	// -------------------------------------------------------------------
	// Writing and removing
	// -------------------------------------------------------------------

	bool Remove(const std::filesystem::path& a_file, std::string& a_reason)
	{
		// A file already gone, such as one removed by hand, counts as removed.
		if (const auto error = ::DeleteFileW(a_file.c_str()) ? ERROR_SUCCESS : ::GetLastError();
			error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
			a_reason = std::format("error {:d}", error);
			return false;
		}
		// Mod Organizer 2 may report a file in a mod as removed and still
		// show it, so a second look decides.
		std::error_code ec;
		if (std::filesystem::exists(a_file, ec) || ec) {
			a_reason = "it is still there";
			return false;
		}
		return true;
	}

	bool WriteNew(const std::filesystem::path& a_file, std::string_view a_text, std::string& a_reason)
	{
		std::error_code ec;
		std::filesystem::create_directories(a_file.parent_path(), ec);
		auto fresh = a_file;
		fresh += L".new";
		{
			std::ofstream out{ fresh, std::ios::binary | std::ios::trunc };
			out.write(a_text.data(), static_cast<std::streamsize>(a_text.size()));
			out.close();
			// Without MOVEFILE_REPLACE_EXISTING the move fails on a file that
			// is there, so a save never writes over one.
			if (out.fail()) {
				a_reason = "its text could not be written";
			} else if (::MoveFileExW(fresh.c_str(), a_file.c_str(), 0)) {
				return true;
			} else {
				a_reason = std::format("error {:d}", ::GetLastError());
			}
		}
		std::filesystem::remove(fresh, ec);
		return false;
	}
}
