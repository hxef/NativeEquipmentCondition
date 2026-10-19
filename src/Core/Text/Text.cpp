#include "Core/Text/Text.h"

#include "Core/Feature.h"
#include "Core/Text/Lines.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

// The sentences shown outside a repair: the perk page, the corner of the
// screen, and the section titles of the MCM page. Text.h says where the other
// functions are, and Translations.cpp picks the player's language.
namespace Text
{
	namespace
	{
		// The section titles by their id on the page. NEC.ini's sections stay
		// Features, Balance, HUD and Log, the ids of the settings.
		struct Section
		{
			std::string_view id;
			Key              title;
		};

		constexpr Section SECTIONS[]{
			{ "Wear", WEAR_TITLE },
			{ "Weapons", WEAPONS_TITLE },
			{ "Loot", LOOT_TITLE },
			{ "Repairs", REPAIRS_TITLE },
			{ "HUD", HUD_TITLE },
			{ "Extras", EXTRAS_TITLE },
			{ "Log", LOG_TITLE },
		};

		// A part's name, from PartsPlay.cpp, PartsLoot.cpp or PartsUi.cpp.
		// Nothing for kNone and from kTrace on.
		std::optional<Key> NameOf(Part a_part)
		{
			const auto index = static_cast<std::size_t>(a_part);
			const auto play = PlayParts();
			const auto loot = LootParts();
			const auto ui = UiParts();
			const auto firstPlay = static_cast<std::size_t>(Part::kPerks);
			const auto firstLoot = static_cast<std::size_t>(Part::kSpawn);
			const auto firstUi = static_cast<std::size_t>(Part::kCardCnd);
			if (index >= firstPlay && index < firstPlay + play.size()) {
				return play[index - firstPlay].name;
			}
			if (index >= firstLoot && index < firstLoot + loot.size()) {
				return loot[index - firstLoot].name;
			}
			if (index >= firstUi && index < firstUi + ui.size()) {
				return ui[index - firstUi].name;
			}
			return std::nullopt;
		}
	}

	const char* PickFor(Key a_key, Out a_out)
	{
		return a_out == Out::kLog ? InEnglish(a_key) : Pick(a_key);
	}

	std::string PerkDiscount(std::uint32_t a_percent, bool a_weapons, bool a_armor)
	{
		// A perk pricing neither never gets a line, so the weapons wording it
		// falls back to is never seen.
		return Say(a_weapons && a_armor ? PERK_DISCOUNT_BOTH : a_armor ? PERK_DISCOUNT_ARMOR : PERK_DISCOUNT_WEAPONS,
			a_percent);
	}

	std::string ArmorWornOut(std::string_view a_name)
	{
		return Say(ARMOR_WORN_OUT, a_name);
	}

	std::string MenuLine(std::string_view a_id)
	{
		const auto help = a_id.ends_with(MENU_HELP);
		const auto key = help ? a_id.substr(0, a_id.size() - MENU_HELP.size()) : a_id;
		for (const auto rows : { MenuSwitches(), MenuWear(), MenuNumbers(), MenuHudLog() }) {
			for (const auto& row : rows) {
				if (row.setting->key != key) {
					continue;
				}
				if (help) {
					return Pick(row.help);
				}
				return row.name ? Pick(*row.name) : PartName(PartOf(*row.setting));
			}
		}
		for (const auto& section : SECTIONS) {
			if (!help && section.id == a_id) {
				return Pick(section.title);
			}
		}
		return {};
	}

	std::string PartName(Part a_part)
	{
		const auto name = NameOf(a_part);
		return name ? Pick(*name) : std::string{};
	}

	std::string_view PartLogName(Part a_part)
	{
		const auto name = NameOf(a_part);
		return name ? std::string_view{ InEnglish(*name) } : std::string_view{};
	}

	std::string SettingLogName(const Settings::Named& a_setting)
	{
		for (const auto rows : { MenuSwitches(), MenuWear(), MenuNumbers(), MenuHudLog() }) {
			for (const auto& row : rows) {
				if (row.setting == &a_setting) {
					return std::string{ row.name ? std::string_view{ InEnglish(*row.name) } : PartLogName(PartOf(a_setting)) };
				}
			}
		}
		return std::string{ a_setting.key };
	}
}
