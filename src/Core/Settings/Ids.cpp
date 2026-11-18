#include "Core/Settings/Settings.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <variant>

// Finding a switch or number by the id the MCM page and the Papyrus API write,
// or by its key alone. The lists it searches are in Line.cpp.
namespace Settings
{
	bool IsIdOf(const Named& a_setting, std::string_view a_id)
	{
		const auto same = [](std::string_view a_lhs, std::string_view a_rhs) {
			return std::ranges::equal(a_lhs, a_rhs, [](char a_l, char a_r) {
				return std::tolower(static_cast<unsigned char>(a_l)) == std::tolower(static_cast<unsigned char>(a_r));
			});
		};
		const auto key = a_setting.key.size();
		return a_id.size() == key + 1 + a_setting.section.size() && a_id[key] == ':' &&
		       same(a_id.substr(0, key), a_setting.key) && same(a_id.substr(key + 1), a_setting.section);
	}

	Live<bool>* SwitchById(std::string_view a_id)
	{
		const auto switches = EverySwitch();
		const auto it = std::ranges::find_if(switches, [&](const Live<bool>* a_switch) { return IsIdOf(*a_switch, a_id); });
		return it != switches.end() ? *it : nullptr;
	}

	const Named* SettingByKey(std::string_view a_key)
	{
		for (const auto* on : EverySwitch()) {
			if (on->key == a_key) {
				return on;
			}
		}
		for (const auto& number : Numbers()) {
			const auto* named = std::visit([](const auto* a_number) -> const Named* { return a_number; }, number);
			if (named->key == a_key) {
				return named;
			}
		}
		return nullptr;
	}
}
