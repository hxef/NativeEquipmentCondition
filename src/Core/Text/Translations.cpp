#include "Core/Text/Text.h"

#include "Core/IniText.h"
#include "Core/Text/FileText.h"
#include "Core/Text/Lines.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// The language files, F4SE/Plugins/NEC/Translations/<code>.ini, 1 KEY=text
// line per sentence. publish/Translations/en.ini explains the rules. A file is
// read the first time an sLanguage value needs it, once per value as written,
// and each line it cannot use stays English with a warning in NEC.log, so no
// text reaches std::vformat with a slot it cannot fill.
namespace Text
{
	namespace
	{
		// -------------------------------------------------------------------
		// Reading a file
		// -------------------------------------------------------------------

		// In a folder beside NEC.ini, and the game runs from its own folder.
		constexpr std::string_view FOLDER = "Data/F4SE/Plugins/NEC/Translations/";

		// A bigger file is no language file.
		constexpr std::uintmax_t MOST_BYTES = 1024 * 1024;

		// sLanguage names a file only with a to z and 0 to 9, up to 8, so it
		// can never point outside the folder.
		constexpr std::size_t MOST_CODE = 8;

		// NEC.log names this many faults of a file, then counts the rest.
		constexpr std::size_t MOST_FAULTS = 20;

		// What [Layout] may set, see Fit.cpp.
		constexpr std::size_t LINE_WIDTH_LOW = 10;
		constexpr std::size_t LINE_WIDTH_HIGH = 100;
		constexpr std::size_t ROW_LINES_LOW = 5;
		constexpr std::size_t ROW_LINES_HIGH = 20;

		using IniText::SameName;
		using IniText::Trimmed;

		// The whole file, or false when any of it cannot be read.
		bool ReadWhole(const std::string& a_file, std::uintmax_t a_size, std::string& a_bytes)
		{
			std::ifstream file{ a_file, std::ios::binary };
			a_bytes.resize(static_cast<std::size_t>(a_size));
			return file && file.read(a_bytes.data(), static_cast<std::streamsize>(a_size)) &&
			       file.gcount() == static_cast<std::streamsize>(a_size);
		}

		// The key a file names, in any case.
		std::optional<Key> KeyNamed(std::string_view a_name)
		{
			for (std::size_t i = 0; i < KEY_COUNT; i++) {
				if (SameName(ENGLISH[i].name, a_name)) {
					return Key{ static_cast<std::uint16_t>(i) };
				}
			}
			return std::nullopt;
		}

		// -------------------------------------------------------------------
		// A language's table
		// -------------------------------------------------------------------

		// 1 language's lines. Built once and never freed, and nothing in it
		// changes once it is shown, so a text Pick hands out stays good to
		// the end of the game, from any thread.
		struct Table
		{
			std::string                        setting;  // sLanguage as read, to see it change
			std::string                        code;     // lowercased
			std::array<const char*, KEY_COUNT> lines{};
			std::vector<std::string>           texts;    // the file's lines the array points into
			std::size_t                        lineWidth = 0;
			std::size_t                        rowLines = 0;
		};

		// What a file gave, before it becomes a table.
		struct Read
		{
			std::array<std::optional<std::string>, KEY_COUNT> given;
			std::array<std::size_t, KEY_COUNT>                where{};
			std::vector<std::string>                          faults;
		};

		// A [Layout] number, clamped, see Fit.cpp. An empty one sets nothing.
		void ReadLayout(std::string_view a_name, std::string_view a_value, std::size_t a_low, std::size_t a_high,
			std::size_t& a_into, std::string_view a_where, Read& a_read)
		{
			if (a_value.empty()) {
				return;
			}
			std::size_t number = 0;
			const auto [end, error] = std::from_chars(a_value.data(), a_value.data() + a_value.size(), number);
			if (error != std::errc{} || end != a_value.data() + a_value.size()) {
				a_read.faults.push_back(std::format("{:s}[Layout] {:s} {:s} is no whole number, so NEC skips it.", a_where, a_name, a_value));
				return;
			}
			a_into = std::clamp(number, a_low, a_high);
			if (a_into != number) {
				a_read.faults.push_back(std::format("{:s}[Layout] {:s} {:s} is {:s} than {:d}, so NEC takes {:d}.", a_where, a_name, a_value,
					number > a_high ? "more" : "less", a_into, a_into));
			}
		}

		// Every line of a file's text into a_table and a_read.
		void ReadLines(std::string_view a_text, std::string_view a_name, Table& a_table, Read& a_read)
		{
			std::string_view section;
			std::size_t      number = 0;
			for (std::size_t start = 0; start <= a_text.size();) {
				auto end = a_text.find('\n', start);
				end = end == std::string_view::npos ? a_text.size() : end;
				auto line = a_text.substr(start, end - start);
				start = end + 1;
				number++;
				line = Trimmed(line.ends_with('\r') ? line.substr(0, line.size() - 1) : line);
				if (line.empty() || line.front() == ';' || line.front() == '#') {
					continue;
				}
				const auto where = std::format("{:s} line {:d}: ", a_name, number);
				const auto equals = line.find('=');
				if (line.front() == '[' && line.find(']') != std::string_view::npos) {
					section = Trimmed(line.substr(1, line.find(']') - 1));
					continue;
				}
				if (line.front() == '[' || equals == std::string_view::npos || equals == 0) {
					a_read.faults.push_back(where + "this line is no KEY=text, [section] or ; comment, so NEC skips it.");
					continue;
				}
				const auto name = Trimmed(line.substr(0, equals));
				const auto text = Trimmed(line.substr(equals + 1));
				if (SameName(section, "Layout") && SameName(name, "LineWidth")) {
					ReadLayout("LineWidth", text, LINE_WIDTH_LOW, LINE_WIDTH_HIGH, a_table.lineWidth, where, a_read);
					continue;
				}
				if (SameName(section, "Layout") && SameName(name, "RowLines")) {
					ReadLayout("RowLines", text, ROW_LINES_LOW, ROW_LINES_HIGH, a_table.rowLines, where, a_read);
					continue;
				}
				const auto key = KeyNamed(name);
				if (!key) {
					a_read.faults.push_back(std::format("{:s}{:s} is no key of NEC {:d}.{:d}.{:d}, maybe one an older NEC had.", where, name,
						F4SE::GetPluginVersion().major(), F4SE::GetPluginVersion().minor(), F4SE::GetPluginVersion().patch()));
					continue;
				}
				const auto keyName = ENGLISH[key->index].name;
				auto&      given = a_read.given[key->index];
				if (const auto before = a_read.where[key->index]; before != 0) {
					a_read.faults.push_back(std::format("{:s}{:s} comes again, so line {:d} does not count.", where, keyName, before));
				}
				a_read.where[key->index] = number;
				given.reset();
				// An empty text is a line not translated yet.
				if (text.empty()) {
					continue;
				}
				if (const auto why = SlotFault(text, *key)) {
					a_read.faults.push_back(std::format("{:s}{:s} {:s}, so it stays English.", where, keyName, *why));
					continue;
				}
				given.emplace(text);
			}
		}

		// sLanguage as NEC.log names it: its code, or the value as read where
		// that differs.
		std::string Named(const Table& a_table)
		{
			if (a_table.setting.empty()) {
				return "empty";
			}
			return a_table.setting == a_table.code ? a_table.code : std::format("\"{:s}\"", a_table.setting);
		}

		// The file at a_file into a_table and a_read, or a fault saying why
		// none of it was read.
		void ReadFile(const std::string& a_name, const std::string& a_file, Table& a_table, Read& a_read)
		{
			std::error_code ec;
			std::string     bytes;
			const auto      size = std::filesystem::file_size(a_file, ec);
			if (!ec && size > MOST_BYTES) {
				a_read.faults.push_back(a_name + " is bigger than 1 MB, so NEC reads none of it.");
			} else if (ec || !ReadWhole(a_file, size, bytes)) {
				a_read.faults.push_back(a_name + " could not be read, so NEC reads none of it.");
			} else if (std::string why; const auto text = Decoded(bytes, why)) {
				ReadLines(*text, a_name, a_table, a_read);
			} else {
				a_read.faults.push_back(std::format("{:s} {:s}, so NEC reads none of it. Save it as UTF-8.", a_name, why));
			}
		}

		// How many lines a file gave, and how many of those differ from NEC's
		// own English.
		struct Counts
		{
			std::size_t gives = 0;
			std::size_t differ = 0;
		};

		// The lines a_read gave into a_table, each pointing into texts, which
		// never grows past what it reserves.
		Counts Fill(Table& a_table, Read& a_read)
		{
			Counts counts;
			a_table.texts.reserve(KEY_COUNT);
			for (std::size_t i = 0; i < KEY_COUNT; i++) {
				if (a_read.given[i]) {
					counts.gives++;
					counts.differ += *a_read.given[i] != ENGLISH[i].text;
					a_table.lines[i] = a_table.texts.emplace_back(std::move(*a_read.given[i])).c_str();
				}
			}
			return counts;
		}

		// What a file gave and every fault of it, in NEC.log.
		void Report(const std::string& a_name, const Table& a_table, const Read& a_read, Counts a_counts)
		{
			for (std::size_t i = 0; i < a_read.faults.size() && i < MOST_FAULTS; i++) {
				REX::WARN("{:s}", a_read.faults[i]);
			}
			if (a_read.faults.size() > MOST_FAULTS) {
				REX::WARN("{:s} has {:d} more faults, which NEC.log leaves out.", a_name, a_read.faults.size() - MOST_FAULTS);
			}

			const auto [gives, differ] = a_counts;
			auto line = std::format("Texts: sLanguage is {:s}. {:s} gives {:d} of {:d} lines", Named(a_table), a_name, gives, KEY_COUNT);
			if (gives < KEY_COUNT) {
				line += std::format(", the other {:d} {:s} English", KEY_COUNT - gives, KEY_COUNT - gives == 1 ? "stays" : "stay");
			}
			if (a_table.code == "en" && differ > 0) {
				const auto verb = differ == 1 ? "differs" : "differ";
				line += gives < KEY_COUNT ? std::format(", and {:d} of the {:d} {:s} from NEC's own English", differ, gives, verb) :
				                            std::format(", {:d} of them {:s} from NEC's own English", differ, verb);
			}
			REX::INFO("{:s}.", line);

			// Ready to paste into a file after a NEC update.
			for (std::size_t i = 0; i < KEY_COUNT; i++) {
				if (a_read.where[i] == 0 && gives > 0) {
					REX::DEBUG("{:s} lacks {:s}={:s}", a_name, ENGLISH[i].name, ENGLISH[i].text);
				}
			}
		}

		// The table for a_setting: the file of its code where there is one,
		// line by line over the English. Says what it found in NEC.log.
		const Table* Build(std::string_view a_setting)
		{
			auto* table = new Table{};
			table->setting = a_setting;
			for (std::size_t i = 0; i < KEY_COUNT; i++) {
				table->lines[i] = ENGLISH[i].text;
			}
			for (const auto ch : a_setting) {
				table->code += IniText::Lower(ch);
			}
			if (table->code.empty()) {
				table->code = "en";
			}
			if (table->code.size() > MOST_CODE || table->code.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") != std::string::npos) {
				REX::WARN("sLanguage \"{:s}\" is no language code (a to z and 0 to 9, up to 8), so NEC reads no file and speaks English.", a_setting);
				return table;
			}

			const auto      name = std::format("Translations/{:s}.ini", table->code);
			const auto      file = std::format("{:s}{:s}.ini", FOLDER, table->code);
			std::error_code ec;
			if (!std::filesystem::exists(file, ec)) {
				REX::INFO("Texts: sLanguage is {:s}, and there is no {:s}, so NEC speaks English.", Named(*table), name);
				return table;
			}
			Read read;
			ReadFile(name, file, *table, read);
			Report(name, *table, read, Fill(*table, read));
			return table;
		}

		// Set by Load. Before it every line is English.
		std::atomic<bool> g_ready{ false };

		// The table the last Pick used, the one to try first.
		std::atomic<const Table*> g_last{ nullptr };

		// Every table built, kept for a change back to an earlier sLanguage.
		// The lock lets 1 thread at a time look up or build one.
		std::mutex                g_lock;
		std::vector<const Table*> g_tables;

		// The table of sLanguage as it reads now, built the first time it is
		// asked for. Nothing before Load.
		const Table* TableNow()
		{
			if (!g_ready.load(std::memory_order_acquire)) {
				return nullptr;
			}
			// Read from the game's own setting at every call, so the page
			// follows a change from the console. Read by ID, so a call does
			// not search every INI setting by name. Copied, since a setini
			// from the console frees the game's own string while this may
			// still wait on the lock.
			static const REL::Relocation<RE::Setting*> setting{ RE::ID::Setting::sLanguage };
			const std::string                          value{ setting.get()->GetString() };
			if (const auto* last = g_last.load(std::memory_order_acquire); last && last->setting == value) {
				return last;
			}
			std::scoped_lock lock{ g_lock };
			const auto       it = std::ranges::find_if(g_tables, [&](const Table* a_table) { return a_table->setting == value; });
			const auto*      table = it != g_tables.end() ? *it : g_tables.emplace_back(Build(value));
			g_last.store(table, std::memory_order_release);
			return table;
		}
	}

	void Load()
	{
		g_ready.store(true, std::memory_order_release);
		static_cast<void>(TableNow());
	}

	const char* Pick(Key a_key)
	{
		const auto* table = TableNow();
		return table ? table->lines[a_key.index] : InEnglish(a_key);
	}

	Layout LayoutNow()
	{
		const auto* table = TableNow();
		return table ? Layout{ table->code, table->lineWidth, table->rowLines } : Layout{ "en" };
	}
}
