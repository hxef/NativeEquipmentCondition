#include "Core/Text/Text.h"

#include "Core/Feature.h"
#include "Core/Text/Lines.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

// Picking the language, and the sentences shown outside a repair: the perk page,
// the corner of the screen, and the section titles of the MCM page. Text.h
// says where the other tables are.
namespace Text
{
	namespace
	{
		// Said in the corner when a piece the player wears reaches 0. Each
		// language puts it so the name needs no gender.
		constexpr Line ARMOR_WORN_OUT[]{
			{ "en", "{0} is worn out." },
			{ "fr", "{0} est hors d'usage." },
			{ "de", "{0} ist verschlissen." },
			{ "it", "{0} è fuori uso." },
			{ "es", "{0} está inservible." },
			{ "esmx", "{0} está inservible." },
			{ "ptbr", "{0} está inutilizável." },
			{ "pl", "{0} nie nadaje się do użytku." },
			{ "ru", "{0}: полный износ." },
			{ "ja", "{0}は完全に消耗しました" },
			{ "zhhant", "{0}已完全磨損。" },
			{ "zhhans", "{0}已完全磨损。" },
		};

		// Added to every rank of a crafting perk that takes something off a
		// repair, with what that rank alone is worth, so the perk page shows
		// what the next rank gives. It says items built mostly from the perk's
		// mods because 1 perk prices the whole repair, see CraftingPerks.h. One
		// table per subject, weapons, armor or both, since the verb and the
		// adjectives follow the subject in most languages.
		constexpr Line PERK_DISCOUNT_WEAPONS[]{
			{ "en", "Weapons built mostly from this perk's mods take {0}% fewer components to repair at the workbench." },
			{ "fr", "Les armes composées en majorité de modules de cette aptitude demandent {0}\u00A0% de composants en moins à réparer à l'établi." },
			{ "de", "Waffen, die überwiegend aus Mods dieses Skills bestehen, benötigen an der Werkbank {0}\u00A0% weniger Komponenten für die Reparatur." },
			{ "it", "Le armi composte perlopiù da modifiche di questo talento richiedono il {0}% di componenti in meno per la riparazione al banco da lavoro." },
			{ "es", "Las armas compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "esmx", "Las armas compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "ptbr", "Armas compostas em sua maioria por mods desta vantagem exigem {0}% menos componentes para conserto na bancada." },
			{ "pl", "Bronie zbudowane głównie z modyfikacji tego profitu wymagają o {0}% mniej komponentów do naprawy w pracowni." },
			{ "ru", "Оружие, собранное в основном из модификаций этой способности, требует на {0}% меньше компонентов при ремонте на верстаке." },
			{ "ja", "このPerkのモジュールで主に構成された武器は、作業台での修理に必要な部品が{0}%減少します" },
			{ "zhhant", "主要由此輔助能力的改造配件組成的武器，在工作台修理時所需的元件減少{0}%。" },
			{ "zhhans", "主要由此辅助能力的改造配件组成的武器，在工作台修理时所需的元件减少{0}%。" },
		};

		constexpr Line PERK_DISCOUNT_ARMOR[]{
			{ "en", "Armor built mostly from this perk's mods takes {0}% fewer components to repair at the workbench." },
			{ "fr", "Les armures composées en majorité de modules de cette aptitude demandent {0}\u00A0% de composants en moins à réparer à l'établi." },
			{ "de", "Rüstungen, die überwiegend aus Mods dieses Skills bestehen, benötigen an der Werkbank {0}\u00A0% weniger Komponenten für die Reparatur." },
			{ "it", "Le armature composte perlopiù da modifiche di questo talento richiedono il {0}% di componenti in meno per la riparazione al banco da lavoro." },
			{ "es", "Las armaduras compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "esmx", "Las armaduras compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "ptbr", "Armaduras compostas em sua maioria por mods desta vantagem exigem {0}% menos componentes para conserto na bancada." },
			{ "pl", "Pancerze zbudowane głównie z modyfikacji tego profitu wymagają o {0}% mniej komponentów do naprawy w pracowni." },
			{ "ru", "Броня, собранная в основном из модификаций этой способности, требует на {0}% меньше компонентов при ремонте на верстаке." },
			{ "ja", "このPerkのモジュールで主に構成されたアーマーは、作業台での修理に必要な部品が{0}%減少します" },
			{ "zhhant", "主要由此輔助能力的改造配件組成的裝甲，在工作台修理時所需的元件減少{0}%。" },
			{ "zhhans", "主要由此辅助能力的改造配件组成的装甲，在工作台修理时所需的元件减少{0}%。" },
		};

		constexpr Line PERK_DISCOUNT_BOTH[]{
			{ "en", "Weapons and armor built mostly from this perk's mods take {0}% fewer components to repair at the workbench." },
			{ "fr", "Les armes et armures composées en majorité de modules de cette aptitude demandent {0}\u00A0% de composants en moins à réparer à l'établi." },
			{ "de", "Waffen und Rüstungen, die überwiegend aus Mods dieses Skills bestehen, benötigen an der Werkbank {0}\u00A0% weniger Komponenten für die Reparatur." },
			{ "it", "Le armi e le armature composte perlopiù da modifiche di questo talento richiedono il {0}% di componenti in meno per la riparazione al banco da lavoro." },
			{ "es", "Las armas y armaduras compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "esmx", "Las armas y armaduras compuestas en su mayoría por módulos de este extra necesitan un {0}% menos de componentes para repararse en el banco de trabajo." },
			{ "ptbr", "Armas e armaduras compostas em sua maioria por mods desta vantagem exigem {0}% menos componentes para conserto na bancada." },
			{ "pl", "Bronie i pancerze zbudowane głównie z modyfikacji tego profitu wymagają o {0}% mniej komponentów do naprawy w pracowni." },
			{ "ru", "Оружие и броня, собранные в основном из модификаций этой способности, требуют на {0}% меньше компонентов при ремонте на верстаке." },
			{ "ja", "このPerkのモジュールで主に構成された武器とアーマーは、作業台での修理に必要な部品が{0}%減少します" },
			{ "zhhant", "主要由此輔助能力的改造配件組成的武器和裝甲，在工作台修理時所需的元件減少{0}%。" },
			{ "zhhans", "主要由此辅助能力的改造配件组成的武器和装甲，在工作台修理时所需的元件减少{0}%。" },
		};

		constexpr Line WEAR_TITLE[]{
			{ "en", "Wear" },
			{ "fr", "Usure" },
			{ "de", "Abnutzung" },
			{ "it", "Usura" },
			{ "es", "Desgaste" },
			{ "esmx", "Desgaste" },
			{ "ptbr", "Desgaste" },
			{ "pl", "Zużycie" },
			{ "ru", "Износ" },
			{ "ja", "消耗" },
			{ "zhhant", "損耗" },
			{ "zhhans", "损耗" },
		};

		constexpr Line WEAPONS_TITLE[]{
			{ "en", "Weapons" },
			{ "fr", "Armes" },
			{ "de", "Waffen" },
			{ "it", "Armi" },
			{ "es", "Armas" },
			{ "esmx", "Armas" },
			{ "ptbr", "Armas" },
			{ "pl", "Broń" },
			{ "ru", "Оружие" },
			{ "ja", "武器" },
			{ "zhhant", "武器" },
			{ "zhhans", "武器" },
		};

		constexpr Line LOOT_TITLE[]{
			{ "en", "Loot and prices" },
			{ "fr", "Butin et prix" },
			{ "de", "Beute und Preise" },
			{ "it", "Bottino e prezzi" },
			{ "es", "Botín y precios" },
			{ "esmx", "Botín y precios" },
			{ "ptbr", "Saque e preços" },
			{ "pl", "Łupy i ceny" },
			{ "ru", "Трофеи и цены" },
			{ "ja", "戦利品と価格" },
			{ "zhhant", "戰利品與價格" },
			{ "zhhans", "战利品与价格" },
		};

		constexpr Line REPAIRS_TITLE[]{
			{ "en", "Repairs" },
			{ "fr", "Réparations" },
			{ "de", "Reparaturen" },
			{ "it", "Riparazioni" },
			{ "es", "Reparaciones" },
			{ "esmx", "Reparaciones" },
			{ "ptbr", "Consertos" },
			{ "pl", "Naprawy" },
			{ "ru", "Ремонт" },
			{ "ja", "修理" },
			{ "zhhant", "修理" },
			{ "zhhans", "修理" },
		};

		constexpr Line HUD_TITLE[]{
			{ "en", "HUD" },
			{ "fr", "ATH" },
			{ "de", "HUD" },
			{ "it", "Interfaccia" },
			{ "es", "HUD" },
			{ "esmx", "HUD" },
			{ "ptbr", "HUD" },
			{ "pl", "Interfejs" },
			{ "ru", "Интерфейс" },
			{ "ja", "HUD" },
			{ "zhhant", "狀態欄" },
			{ "zhhans", "状态栏" },
		};

		constexpr Line EXTRAS_TITLE[]{
			{ "en", "Little extras" },
			{ "fr", "Petits plus" },
			{ "de", "Kleine Extras" },
			{ "it", "Piccole aggiunte" },
			{ "es", "Pequeños detalles" },
			{ "esmx", "Pequeños detalles" },
			{ "ptbr", "Pequenos extras" },
			{ "pl", "Drobne dodatki" },
			{ "ru", "Приятные мелочи" },
			{ "ja", "おまけ" },
			{ "zhhant", "其他小功能" },
			{ "zhhans", "其他小功能" },
		};

		constexpr Line LOG_TITLE[]{
			{ "en", "Log" },
			{ "fr", "Journal" },
			{ "de", "Protokoll" },
			{ "it", "Registro" },
			{ "es", "Registro" },
			{ "esmx", "Registro" },
			{ "ptbr", "Registro" },
			{ "pl", "Dziennik" },
			{ "ru", "Журнал" },
			{ "ja", "ログ" },
			{ "zhhant", "日誌" },
			{ "zhhans", "日志" },
		};

		// The section titles by their id on the page. NEC.ini's sections stay
		// Features, Balance, HUD and Log, the ids of the settings.
		struct Section
		{
			std::string_view      id;
			std::span<const Line> title;
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

		constexpr std::string_view HELP = ".help";

		// A part's name in every language, from PartsPlay.cpp or PartsUi.cpp.
		// Empty for kNone and kTrace.
		std::span<const Line> NameOf(Part a_part)
		{
			const auto index = static_cast<std::size_t>(a_part);
			const auto play = PlayParts();
			const auto ui = UiParts();
			const auto firstPlay = static_cast<std::size_t>(Part::kPerks);
			const auto firstUi = static_cast<std::size_t>(Part::kCardCnd);
			if (index >= firstPlay && index < firstPlay + play.size()) {
				return play[index - firstPlay].name;
			}
			if (index >= firstUi && index < firstUi + ui.size()) {
				return ui[index - firstUi].name;
			}
			return {};
		}
	}

	const char* Pick(std::span<const Line> a_lines)
	{
		const auto language = Language();
		for (const auto& line : a_lines) {
			if (line.code == language) {
				return line.text;
			}
		}
		return a_lines.front().text;
	}

	const char* PickFor(std::span<const Line> a_lines, Out a_out)
	{
		return a_out == Out::kLog ? a_lines.front().text : Pick(a_lines);
	}

	std::string Language()
	{
		// Read from the game's own setting at every call. The plugin loads before
		// the game reads Fallout4.ini, and until then it holds its default, en.
		// Read by ID, so a call does not search every INI setting by name.
		static const REL::Relocation<RE::Setting*> setting{ RE::ID::Setting::sLanguage };
		std::string out{ setting.get()->GetString() };
		std::transform(out.begin(), out.end(), out.begin(),
			[](unsigned char a_ch) { return static_cast<char>(std::tolower(a_ch)); });
		return out.empty() ? std::string{ "en" } : out;
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
		const auto help = a_id.ends_with(HELP);
		const auto key = help ? a_id.substr(0, a_id.size() - HELP.size()) : a_id;
		for (const auto rows : { MenuSwitches(), MenuNumbers(), MenuHudLog() }) {
			for (const auto& row : rows) {
				if (row.setting->key != key) {
					continue;
				}
				if (help) {
					return Pick(row.help);
				}
				return row.name.empty() ? PartName(PartOf(*row.setting)) : Pick(row.name);
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
		return name.empty() ? std::string{} : Pick(name);
	}

	std::string_view PartLogName(Part a_part)
	{
		const auto name = NameOf(a_part);
		return name.empty() ? std::string_view{} : std::string_view{ name.front().text };
	}

	std::string SettingLogName(const Settings::Named& a_setting)
	{
		for (const auto rows : { MenuSwitches(), MenuNumbers(), MenuHudLog() }) {
			for (const auto& row : rows) {
				if (row.setting == &a_setting) {
					return std::string{ row.name.empty() ? PartLogName(PartOf(a_setting)) : std::string_view{ row.name.front().text } };
				}
			}
		}
		return std::string{ a_setting.key };
	}
}
