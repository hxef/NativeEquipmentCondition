#include "Condition/WearList/WearList.h"

#include "Condition/Condition.h"
#include "Condition/WearList/Table.h"
#include "Core/IniText.h"
#include "Core/ListFiles.h"
#include "Core/Snapshot.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace WearList
{
	namespace
	{
		// -------------------------------------------------------------------
		// The lines
		// -------------------------------------------------------------------

		constexpr ListFiles::List LIST{ .folder = "Wear", .unread = "uses no wear list", .word = "wear or damage" };

		// What a line names, as the logs say it.
		enum class Kind
		{
			kWeapon,
			kMod,
			kKeyword,
		};

		constexpr std::size_t      KINDS = 3;
		constexpr std::string_view KIND_NAMES[KINDS]{ "weapon", "mod", "keyword" };

		// The 2 numbers 1 line sets, nothing for one it leaves out.
		struct Numbers
		{
			std::optional<float> wear;
			std::optional<float> damage;
		};

		// What the files set so far, and the forms the file being read set,
		// for its summary.
		struct Reading
		{
			Table                                         table;
			std::array<std::vector<RE::TESFormID>, KINDS> mine;
		};

		// "Vault 111 Jumpsuit [0001EED7]", or "[0001EED7]" for a form with no
		// name.
		std::string NameOf(const RE::TESForm& a_form)
		{
			const std::string_view name = RE::TESFullName::GetFullName(a_form);
			return name.empty() ? std::format("[{:08X}]", a_form.formID) : std::format("{:s} [{:08X}]", name, a_form.formID);
		}

		// "the keyword WeaponTypeMelee1H [0004A0A4]", as the debug and set
		// again lines name a form. A keyword goes by its editor ID, which the
		// game keeps for keywords, and the rest by name.
		std::string WhoOf(Kind a_kind, const RE::TESForm& a_form)
		{
			if (a_kind == Kind::kKeyword) {
				const auto&            keyword = static_cast<const RE::BGSKeyword&>(a_form);
				const std::string_view id = keyword.formEditorID.c_str();
				return id.empty() ? std::format("the keyword [{:08X}]", a_form.formID) :
				                    std::format("the keyword {:s} [{:08X}]", id, a_form.formID);
			}
			return std::format("the {:s} {:s}", KIND_NAMES[std::to_underlying(a_kind)], NameOf(a_form));
		}


		// The next word of a_text, split at spaces and tabs, and a_text past
		// it. Empty at the end.
		std::string_view NextWord(std::string_view& a_text)
		{
			a_text = IniText::Trimmed(a_text);
			const auto gap = a_text.find_first_of(" \t");
			const auto word = a_text.substr(0, gap);
			a_text = gap == std::string_view::npos ? std::string_view{} : a_text.substr(gap);
			return word;
		}

		// The numbers of "wear 1.5 damage 0.3", or nothing once a_faults says
		// what is wrong with them. A number is written with a point, and 1
		// fault skips the whole line.
		std::optional<Numbers> Parse(const ListFiles::Line& a_line, ListFiles::Faults& a_faults)
		{
			const auto skip = [&](const std::string& a_why) {
				a_faults.Add(std::format("{:s} line {:d}{:s}, so NEC skips it.", a_line.file, a_line.number, a_why));
				return std::nullopt;
			};
			Numbers numbers;
			auto    rest = a_line.word;
			for (auto key = NextWord(rest); !key.empty(); key = NextWord(rest)) {
				const auto wear = IniText::SameName(key, "wear");
				if (!wear && !IniText::SameName(key, "damage")) {
					return skip(std::format(": {:s} is not wear or damage", key));
				}
				auto& slot = wear ? numbers.wear : numbers.damage;
				if (slot) {
					return skip(std::format(" sets {:s} twice", wear ? "wear" : "damage"));
				}

				// from_chars reads no comma, so 0,5 stops at the 0 and fails.
				const auto  text = NextWord(rest);
				const auto* last = text.data() + text.size();
				float       value = 0.0F;
				if (text.empty() || std::from_chars(text.data(), last, value) != std::from_chars_result{ last, std::errc{} }) {
					return skip(std::format(": {:s} needs a number after it, such as {:s} {:s}", key, key, wear ? "1.5" : "0.5"));
				}
				// Written so that inf and nan fail too.
				if (!(value >= 0.0F && value <= (wear ? MOST_WEAR : 1.0F))) {
					return skip(std::format(": {:s} {:s} is not 0 to {:s}", key, text, wear ? "10" : "1"));
				}
				slot = value + 0.0F;  // + 0 turns -0 into 0, so the log prints no minus
			}
			return numbers;
		}

		// a_kind's entry for a_form, made empty the first time.
		Entry& EntryOf(Table& a_table, Kind a_kind, const RE::TESForm& a_form)
		{
			if (a_kind != Kind::kKeyword) {
				return (a_kind == Kind::kWeapon ? a_table.weapons : a_table.mods)[a_form.formID];
			}
			const auto* keyword = static_cast<const RE::BGSKeyword*>(&a_form);
			auto&       keywords = a_table.keywords;
			const auto  at = std::ranges::find(keywords, keyword, &std::pair<const RE::BGSKeyword*, Entry>::first);
			return at != keywords.end() ? at->second : keywords.emplace_back(keyword, Entry{}).second;
		}

		// 1 line's numbers into a_reading, said in NEC.log. A number set
		// before on the same form keeps its first line, and the line's other
		// number still counts.
		void Keep(Reading& a_reading, Kind a_kind, const RE::TESForm& a_form, const Numbers& a_numbers, const ListFiles::Line& a_line,
			ListFiles::Faults& a_faults)
		{
			auto& files = a_reading.table.files;
			if (files.empty() || files.back() != a_line.file) {
				files.emplace_back(a_line.file);
			}
			const Set  here{ .file = static_cast<std::uint32_t>(files.size() - 1), .line = static_cast<std::uint32_t>(a_line.number) };
			const auto who = WhoOf(a_kind, a_form);
			auto&      entry = EntryOf(a_reading.table, a_kind, a_form);

			// Whether the line's number of 1 kind goes in, said when it is
			// set again.
			const auto put = [&](std::optional<Set>& a_slot, const std::optional<float>& a_value, bool a_wear) {
				if (a_value && a_slot) {
					a_faults.Add(std::format("{:s} line {:d}: {:s} has {:s} set again, so NEC keeps {:s}{:.2f} from {:s}.", a_line.file,
						a_line.number, who, a_wear ? "wear" : "damage", a_wear ? "x" : "", a_slot->value, Where(a_reading.table, *a_slot, &here)));
				} else if (a_value) {
					a_slot = Set{ .value = *a_value, .file = here.file, .line = here.line, .load = a_reading.table.load };
					return true;
				}
				return false;
			};
			const auto wear = put(entry.wear, a_numbers.wear, true);
			const auto damage = put(entry.damage, a_numbers.damage, false);
			if (!wear && !damage) {
				return;
			}

			const auto wears = wear ? std::format("wears x{:.2f}", *a_numbers.wear) : std::string{};
			const auto keeps = damage ? std::format("keeps {:.2f} of its damage at 0 condition", *a_numbers.damage) : std::string{};
			REX::DEBUG("{:s} line {:d}: {:s} {:s}{:s}{:s}.", a_line.file, a_line.number, who, wears, wear && damage ? " and " : "", keeps);
			auto& mine = a_reading.mine[std::to_underlying(a_kind)];
			if (std::ranges::find(mine, a_form.formID) == mine.end()) {
				mine.push_back(a_form.formID);
			}
		}

		// A line whose form is loaded, by what the form is.
		void Take(Reading& a_reading, const ListFiles::Line& a_line, ListFiles::Faults& a_faults)
		{
			const auto&         form = a_line.form;
			std::optional<Kind> kind;
			if (form.IsWeapon()) {
				if (const auto* why = Condition::WhyNoCondition(static_cast<const RE::TESBoundObject&>(form))) {
					a_faults.Add(std::format("{:s} line {:d}: {:s} takes no condition ({:s}), so NEC skips it.", a_line.file, a_line.number,
						NameOf(form), why));
					return;
				}
				kind = Kind::kWeapon;
			} else if (form.Is(RE::ENUM_FORM_ID::kOMOD) &&
					   static_cast<const RE::BGSMod::Attachment::Mod&>(form).targetFormType.get() == RE::ENUM_FORM_ID::kWEAP) {
				kind = Kind::kMod;
			} else if (form.Is(RE::ENUM_FORM_ID::kKYWD)) {
				kind = Kind::kKeyword;
			}
			if (!kind) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not a weapon, a weapon mod or a keyword, so NEC skips it.", a_line.file,
					a_line.number, NameOf(form)));
			} else if (const auto numbers = Parse(a_line, a_faults)) {
				Keep(a_reading, *kind, form, *numbers, a_line, a_faults);
			}
		}

		// "Wear/MyWeapons.ini sets wear or damage for 5 forms: 1 by weapon, 2
		// by mod and 2 by keyword."
		void Report(std::string_view a_file, const std::array<std::vector<RE::TESFormID>, KINDS>& a_mine)
		{
			std::vector<std::string> parts;
			std::size_t              total = 0;
			for (std::size_t i = 0; i < KINDS; i++) {
				if (const auto count = a_mine[i].size(); count > 0) {
					parts.push_back(std::format("{:d} by {:s}", count, KIND_NAMES[i]));
					total += count;
				}
			}
			REX::INFO("{:s} sets wear or damage for {:d} {:s}{:s}.", a_file, total, total == 1 ? "form" : "forms", ListFiles::Joined(parts));
		}

		// -------------------------------------------------------------------
		// The list
		// -------------------------------------------------------------------

		// Read from any thread that asks Of, and never changed once
		// published. g_any lets a call with no file skip the lock. g_reads
		// counts the reads for Table::load, and only Load touches it.
		Snapshot<Table>   g_table;
		std::atomic<bool> g_any{ false };
		std::uint32_t     g_reads = 0;
	}

	void Load()
	{
		Reading reading;
		reading.table.load = ++g_reads;
		ListFiles::Read(
			LIST, [&reading](const ListFiles::Line& a_line, ListFiles::Faults& a_faults) { Take(reading, a_line, a_faults); },
			[&reading](std::string_view a_file) {
				Report(a_file, reading.mine);
				reading.mine = {};
			});

		const auto& table = reading.table;
		const auto  any = !table.weapons.empty() || !table.mods.empty() || !table.keywords.empty();
		g_table.Publish(any ? std::make_shared<const Table>(std::move(reading.table)) : nullptr);
		g_any.store(any, std::memory_order_relaxed);
	}

	void Unload()
	{
		g_any.store(false, std::memory_order_relaxed);
		g_table.Publish(nullptr);
	}

	bool Any()
	{
		return g_any.load(std::memory_order_relaxed);
	}

	std::shared_ptr<const Table> Current()
	{
		return g_table.Get();
	}

	std::string Where(const Table& a_table, const Set& a_set, const Set* a_after)
	{
		if (a_after && a_after->file == a_set.file) {
			return std::format("line {:d}", a_set.line);
		}
		const auto& files = a_table.files;
		const auto  known = a_set.load == a_table.load && a_set.file < files.size();
		return std::format("{:s} line {:d}", known ? files[a_set.file] : "?", a_set.line);
	}
}
