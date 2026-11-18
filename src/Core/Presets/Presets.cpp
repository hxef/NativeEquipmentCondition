#include "Core/Presets/Shared.h"

#include "Core/IniText.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// The pick: reading it at the game start, the list, and where a change from
// the MCM page goes. What the page's buttons do is in Actions.cpp.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// What a preset holds
		// -------------------------------------------------------------------

		// What a preset does not hold: a setting that only changes what a menu
		// or the HUD shows goes here, so it stays the player's own whatever is
		// picked. Every gameplay rule is held by default, so a setting added
		// later is held unless it is named here. Each bar's flip is in [HUD].
		// NOT_HELD points at the settings, so renaming one fails to build
		// instead of making it held.
		constexpr std::string_view NOT_HELD_SECTIONS[]{ "HUD", "Log", "Presets" };
		constexpr const Settings::Named* NOT_HELD[]{
			&Settings::iWeaponCnd,
			&Settings::iPowerArmorCnd,
			&Settings::iQuickContainerCnd,
			&Settings::iItemCardCnd,
			&Settings::bCardWearRate,
			&Settings::bPowerArmorCardCnd,
			&Settings::iCndDecimals,
			&Settings::bCndColumn,
			&Settings::iRepairBoxRow,
			&Settings::bRepairBoxColumns,
			&Settings::bRepairBoxGains,
			&Settings::iSpareModRows,
			&Settings::bTraderSign,
			&Settings::bFridgeIcon,
			&Settings::bLoadingTips,
			&Settings::bConfirmScroll,
			&Settings::bInspectPrice,
			&Settings::bNecOn,
		};

		State g_state;
	}

	bool Held(std::string_view a_section, std::string_view a_key)
	{
		const auto same = [](std::string_view a_lhs, std::string_view a_rhs) { return IniText::SameName(a_lhs, a_rhs); };
		if (std::ranges::any_of(NOT_HELD_SECTIONS, [&](std::string_view a_other) { return same(a_other, a_section); })) {
			return false;
		}
		return std::ranges::none_of(NOT_HELD, [&](const Settings::Named* a_other) {
			return same(a_other->section, a_section) && same(a_other->key, a_key);
		});
	}

	// -------------------------------------------------------------------
	// What the files share
	// -------------------------------------------------------------------

	State& Now()
	{
		return g_state;
	}

	void Say(const Lines& a_lines)
	{
		for (const auto& line : a_lines) {
			if (line.warn) {
				REX::WARN("{:s}", line.text);
			} else {
				REX::INFO("{:s}", line.text);
			}
		}
	}

	Entry DefaultEntry()
	{
		return { .name = "Default", .stem = {}, .kind = Kind::kDefault, .file = {}, .changes = Settings::UserFile() };
	}

	std::string ShownOf(const Entry& a_entry)
	{
		return a_entry.kind != Kind::kOwn && a_entry.changed > 0 ? a_entry.name + " (Custom)" : a_entry.name;
	}

	bool SameEntry(const Entry& a_lhs, const Entry& a_rhs)
	{
		return a_lhs.kind == a_rhs.kind && (a_lhs.kind == Kind::kDefault || SameFile(a_lhs.file.native(), a_rhs.file.native()));
	}

	void Take(State& a_state, const Entry& a_entry, const Applied& a_applied)
	{
		a_state.picked = a_entry;
		a_state.picked.changed = a_applied.changes;
		a_state.original = a_applied.original;
		a_state.given = a_applied.given;
		a_state.about = a_applied.about;
		a_state.dangling.clear();
		Settings::sPreset.SetValue(a_entry.stem);
		const auto& [section, key] = Settings::PRESET_KEY;
		const auto kept = a_entry.kind == Kind::kDefault ? Settings::Drop(section, key) : Settings::Keep(section, key, a_entry.stem);
		if (!kept) {
			REX::WARN("{:s} stays {:s} only until the game restarts.", key, a_entry.stem.empty() ? "empty"sv : std::string_view{ a_entry.stem });
		}
	}

	std::string Count(std::size_t a_count, std::string_view a_word)
	{
		return std::format("{:d} {:s}{:s}", a_count, a_word, a_count == 1 ? "" : "s");
	}

	// -------------------------------------------------------------------
	// The game start
	// -------------------------------------------------------------------

	void Load()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		const auto scan = Scan();

		// Matched in any case, as Windows matches the file names.
		const auto& text = Settings::sPreset.GetValue();
		std::wstring wide;
		const auto named = [&](const std::filesystem::path& a_file) { return !wide.empty() && SameFile(a_file.stem().native(), wide); };
		auto chosen = DefaultEntry();
		Lines gone;
		if (!text.empty()) {
			REX::UTF8_TO_UTF16(text, wide);
			const auto it = std::ranges::find_if(scan.entries, [&](const Entry& a_entry) { return named(a_entry.file); });
			if (it != scan.entries.end()) {
				chosen = *it;
			} else {
				// Default stands for this game, and sPreset and every file stay,
				// so putting the file back brings it all back.
				state.dangling = text;
				const auto left = std::ranges::find_if(scan.alike, named);
				auto line = left != scan.alike.end() ?
				                std::format("NEC_custom.ini picks the preset {:s}, but {:s} is left out of the list, since another file lists under its name, so Default stands until you pick a preset.", text, LogName(*left)) :
				                std::format("NEC_custom.ini picks the preset {:s}, but neither MCM_Presets nor MCM_Presets/Custom has {:s}.ini, so Default stands until you pick a preset.", text, text);
				if (const auto kept = std::ranges::find_if(scan.orphans, named); kept != scan.orphans.end()) {
					line += std::format(" Your changes to {:s} stay in {:s}.", text, LogName(*kept));
				}
				gone.push_back({ true, std::move(line) });
			}
		}

		const auto applied = Apply(chosen);
		state.picked = chosen;
		state.picked.changed = applied.changes;
		state.original = applied.original;
		state.given = applied.given;
		state.about = applied.about;

		// NEC.log is not open yet, so Report writes these.
		state.kept.push_back(applied.start);
		state.kept.insert(state.kept.end(), applied.lines.begin(), applied.lines.end());
		state.kept.insert(state.kept.end(), gone.begin(), gone.end());
		state.kept.insert(state.kept.end(), scan.lines.begin(), scan.lines.end());
	}

	void Report()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		Say(state.kept);
		state.kept.clear();
	}

	// -------------------------------------------------------------------
	// The list
	// -------------------------------------------------------------------

	namespace
	{
		// The list as both folders hold it now. With the lock held.
		std::vector<Entry> Listed(State& a_state)
		{
			// The start already said what each folder leaves out.
			auto scan = Scan();

			std::vector<Entry> list{ DefaultEntry() };
			list.front().changed = ReadFile(Settings::UserFile(), false, Defaults(), {}).set;
			for (auto& entry : scan.entries) {
				if (entry.kind == Kind::kOriginal) {
					entry.changed = ReadFile(entry.changes, true, Defaults(), {}).set;
				}
				list.push_back(std::move(entry));
			}

			// The picked preset stays in the list while it is picked, even with
			// its file removed by hand, so the page still shows what is in play.
			const auto it = std::ranges::find_if(list, [&](const Entry& a_entry) { return SameEntry(a_entry, a_state.picked); });
			if (it != list.end()) {
				a_state.picked.changed = it->changed;
			} else {
				const auto at = std::ranges::find_if(std::next(list.begin()), list.end(), [&](const Entry& a_entry) { return Before(a_state.picked, a_entry); });
				list.insert(at, a_state.picked);
			}
			return list;
		}
	}

	std::vector<Entry> Entries()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		state.shown = Listed(state);
		return state.shown;
	}

	std::size_t Picked()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		// MCM asks for the place before the list, so the place is found in
		// the list its next ask gets. The kept list stays the one MCM shows,
		// which a pick's index means.
		const auto list = Listed(state);
		const auto it = std::ranges::find_if(list, [&](const Entry& a_entry) { return SameEntry(a_entry, state.picked); });
		return it != list.end() ? static_cast<std::size_t>(std::distance(list.begin(), it)) : 0;
	}

	std::string PickedName()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		return state.picked.name;
	}

	std::string PickedStem()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		return state.picked.stem;
	}

	bool PickedHasChanges()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		return state.picked.kind != Kind::kOwn && state.picked.changed > 0;
	}

	bool PickedIsOwn()
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		return state.picked.kind == Kind::kOwn;
	}

	// -------------------------------------------------------------------
	// Where a change goes
	// -------------------------------------------------------------------

	std::filesystem::path Target(std::string_view a_section, std::string_view a_key)
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		const auto& picked = state.picked;
		if (picked.kind == Kind::kDefault || !Held(a_section, a_key)) {
			return Settings::UserFile();
		}
		return picked.kind == Kind::kOriginal ? picked.changes : picked.file;
	}

	namespace
	{
		template <class T>
		std::string BaseOf(const Settings::Live<T>& a_setting)
		{
			auto& state = Now();
			const std::scoped_lock l{ state.lock };
			const auto index = HeldIndexOf(a_setting);
			// A text that failed its check counts as left out, so NEC.ini's
			// value is where the line goes.
			if (index && state.picked.kind == Kind::kOriginal && *index < state.original.size() && state.original[*index]) {
				return TextOf(*state.original[*index]);
			}
			return TextOf(Value{ a_setting.GetValueDefault() });
		}
	}

	std::string Base(const Settings::Live<bool>& a_setting)
	{
		return BaseOf(a_setting);
	}

	std::string Base(const Settings::Live<float>& a_setting)
	{
		return BaseOf(a_setting);
	}

	std::string Base(const Settings::Live<std::int32_t>& a_setting)
	{
		return BaseOf(a_setting);
	}

	std::string OwnTextHeld(const State& a_state, std::string_view a_name, std::string_view a_from, std::string_view a_about,
		std::size_t& a_count)
	{
		auto text = std::format(
			"; NEC preset: {:s}\n"
			"; Saved in game from {:s}.\n"
			"; A gameplay setting left out takes NEC.ini's value. A shared preset goes in Data\\F4SE\\Plugins\\NEC\\Config\\MCM_Presets.\n"
			"\n",
			a_name, a_from);
		const auto held = HeldSettings();
		const auto defaults = Defaults();
		std::string_view section;
		a_count = 0;
		for (std::size_t i = 0; i < held.size(); ++i) {
			// A switch left to another mod is false in play, so it takes what
			// the picked preset's files give it.
			std::optional<Value> value = NowOf(held[i]);
			if (IsYielded(held[i])) {
				value = i < a_state.given.size() ? a_state.given[i] : std::nullopt;
			}
			if (!value || TextOf(*value) == TextOf(defaults[i])) {
				continue;
			}
			const auto& named = NamedOf(held[i]);
			if (named.section != section) {
				text += std::format("{:s}[{:s}]\n", section.empty() ? "" : "\n", named.section);
				section = named.section;
			}
			text += std::format("{:s}={:s}\n", named.key, TextOf(*value));
			++a_count;
		}
		if (!a_about.empty()) {
			text += std::format("{:s}[{:s}]\n{:s}={:s}\n", section.empty() ? "" : "\n", ABOUT_KEY.section, ABOUT_KEY.key,
				AboutWritten(a_about));
		}
		return text;
	}

	std::string NewText(const std::filesystem::path& a_file, bool a_drop)
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		const auto& picked = state.picked;
		if (picked.kind == Kind::kOwn && SameFile(a_file.native(), picked.file.native())) {
			std::error_code ec;
			if (std::filesystem::exists(a_file, ec)) {
				return {};
			}
			// Removed by hand while picked: the whole preset is written again,
			// so the next game start still finds it. Wrote logs it.
			state.written = a_file;
			std::size_t count = 0;
			return OwnTextHeld(state, picked.name, picked.name, state.about, count);
		}
		if (!a_drop && picked.kind == Kind::kOriginal && SameFile(a_file.native(), picked.changes.native())) {
			return std::format("; Your changes to the NEC preset {:s}, kept by NEC's main MCM page.\n\n", picked.name);
		}
		return {};
	}

	std::string_view Stands(const std::filesystem::path& a_file)
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		const auto& picked = state.picked;
		const bool changes = picked.kind == Kind::kOriginal && SameFile(a_file.native(), picked.changes.native());
		return changes ? "the preset's"sv : "NEC.ini's"sv;
	}

	bool Wrote(const std::filesystem::path& a_file)
	{
		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		auto& picked = state.picked;
		std::error_code ec;
		if (!state.written.empty() && SameFile(a_file.native(), state.written.native())) {
			if (std::filesystem::exists(a_file, ec)) {
				REX::INFO("{:s} was gone, so NEC wrote {:s} again from the settings in play.", LogName(a_file), picked.name);
			}
			state.written.clear();
		}
		const auto mine = (picked.kind == Kind::kDefault && SameFile(a_file.native(), Settings::UserFile().native())) ||
		                  (picked.kind == Kind::kOriginal && SameFile(a_file.native(), picked.changes.native()));
		if (!mine) {
			return false;
		}
		const auto had = picked.changed > 0;
		picked.changed = ReadFile(a_file, picked.kind == Kind::kOriginal, Defaults(), {}).set;
		return had != (picked.changed > 0);
	}
}
