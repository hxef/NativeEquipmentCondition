#include "Core/Presets/Shared.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/IniText.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// The settings a preset holds, and setting them by a preset's stack.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// The held settings
		// -------------------------------------------------------------------

		// Each key of NEC.ini as [section] and key, in the file's order.
		std::vector<Settings::Named> KeysOfBaseFile(std::string& a_text)
		{
			std::vector<Settings::Named> keys;
			if (!Settings::ReadKept(Settings::BaseFile(), a_text)) {
				return keys;
			}
			std::string_view section;
			for (const auto line : IniText::Lines(a_text)) {
				const auto text = IniText::Trimmed(line);
				if (text.empty() || text.front() == ';' || text.front() == '#') {
					continue;
				}
				if (text.front() == '[') {
					section = IniText::Trimmed(text.substr(1, text.find(']') - 1));
					continue;
				}
				if (const auto equals = text.find('='); equals != std::string_view::npos) {
					keys.push_back({ section, IniText::Trimmed(text.substr(0, equals)) });
				}
			}
			return keys;
		}

		std::vector<Setting> Build()
		{
			std::vector<Setting> held;
			for (auto* on : Settings::EverySwitch()) {
				if (Held(on->section, on->key)) {
					held.emplace_back(on);
				}
			}
			for (const auto& number : Settings::Numbers()) {
				std::visit([&](auto* a_number) {
					if (Held(a_number->section, a_number->key)) {
						held.emplace_back(a_number);
					}
				},
					number);
			}
			// In NEC.ini's order, so an own preset reads as NEC.ini does. A
			// key NEC.ini lacks goes last.
			std::string text;
			const auto keys = KeysOfBaseFile(text);
			const auto place = [&](const Setting& a_setting) {
				const auto& named = NamedOf(a_setting);
				const auto it = std::ranges::find_if(keys, [&](const Settings::Named& a_key) {
					return IniText::SameName(a_key.section, named.section) && IniText::SameName(a_key.key, named.key);
				});
				return static_cast<std::size_t>(std::distance(keys.begin(), it));
			};
			std::ranges::stable_sort(held, {}, place);
			return held;
		}

		template <class T>
		void Set(Settings::Live<T>* a_live, const Value& a_value)
		{
			a_live->SetValue(std::get<T>(a_value));
		}
	}

	// -------------------------------------------------------------------
	// The held settings
	// -------------------------------------------------------------------

	std::span<const Setting> HeldSettings()
	{
		static const std::vector<Setting> held = Build();
		return held;
	}

	std::span<const Settings::Named* const> HeldNames()
	{
		static const std::vector<const Settings::Named*> names = [] {
			std::vector<const Settings::Named*> out;
			for (const auto& setting : HeldSettings()) {
				out.push_back(&NamedOf(setting));
			}
			return out;
		}();
		return names;
	}

	const Settings::Named& NamedOf(const Setting& a_setting)
	{
		return std::visit([](auto* a_live) -> const Settings::Named& { return *a_live; }, a_setting);
	}

	std::optional<std::size_t> HeldIndexOf(const Settings::Named& a_setting)
	{
		const auto names = HeldNames();
		const auto it = std::ranges::find(names, &a_setting);
		return it != names.end() ? std::optional{ static_cast<std::size_t>(std::distance(names.begin(), it)) } : std::nullopt;
	}

	Values Defaults()
	{
		Values out;
		for (const auto& setting : HeldSettings()) {
			out.push_back(std::visit([](auto* a_live) -> Value { return a_live->GetValueDefault(); }, setting));
		}
		return out;
	}

	Value NowOf(const Setting& a_setting)
	{
		return std::visit([](auto* a_live) -> Value { return a_live->GetValue(); }, a_setting);
	}

	std::string TextOf(const Value& a_value)
	{
		return std::visit([](auto a_number) -> std::string {
			if constexpr (std::is_same_v<decltype(a_number), bool>) {
				return a_number ? "true" : "false";
			} else if constexpr (std::is_same_v<decltype(a_number), float>) {
				return std::format("{:g}", a_number);
			} else {
				return std::format("{:d}", a_number);
			}
		},
			a_value);
	}

	bool IsYielded(const Setting& a_setting)
	{
		const auto* const* on = std::get_if<Settings::Live<bool>*>(&a_setting);
		return on && CallPatch::IsYielded(**on);
	}

	// -------------------------------------------------------------------
	// Setting the values
	// -------------------------------------------------------------------

	Applied Apply(const Entry& a_entry)
	{
		const auto held = HeldSettings();
		auto values = Defaults();
		Applied out;
		out.original.resize(held.size());
		out.given.resize(held.size());
		const auto take = [&](const Read& a_read) {
			for (std::size_t i = 0; i < held.size(); ++i) {
				if (a_read.given[i]) {
					values[i] = *a_read.given[i];
					out.given[i] = a_read.given[i];
				}
			}
		};
		const auto counted = [&](const Read& a_read) {
			out.lines.push_back({ false, std::format("{:s} sets {:d} of the {:d} settings a preset holds, and NEC.ini gives the rest.",
											 LogName(a_entry.file), a_read.set, held.size()) });
		};
		// Only the preset's own file describes it, never a changes file.
		const auto described = [&](const Read& a_own) {
			out.about = a_own.about;
			if (const auto letters = LettersOf(a_own.about); letters > MOST_ABOUT_LETTERS) {
				out.lines.push_back({ true, std::format("{:s} has a description of {:d} letters, so NEC's MCM page cuts it at {:d}.",
												LogName(a_entry.file), letters, MOST_ABOUT_LETTERS) });
			}
		};

		switch (a_entry.kind) {
		case Kind::kDefault:
			{
				const auto changes = ReadFile(a_entry.changes, false, values, {});
				take(changes);
				out.changes = changes.set;
				out.start = { false, out.changes > 0 ? "Preset Default, from NEC.ini, with your changes from NEC_custom.ini." : "Preset Default, from NEC.ini." };
				break;
			}
		case Kind::kOriginal:
			{
				const auto own = ReadFile(a_entry.file, true, values, "NEC.ini's");
				take(own);
				out.original = own.given;
				// A bad text here leaves the shipped preset's value standing.
				const auto changes = ReadFile(a_entry.changes, true, values, std::format("{:s}'s", a_entry.name));
				take(changes);
				out.changes = changes.set;
				out.start = { false, out.changes > 0 ?
										 std::format("Preset {:s}, from {:s}, with your changes from {:s}.", a_entry.name, LogName(a_entry.file), LogName(a_entry.changes)) :
										 std::format("Preset {:s}, from {:s}.", a_entry.name, LogName(a_entry.file)) };
				counted(own);
				out.lines.insert(out.lines.end(), own.faults.begin(), own.faults.end());
				out.lines.insert(out.lines.end(), changes.faults.begin(), changes.faults.end());
				described(own);
				break;
			}
		case Kind::kOwn:
			{
				const auto own = ReadFile(a_entry.file, true, values, "NEC.ini's");
				take(own);
				out.start = { false, std::format("Preset {:s}, from {:s}.", a_entry.name, LogName(a_entry.file)) };
				counted(own);
				out.lines.insert(out.lines.end(), own.faults.begin(), own.faults.end());
				described(own);
				break;
			}
		}

		// A switch left to another mod stays false, see CallPatch.h.
		for (std::size_t i = 0; i < held.size(); ++i) {
			if (IsYielded(held[i]) || NowOf(held[i]) == values[i]) {
				continue;
			}
			std::visit([&](auto* a_live) { Set(a_live, values[i]); }, held[i]);
			out.changed.push_back(&NamedOf(held[i]));
		}
		return out;
	}
}
