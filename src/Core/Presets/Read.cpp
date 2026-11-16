#include "Core/Presets/Shared.h"

#include "Core/IniText.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

// The ini reader CommonLibF4 reads NEC.ini with. It brings in windows.h,
// without its min and max macros, which would break std::min and std::max.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <SimpleIni.h>
#undef ERROR

// Reading a preset's file, with each text checked first.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// A value's text
		// -------------------------------------------------------------------

		// What SimpleIni's GetBoolValue takes, which reads only the start:
		// true, yes, on and 1, or false, no, off and 0.
		bool IsSwitchText(std::string_view a_text)
		{
			if (a_text.empty()) {
				return false;
			}
			switch (a_text[0]) {
			case 't':
			case 'T':
			case 'y':
			case 'Y':
			case '1':
			case 'f':
			case 'F':
			case 'n':
			case 'N':
			case '0':
				return true;
			case 'o':
			case 'O':
				return a_text.size() > 1 && std::string_view{ "nNfF" }.contains(a_text[1]);
			default:
				return false;
			}
		}

		// A whole number in decimal, or in hex after 0x, as SimpleIni reads it.
		bool IsWholeText(std::string_view a_text)
		{
			auto base = 10;
			if (a_text.starts_with("0x") || a_text.starts_with("0X")) {
				a_text.remove_prefix(2);
				base = 16;
			}
			std::int32_t value = 0;
			const auto [end, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value, base);
			return !a_text.empty() && ec == std::errc{} && end == a_text.data() + a_text.size();
		}

		bool IsNumberText(std::string_view a_text)
		{
			float value = 0.0F;
			const auto [end, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value);
			return !a_text.empty() && ec == std::errc{} && end == a_text.data() + a_text.size();
		}

		bool Passes(const Setting& a_setting, std::string_view a_text)
		{
			return std::visit([&](auto* a_live) {
				using T = std::remove_cvref_t<decltype(a_live->GetValue())>;
				if constexpr (std::is_same_v<T, bool>) {
					return IsSwitchText(a_text);
				} else if constexpr (std::is_same_v<T, float>) {
					return IsNumberText(a_text);
				} else {
					return IsWholeText(a_text);
				}
			},
				a_setting);
		}

		// Whether NEC has a setting a_key under [a_section] at all.
		bool Known(std::string_view a_section, std::string_view a_key)
		{
			const auto id = std::format("{:s}:{:s}", a_key, a_section);
			const auto named = [&](const Settings::Named& a_named) {
				return IniText::SameName(a_named.section, a_section) && IniText::SameName(a_named.key, a_key);
			};
			return Settings::SwitchById(id) || Settings::NumberById<float>(id) || Settings::NumberById<std::int32_t>(id) ||
			       named({ "Log", "sLogLevel" }) || named(Settings::PRESET_KEY);
		}

		std::optional<std::size_t> HeldIndex(std::string_view a_section, std::string_view a_key)
		{
			const auto held = HeldSettings();
			for (std::size_t i = 0; i < held.size(); ++i) {
				const auto& named = NamedOf(held[i]);
				if (IniText::SameName(named.section, a_section) && IniText::SameName(named.key, a_key)) {
					return i;
				}
			}
			return std::nullopt;
		}

		// The first 10 warnings of a file, then 1 count line.
		constexpr std::size_t MOST_FAULTS = 10;
	}

	// -------------------------------------------------------------------
	// Reading a file
	// -------------------------------------------------------------------

	Read ReadFile(const std::filesystem::path& a_file, bool a_check, const Values& a_before, std::string_view a_stands)
	{
		const auto held = HeldSettings();
		Read out;
		out.given.resize(held.size());
		const auto name = LogName(a_file);
		std::size_t more = 0;
		const auto fault = [&](std::string a_line) {
			if (!a_check) {
				return;
			}
			if (out.faults.size() < MOST_FAULTS) {
				out.faults.push_back({ true, std::move(a_line) });
			} else {
				++more;
			}
		};

		std::error_code ec;
		if (!std::filesystem::exists(a_file, ec)) {
			return out;
		}
		const auto size = std::filesystem::file_size(a_file, ec);
		if (!ec && size > MOST_BYTES) {
			fault(std::format("{:s} is bigger than 1 MB, so NEC skips it.", name));
			return out;
		}
		std::string text;
		CSimpleIniA ini;
		ini.SetUnicode(true);
		ini.SetQuotes(true);
		if (!Settings::ReadKept(a_file, text) || ini.LoadData(text.data(), text.size()) < 0) {
			fault(std::format("{:s} could not be read, so NEC skips it.", name));
			return out;
		}
		// NEC_custom.ini is read as the setting store reads it, up to a NUL.
		if (a_check && text.find('\0') != std::string::npos) {
			fault(std::format("{:s} is not plain text, as a file saved as UTF-16 is, so NEC skips it. Save it as UTF-8.", name));
			return out;
		}

		CSimpleIniA::TNamesDepend sections;
		ini.GetAllSections(sections);
		sections.sort(CSimpleIniA::Entry::LoadOrder());
		for (const auto& section : sections) {
			CSimpleIniA::TNamesDepend keys;
			ini.GetAllKeys(section.pItem, keys);
			keys.sort(CSimpleIniA::Entry::LoadOrder());
			for (const auto& key : keys) {
				const std::string_view in{ section.pItem };
				const std::string_view what{ key.pItem };
				// A preset's description is no setting, so it is no fault
				// and no count.
				if (IniText::SameName(in, ABOUT_KEY.section) && IniText::SameName(what, ABOUT_KEY.key)) {
					out.about = ini.GetValue(section.pItem, key.pItem, "");
					continue;
				}
				const auto index = HeldIndex(in, what);
				if (!index) {
					fault(Known(in, what) ?
							  std::format("{:s} sets {:s}, which a preset does not hold, so it does nothing. It goes in NEC_custom.ini.", name, what) :
							  std::format("{:s} sets {:s} under [{:s}], which NEC does not know, so it does nothing.", name, what, in));
					continue;
				}
				const auto& setting = held[*index];
				const std::string_view raw{ ini.GetValue(section.pItem, key.pItem, "") };
				if (a_check && !Passes(setting, raw)) {
					const auto* const switched = std::get_if<Settings::Live<bool>*>(&setting);
					fault(std::format("{:s} sets {:s} to {:s}, which {:s}, so {:s} {:s} stands.", name, NamedOf(setting).key, raw,
						switched ? "is not true or false"sv : "is no number"sv, a_stands, TextOf(a_before[*index])));
					continue;
				}
				// Read as the setting store reads NEC.ini, by the setting's own
				// section and key, which the reader matches in any case.
				out.given[*index] = std::visit([&](auto* a_live) -> Value {
					auto value = a_live->GetValueDefault();
					REX::Impl::IniSettingLoad(&ini, a_live->section, a_live->key, value, value);
					return value;
				},
					setting);
				++out.set;
			}
		}
		if (more > 0) {
			out.faults.push_back({ true, std::format("{:s} has {:d} more {:s} like these.", name, more, more == 1 ? "fault"sv : "faults"sv) });
		}
		return out;
	}
}
