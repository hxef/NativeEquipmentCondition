#include "Core/ListFiles.h"

#include "Core/IniText.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ListFiles
{
	namespace
	{
		// -------------------------------------------------------------------
		// Reading the files
		// -------------------------------------------------------------------

		// Beside the Translations folder, and the game runs from its own
		// folder. Every name stays wide, since path::string() turns a name
		// into the system's code page and throws for a letter it lacks, such
		// as a Russian name on a Western PC.
		constexpr std::wstring_view FOLDERS = L"Data/F4SE/Plugins/NEC/";

		// A bigger file is no list.
		constexpr std::uintmax_t MOST_BYTES = 1024 * 1024;

		// The warnings the log writes for 1 file. The rest are only counted.
		constexpr std::size_t MOST_FAULTS = 10;

		// A line about a_list's files, into its own log or else NEC.log.
		void Warn(const List& a_list, const std::string& a_line)
		{
			if (a_list.say) {
				a_list.say(a_line);
			} else {
				REX::WARN("{:s}", a_line);
			}
		}

		void Info(const List& a_list, const std::string& a_line)
		{
			if (a_list.say) {
				a_list.say(a_line);
			} else {
				REX::INFO("{:s}", a_line);
			}
		}

		// The mark Notepad puts before a file saved as UTF-8, skipped.
		constexpr std::string_view BOM = "\xEF\xBB\xBF";

		// a_text in lower case, A to Z only, for a file name.
		std::wstring Lowered(std::wstring_view a_text)
		{
			std::wstring out{ a_text };
			for (auto& ch : out) {
				if (ch >= L'A' && ch <= L'Z') {
					ch = static_cast<wchar_t>(ch - L'A' + L'a');
				}
			}
			return out;
		}

		// A file in the folder.
		struct File
		{
			std::filesystem::path path;
			std::wstring          order;  // its name in lower case
			std::string           name;   // <folder>/<its name> in UTF-8, for the log
		};

		File FileOf(const List& a_list, const std::filesystem::path& a_path)
		{
			// A copy, since filename() returns a path that dies with this line.
			const std::wstring wide = a_path.filename().native();
			std::string text;
			if (!REX::UTF16_TO_UTF8(wide, text)) {
				text = "?";
			}
			return { a_path, Lowered(wide), std::format("{:s}{:c}{:s}", a_list.folder, a_list.say ? '\\' : '/', text) };
		}

		// The files in the folder in name order, in any case, and none when
		// there is no folder. Subfolders are not read.
		std::vector<File> Files(const List& a_list)
		{
			std::vector<File>     files;
			std::filesystem::path folder{ FOLDERS };
			folder += std::wstring{ a_list.folder.begin(), a_list.folder.end() };
			std::error_code ec;
			// A missing folder still sets ec, so it returns here with no line.
			const auto state = std::filesystem::status(folder, ec);
			if (state.type() == std::filesystem::file_type::not_found) {
				return files;
			}
			if (!ec && std::filesystem::is_directory(state)) {
				std::filesystem::directory_iterator it{ folder, ec };
				for (; !ec && it != std::filesystem::directory_iterator{}; it.increment(ec)) {
					if (it->is_regular_file(ec)) {
						files.push_back(FileOf(a_list, it->path()));
					}
				}
			}
			// The error's number, since its message comes in the system's
			// language and NEC.log is in English.
			if (ec) {
				Warn(a_list, std::format("{:s} could not be read (error {:d}), so NEC {:s}.", a_list.folder, ec.value(), a_list.unread));
				return {};
			}
			std::ranges::sort(files, {}, &File::order);
			return files;
		}

		// a_file's text without its byte order mark, or nothing once the log
		// says why it skips the file.
		std::optional<std::string> TextOf(const List& a_list, const File& a_file)
		{
			std::error_code ec;
			const auto      size = std::filesystem::file_size(a_file.path, ec);
			if (!ec && size > MOST_BYTES) {
				Warn(a_list, std::format("{:s} is bigger than 1 MB, so NEC skips it.", a_file.name));
				return std::nullopt;
			}
			std::string   bytes(ec ? 0 : static_cast<std::size_t>(size), '\0');
			std::ifstream in{ a_file.path, std::ios::binary };
			if (ec || !in || !in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
				Warn(a_list, std::format("{:s} could not be read, so NEC skips it.", a_file.name));
				return std::nullopt;
			}
			// A file saved as UTF-16 holds a NUL byte in every ASCII letter.
			if (bytes.find('\0') != std::string::npos) {
				Warn(a_list, std::format("{:s} is not plain text, as a file saved as UTF-16 is, so NEC skips it. Save it as UTF-8.", a_file.name));
				return std::nullopt;
			}
			if (bytes.starts_with(BOM)) {
				bytes.erase(0, BOM.size());
			}
			return bytes;
		}

		// What 1 line names, the plugin, the form ID and the word as written.
		struct Item
		{
			std::string_view plugin;
			std::uint32_t    id = 0;
			std::string_view word{};
		};

		bool IsComment(std::string_view a_text)
		{
			return a_text.empty() || a_text.front() == ';' || a_text.front() == '#';
		}

		// a_line as Plugin.esp|FormID, with 1 to 8 hex digits and 0x or not,
		// then "= word" when a_list asks for a word, or nothing for any other
		// line. Only a comment may follow, so a plugin name may hold ; or #.
		std::optional<Item> Parse(const List& a_list, std::string_view a_line)
		{
			const auto bar = a_line.find('|');
			if (bar == std::string_view::npos) {
				return std::nullopt;
			}
			auto id = IniText::Trimmed(a_line.substr(bar + 1));
			if (id.starts_with("0x") || id.starts_with("0X")) {
				id.remove_prefix(2);
			}
			const auto digits = id.substr(0, id.find_first_not_of("0123456789abcdefABCDEF"));
			auto       after = IniText::Trimmed(id.substr(digits.size()));
			Item       item{ IniText::Trimmed(a_line.substr(0, bar)) };
			if (!a_list.word.empty()) {
				if (!after.starts_with('=')) {
					return std::nullopt;
				}
				after.remove_prefix(1);
				const auto word = after.substr(0, after.find_first_of(";#"));
				item.word = IniText::Trimmed(word);
				after = after.substr(word.size());
			}
			if (item.plugin.empty() || digits.empty() || digits.size() > 8 || !IsComment(after) ||
				(!a_list.word.empty() && item.word.empty())) {
				return std::nullopt;
			}
			std::from_chars(digits.data(), digits.data() + digits.size(), item.id, 16);
			return item;
		}

		// -------------------------------------------------------------------
		// Finding the forms
		// -------------------------------------------------------------------

		// The form a line names, or nothing once a_faults says why there is
		// none. A plugin not in the load order adds its line to a_missing
		// instead.
		RE::TESForm* FormOf(const Item& a_item, const File& a_file, std::size_t a_line, Faults& a_faults,
			std::vector<std::pair<std::string_view, std::size_t>>& a_missing)
		{
			const auto* plugin = g_dataHandler->LookupModByName(a_item.plugin);
			if (!plugin || !plugin->IsActive()) {
				const auto same = [&a_item](const auto& a_entry) { return IniText::SameName(a_entry.first, a_item.plugin); };
				if (const auto at = std::ranges::find_if(a_missing, same); at != a_missing.end()) {
					at->second++;
				} else {
					a_missing.emplace_back(a_item.plugin, 1);
				}
				return nullptr;
			}

			// The top 2 digits are a load order slot and never count. A light
			// plugin's records end at FFF, and FE plus its light slot comes
			// before them.
			const auto light = plugin->IsLight();
			const auto local = light && (a_item.id >> 24) == 0xFE ? a_item.id & 0xFFF : a_item.id & 0xFFFFFF;
			if (light && local > 0xFFF) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is a light plugin, whose form IDs end at FFF, so NEC skips {:X}.",
					a_file.name, a_line, plugin->GetFilename(), local));
				return nullptr;
			}
			auto* form = RE::TESForm::GetFormByID(plugin->GetLoadOrderFormID(local));
			if (!form) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} has no form {:0{}X}, so NEC skips it.", a_file.name, a_line,
					plugin->GetFilename(), local, light ? 3 : 6));
			}
			return form;
		}

		// Every line of 1 file to a_take, then its summary.
		void ReadLines(const List& a_list, const File& a_file, std::string_view a_text,
			const std::function<void(const Line&, Faults&)>& a_take)
		{
			Faults                                                faults{ a_list };
			std::vector<std::pair<std::string_view, std::size_t>> missing;
			const auto                                            lines = IniText::Lines(a_text);
			for (std::size_t i = 0; i < lines.size(); i++) {
				const auto line = i + 1;
				const auto text = IniText::Trimmed(lines[i]);
				if (IsComment(text)) {
					continue;
				}
				const auto item = Parse(a_list, text);
				if (!item) {
					faults.Add(std::format("{:s} line {:d} is not Plugin.esp|FormID{:s}{:s}, so NEC skips it.", a_file.name, line,
						a_list.word.empty() ? "" : " = ", a_list.word));
					continue;
				}
				if (auto* form = FormOf(*item, a_file, line, faults, missing)) {
					a_take({ a_file.name, line, *form, item->word }, faults);
				}
			}

			if (faults.Count() > MOST_FAULTS) {
				Warn(a_list, std::format("{:s} has {:d} more faults, which the log leaves out.", a_file.name, faults.Count() - MOST_FAULTS));
			}
			for (const auto& [plugin, count] : missing) {
				Info(a_list, std::format("{:s}: {:s} is not in the load order, so its {:d} {:s} nothing.", a_file.name, plugin, count,
					count == 1 ? "line changes" : "lines change"));
			}
		}
	}

	void Faults::Add(const std::string& a_line)
	{
		if (_count++ < MOST_FAULTS) {
			Warn(_list, a_line);
		}
	}

	void Read(const List& a_list, const std::function<void(const Line&, Faults&)>& a_line,
		const std::function<void(std::string_view)>& a_done)
	{
		if (!g_dataHandler) {
			return;
		}
		for (const auto& file : Files(a_list)) {
			if (Lowered(file.path.extension().native()) != L".ini") {
				Info(a_list, std::format("{:s} is not an .ini file, so NEC does not read it.", file.name));
			} else if (const auto text = TextOf(a_list, file)) {
				ReadLines(a_list, file, *text, a_line);
				a_done(file.name);
			}
		}
	}
}
