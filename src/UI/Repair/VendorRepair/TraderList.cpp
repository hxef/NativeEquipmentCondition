#include "UI/Repair/VendorRepair/TraderList.h"

#include "Condition/Repair.h"
#include "Core/IniText.h"
#include "Core/ListFiles.h"
#include "Core/Snapshot.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace VendorRepair::TraderList
{
	namespace
	{
		// -------------------------------------------------------------------
		// Naming forms
		// -------------------------------------------------------------------

		// Who a line names, in the order a trader is matched.
		enum class Tier
		{
			kPlaced,
			kNpc,
			kFaction,
		};

		constexpr std::size_t TIERS = 3;

		// A form the game made, which lasts 1 save.
		[[nodiscard]] bool Made(RE::TESFormID a_id)
		{
			return (a_id >> 24) == 0xFF;
		}

		// "Lucas Miller [00130B7A]", or "[001679CE]" for a form with no name.
		// A reference goes by its base's name.
		std::string NameOf(const RE::TESForm& a_form)
		{
			const auto*       ref = a_form.As<RE::TESObjectREFR>();
			const auto*       base = ref ? ref->GetObjectReference() : &a_form;
			const std::string name{ base ? RE::TESFullName::GetFullName(*base) : std::string_view{} };
			return name.empty() ? std::format("[{:08X}]", a_form.formID) : std::format("{:s} [{:08X}]", name, a_form.formID);
		}

		// "Fallout4.esm|002CB9", as a line names a form.
		std::string Spelled(const RE::TESFile& a_plugin, std::uint32_t a_local)
		{
			return std::format("{:s}|{:06X}", a_plugin.GetFilename(), a_local);
		}

		// The same for a form ID in the load order, by the plugin whose slot
		// it carries, or nothing for a form the game made.
		std::optional<std::string> Spelled(RE::TESFormID a_id)
		{
			if (!g_dataHandler || Made(a_id)) {
				return std::nullopt;
			}
			for (const auto* plugin : g_dataHandler->files) {
				if (plugin && plugin->IsActive() && plugin->GetLoadOrderFormID(a_id) == a_id) {
					return Spelled(*plugin, plugin->IsLight() ? a_id & 0xFFF : a_id & 0xFFFFFF);
				}
			}
			return std::nullopt;
		}

		// "Fallout4.esm|002CB7 as NPC", or "FF001A2B as placed in this save
		// only" for a form the game made.
		std::string Role(RE::TESFormID a_id, std::string_view a_role)
		{
			const auto spelled = Spelled(a_id);
			return spelled ? std::format("{:s} as {:s}", *spelled, a_role) : std::format("{:08X} as {:s} in this save only", a_id, a_role);
		}

		// The NPC record a_trader was placed from: the one a templated NPC
		// starts as, else their base, never one the game made, since a
		// templated trader such as Percy gets a base made in the save. 0 for
		// none.
		RE::TESFormID NpcOf(const RE::Actor& a_trader)
		{
			const RE::TESForm* records[2]{};
			if (a_trader.extraList) {
				if (const auto* leveled = a_trader.extraList->GetByType<RE::ExtraLeveledCreature>()) {
					records[0] = leveled->originalBase;
				}
			}
			records[1] = a_trader.GetObjectReference();
			for (const auto* record : records) {
				if (record && record->Is(RE::ENUM_FORM_ID::kNPC_) && !Made(record->formID)) {
					return record->formID;
				}
			}
			return 0;
		}

		// The IDs a_trader is matched by, 0 for a tier they have none of.
		std::array<RE::TESFormID, TIERS> WhoIs(const RE::Actor& a_trader)
		{
			const auto* faction = a_trader.vendorFaction;
			return { Made(a_trader.formID) ? 0 : a_trader.formID, NpcOf(a_trader), faction ? faction->formID : 0 };
		}

		// -------------------------------------------------------------------
		// The lines
		// -------------------------------------------------------------------

		constexpr ListFiles::List LIST{
			.folder = "Traders",
			.unread = "measures every trader",
			.word = "kind level",
			.madeHint = "Name the trader's NPC record or faction, as NEC.ui.trace.log shows them.",
		};

		constexpr Text::Trade TRADES[]{ Text::Trade::kWeapons, Text::Trade::kArmor, Text::Trade::kClothing };

		// 1 tier's lines by form ID, each kind set at most once.
		using Lines = std::unordered_map<RE::TESFormID, std::array<std::optional<Set>, std::size(TRADES)>>;
		using Table = std::array<Lines, TIERS>;

		// What the files set so far, and what the file being read set of each
		// tier, for its summary.
		struct Reading
		{
			Table                            table;
			std::array<std::size_t, TIERS> mine{};
		};

		// "armor 90" as the kind and its level, or nothing once a_faults says
		// what is wrong with it.
		std::optional<std::pair<Text::Trade, Set>> Parse(std::string_view a_word, std::string_view a_file, std::size_t a_line,
			ListFiles::Faults& a_faults)
		{
			const auto gap = a_word.find_first_of(" \t");
			const auto kind = a_word.substr(0, gap);
			const auto level = gap == std::string_view::npos ? std::string_view{} : IniText::Trimmed(a_word.substr(gap));
			if (level.empty() || level.find_first_of(" \t") != std::string_view::npos) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not a kind and a level, such as armor 70, so NEC skips it.",
					a_file, a_line, a_word));
				return std::nullopt;
			}
			const auto trade = std::ranges::find_if(TRADES, [kind](Text::Trade a_trade) { return IniText::SameName(Named(a_trade), kind); });
			if (trade == std::end(TRADES)) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not weapons, armor or clothing, so NEC skips it.", a_file, a_line, kind));
				return std::nullopt;
			}

			// measured is the 1 word that leaves the level empty.
			Set           set{ .level = std::nullopt, .file = std::string{ a_file }, .line = a_line };
			std::uint32_t percent = 0;
			const auto [end, ec] = std::from_chars(level.data(), level.data() + level.size(), percent);
			if (IniText::SameName(level, "none")) {
				set.level = 0;
			} else if (ec == std::errc{} && end == level.data() + level.size() && percent >= MIN_CEILING && percent <= Repair::FULL &&
					   percent % 10 == 0) {
				set.level = percent;
			} else if (!IniText::SameName(level, "measured")) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not 30 to 100 in steps of 10, none or measured, so NEC skips it.",
					a_file, a_line, level));
				return std::nullopt;
			}
			return std::pair{ *trade, std::move(set) };
		}

		// "the faction [001679CE] sets armor to 90%", for the debug line.
		std::string Sets(Tier a_tier, std::string_view a_who, Text::Trade a_trade, const std::optional<std::uint32_t>& a_level)
		{
			const auto who = a_tier == Tier::kPlaced ? std::format("{:s} as placed", a_who) :
			                 a_tier == Tier::kNpc    ? std::format("the NPC record {:s}", a_who) :
			                                           std::format("the faction {:s}", a_who);
			if (!a_level) {
				return std::format("{:s} leaves {:s} measured", who, Named(a_trade));
			}
			return *a_level == 0 ? std::format("{:s} sets {:s} to no repair", who, Named(a_trade)) :
			                       std::format("{:s} sets {:s} to {:d}%", who, Named(a_trade), *a_level);
		}

		// 1 line into a_reading, said in NEC.log. a_who is how NEC.log names
		// the form, and a_unloaded says it was not loaded.
		void Keep(Reading& a_reading, Tier a_tier, RE::TESFormID a_id, std::string_view a_who, bool a_unloaded, std::string_view a_word,
			std::string_view a_file, std::size_t a_line, ListFiles::Faults& a_faults)
		{
			auto parsed = Parse(a_word, a_file, a_line, a_faults);
			if (!parsed) {
				return;
			}
			auto& [trade, set] = *parsed;
			auto& slot = a_reading.table[std::to_underlying(a_tier)][a_id][std::to_underlying(trade)];
			if (slot) {
				const auto where = slot->file == a_file ? std::format("line {:d}", slot->line) : std::format("{:s} line {:d}", slot->file, slot->line);
				a_faults.Add(std::format("{:s} line {:d}: {:s} has {:s} set again, so NEC keeps {:s}.", a_file, a_line, a_who, Named(trade), where));
				return;
			}
			if (a_unloaded) {
				REX::INFO("{:s} line {:d}: {:s} is not loaded as the game starts, so NEC takes it for a trader placed in the world.", a_file,
					a_line, a_who);
			}
			REX::DEBUG("{:s} line {:d}: {:s}.", a_file, a_line, Sets(a_tier, a_who, trade, set.level));
			slot = std::move(set);
			a_reading.mine[std::to_underlying(a_tier)]++;
		}

		// A line whose form is loaded, by what the form is.
		void Take(Reading& a_reading, const ListFiles::Line& a_line, ListFiles::Faults& a_faults)
		{
			const auto& form = a_line.form;
			const auto  tier = form.Is(RE::ENUM_FORM_ID::kACHR) ? std::optional{ Tier::kPlaced } :
			                   form.Is(RE::ENUM_FORM_ID::kNPC_) ? std::optional{ Tier::kNpc } :
			                   form.Is(RE::ENUM_FORM_ID::kFACT) ? std::optional{ Tier::kFaction } :
			                                                      std::nullopt;
			if (!tier) {
				a_faults.Add(std::format("{:s} line {:d}: {:s} is not a trader, an NPC record or a faction, so NEC skips it.", a_line.file,
					a_line.number, NameOf(form)));
				return;
			}
			Keep(a_reading, *tier, form.formID, NameOf(form), false, a_line.word, a_line.file, a_line.number, a_faults);
		}

		// "Traders/MyTraders.ini sets 5 repair levels: 2 for traders as
		// placed, 1 for an NPC record and 2 for factions."
		void Report(std::string_view a_file, const std::array<std::size_t, TIERS>& a_counts)
		{
			constexpr std::string_view ONE[]{ "a trader as placed", "an NPC record", "a faction" };
			constexpr std::string_view MANY[]{ "traders as placed", "NPC records", "factions" };
			std::vector<std::string>   parts;
			std::size_t                total = 0;
			for (std::size_t i = 0; i < TIERS; i++) {
				if (a_counts[i] > 0) {
					parts.push_back(std::format("{:d} for {:s}", a_counts[i], a_counts[i] == 1 ? ONE[i] : MANY[i]));
					total += a_counts[i];
				}
			}
			std::string text;
			for (std::size_t i = 0; i < parts.size(); i++) {
				text += (i == 0 ? ": " : i + 1 == parts.size() ? " and " : ", ") + parts[i];
			}
			REX::INFO("{:s} sets {:d} repair {:s}{:s}.", a_file, total, total == 1 ? "level" : "levels", text);
		}

		// -------------------------------------------------------------------
		// The list
		// -------------------------------------------------------------------

		// Read from the main thread and from a Papyrus thread that asks
		// GetTraderRepairLevel, and never changed once published.
		Snapshot<Table> g_table;
	}

	void Load()
	{
		Reading reading;
		ListFiles::Read(
			LIST, [&reading](const ListFiles::Line& a_line, ListFiles::Faults& a_faults) { Take(reading, a_line, a_faults); },
			[&reading](const ListFiles::Unloaded& a_line, ListFiles::Faults& a_faults) {
				Keep(reading, Tier::kPlaced, a_line.plugin.GetLoadOrderFormID(a_line.local), Spelled(a_line.plugin, a_line.local), true,
					a_line.word, a_line.file, a_line.number, a_faults);
			},
			[&reading](std::string_view a_file) {
				Report(a_file, reading.mine);
				reading.mine = {};
			});

		const auto any = std::ranges::any_of(reading.table, [](const Lines& a_lines) { return !a_lines.empty(); });
		g_table.Publish(any ? std::make_shared<const Table>(std::move(reading.table)) : nullptr);
	}

	void Unload()
	{
		g_table.Publish(nullptr);
	}

	std::optional<Set> Find(const RE::Actor& a_trader, Text::Trade a_trade)
	{
		const auto table = g_table.Get();
		if (!table) {
			return std::nullopt;
		}
		const auto who = WhoIs(a_trader);
		for (std::size_t tier = 0; tier < TIERS; tier++) {
			const auto& lines = (*table)[tier];
			if (const auto at = who[tier] ? lines.find(who[tier]) : lines.end(); at != lines.end()) {
				if (const auto& set = at->second[std::to_underlying(a_trade)]) {
					return set;
				}
			}
		}
		return std::nullopt;
	}

	std::string Ids(const RE::Actor& a_trader)
	{
		const auto who = WhoIs(a_trader);
		const auto npc = who[std::to_underlying(Tier::kNpc)];
		const auto faction = who[std::to_underlying(Tier::kFaction)];
		return std::format("{:s}, {:s}, {:s}", Role(a_trader.formID, "placed"), npc ? Role(npc, "NPC") : "an NPC made in this save only",
			faction ? Role(faction, "faction") : "no faction");
	}
}
