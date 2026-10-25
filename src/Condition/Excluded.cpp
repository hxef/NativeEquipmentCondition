#include "Condition/Excluded.h"

#include "Condition/Condition.h"
#include "Core/IniText.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Excluded
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
		constexpr std::wstring_view FOLDER = L"Data/F4SE/Plugins/NEC/Exclude";

		// A bigger file is no list.
		constexpr std::uintmax_t MOST_BYTES = 1024 * 1024;

		// The warnings NEC.log writes for 1 file. The rest are only counted.
		constexpr std::size_t MOST_FAULTS = 10;

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
			std::string           name;   // Exclude/<its name> in UTF-8, for NEC.log
		};

		File FileOf(const std::filesystem::path& a_path)
		{
			// A copy, since filename() returns a path that dies with this line.
			const std::wstring wide = a_path.filename().native();
			std::string text;
			if (!REX::UTF16_TO_UTF8(wide, text)) {
				text = "?";
			}
			return { a_path, Lowered(wide), "Exclude/" + text };
		}

		// The files in the folder in name order, in any case, and none when
		// there is no folder. Subfolders are not read.
		std::vector<File> Files()
		{
			std::vector<File>           files;
			const std::filesystem::path folder{ FOLDER };
			std::error_code             ec;
			// A missing folder still sets ec, so it returns here with no line.
			const auto state = std::filesystem::status(folder, ec);
			if (state.type() == std::filesystem::file_type::not_found) {
				return files;
			}
			if (!ec && std::filesystem::is_directory(state)) {
				std::filesystem::directory_iterator it{ folder, ec };
				for (; !ec && it != std::filesystem::directory_iterator{}; it.increment(ec)) {
					if (it->is_regular_file(ec)) {
						files.push_back(FileOf(it->path()));
					}
				}
			}
			// The error's number, since its message comes in the system's
			// language and NEC.log is in English.
			if (ec) {
				REX::WARN("Exclude could not be read (error {:d}), so NEC leaves no item alone.", ec.value());
				return {};
			}
			std::ranges::sort(files, {}, &File::order);
			return files;
		}

		// a_file's text without its byte order mark, or nothing once NEC.log
		// says why it skips the file.
		std::optional<std::string> TextOf(const File& a_file)
		{
			std::error_code ec;
			const auto      size = std::filesystem::file_size(a_file.path, ec);
			if (!ec && size > MOST_BYTES) {
				REX::WARN("{:s} is bigger than 1 MB, so NEC skips it.", a_file.name);
				return std::nullopt;
			}
			std::string   bytes(ec ? 0 : static_cast<std::size_t>(size), '\0');
			std::ifstream in{ a_file.path, std::ios::binary };
			if (ec || !in || !in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
				REX::WARN("{:s} could not be read, so NEC skips it.", a_file.name);
				return std::nullopt;
			}
			// A file saved as UTF-16 holds a NUL byte in every ASCII letter.
			if (bytes.find('\0') != std::string::npos) {
				REX::WARN("{:s} is not plain text, as a file saved as UTF-16 is, so NEC skips it. Save it as UTF-8.", a_file.name);
				return std::nullopt;
			}
			if (bytes.starts_with(BOM)) {
				bytes.erase(0, BOM.size());
			}
			return bytes;
		}

		// What 1 line names, the plugin and the form ID as written.
		struct Item
		{
			std::string_view plugin;
			std::uint32_t    id = 0;
		};

		// a_line as Plugin.esp|FormID, with 1 to 8 hex digits and 0x or not,
		// or nothing for any other line. Only a comment may follow the ID, so
		// a plugin name may hold ; or #.
		std::optional<Item> Parse(std::string_view a_line)
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
			const auto after = IniText::Trimmed(id.substr(digits.size()));
			Item item{ IniText::Trimmed(a_line.substr(0, bar)) };
			if (item.plugin.empty() || digits.empty() || digits.size() > 8 || !(after.empty() || after.front() == ';' || after.front() == '#')) {
				return std::nullopt;
			}
			std::from_chars(digits.data(), digits.data() + digits.size(), item.id, 16);
			return item;
		}

		// -------------------------------------------------------------------
		// Finding the forms
		// -------------------------------------------------------------------

		// A file's warnings, the first MOST_FAULTS of them written.
		struct Faults
		{
			std::size_t count = 0;

			void Add(const std::string& a_line)
			{
				if (count++ < MOST_FAULTS) {
					REX::WARN("{:s}", a_line);
				}
			}
		};

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

		// Every line of 1 file into a_ids, each said in NEC.log.
		void ReadLines(const File& a_file, std::string_view a_text, std::vector<RE::TESFormID>& a_ids)
		{
			Faults                                                faults;
			std::vector<RE::TESFormID>                            mine;
			std::vector<std::pair<std::string_view, std::size_t>> missing;
			const auto                                            lines = IniText::Lines(a_text);
			for (std::size_t i = 0; i < lines.size(); i++) {
				const auto line = i + 1;
				const auto text = IniText::Trimmed(lines[i]);
				if (text.empty() || text.front() == ';' || text.front() == '#') {
					continue;
				}
				const auto item = Parse(text);
				if (!item) {
					faults.Add(std::format("{:s} line {:d} is not Plugin.esp|FormID, so NEC skips it.", a_file.name, line));
					continue;
				}
				const auto* form = FormOf(*item, a_file, line, faults, missing);
				if (!form) {
					continue;
				}
				const auto id = form->formID;
				const auto name = RE::TESFullName::GetFullName(*form);
				if (!form->IsWeapon() && !form->Is(RE::ENUM_FORM_ID::kARMO)) {
					REX::INFO("{:s} line {:d}: {:s} [{:08X}] is not a weapon or armor, so NEC skips it.", a_file.name, line, name, id);
					continue;
				}

				// Kept even when the kind leaves it out today, since armor's
				// rule is measured again at every save load.
				a_ids.push_back(id);
				if (const auto* why = Condition::KindWhyNoCondition(static_cast<const RE::TESBoundObject&>(*form))) {
					REX::INFO("{:s} line {:d}: {:s} [{:08X}] takes no condition anyway, {:s}.", a_file.name, line, name, id, why);
				} else if (std::ranges::find(mine, id) == mine.end()) {
					mine.push_back(id);
					REX::DEBUG("{:s} line {:d}: {:s} [{:08X}] left alone.", a_file.name, line, name, id);
				}
			}

			if (faults.count > MOST_FAULTS) {
				REX::WARN("{:s} has {:d} more faults, which NEC.log leaves out.", a_file.name, faults.count - MOST_FAULTS);
			}
			for (const auto& [plugin, count] : missing) {
				REX::INFO("{:s}: {:s} is not in the load order, so its {:d} {:s} nothing.", a_file.name, plugin, count,
					count == 1 ? "line changes" : "lines change");
			}
			REX::INFO("{:s}: NEC leaves {:d} {:s} alone.", a_file.name, mine.size(), mine.size() == 1 ? "item" : "items");
		}

		// -------------------------------------------------------------------
		// The list
		// -------------------------------------------------------------------

		// Sorted form IDs, read from other threads, and whether it holds any.
		Snapshot<std::vector<RE::TESFormID>> g_list;
		std::atomic<bool>                    g_any{ false };
	}

	void Load()
	{
		std::vector<RE::TESFormID> ids;
		const auto                 files = g_dataHandler ? Files() : std::vector<File>{};
		for (const auto& file : files) {
			if (Lowered(file.path.extension().native()) != L".ini") {
				REX::INFO("{:s} is not an .ini file, so NEC does not read it.", file.name);
			} else if (const auto text = TextOf(file)) {
				ReadLines(file, *text, ids);
			}
		}

		std::ranges::sort(ids);
		const auto [first, last] = std::ranges::unique(ids);
		ids.erase(first, last);
		const auto any = !ids.empty();
		g_list.Publish(any ? std::make_shared<const std::vector<RE::TESFormID>>(std::move(ids)) : nullptr);
		g_any.store(any, std::memory_order_relaxed);
	}

	void Unload()
	{
		g_any.store(false, std::memory_order_relaxed);
		g_list.Publish(nullptr);
	}

	bool Any()
	{
		return g_any.load(std::memory_order_relaxed);
	}

	bool Has(const RE::TESForm& a_form)
	{
		if (!Any()) {
			return false;
		}
		const auto list = g_list.Get();
		return list && std::ranges::binary_search(*list, a_form.formID);
	}
}
