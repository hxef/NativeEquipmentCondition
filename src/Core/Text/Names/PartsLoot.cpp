#include "Core/Text/Lines.h"

#include "Core/Parts.h"

// The names of the Worn loot parts, from Worn loot to Quest rewards arrive new,
// see Core/Parts.h.
namespace Text
{
	namespace
	{
		constexpr Line PART_SPAWN[]{
			{ "en", "Worn loot" },
			{ "fr", "Butin usé" },
			{ "de", "Abgenutzte Beute" },
			{ "it", "Bottino usurato" },
			{ "es", "Botín desgastado" },
			{ "esmx", "Botín desgastado" },
			{ "ptbr", "Saque desgastado" },
			{ "pl", "Zużyte łupy" },
			{ "ru", "Изношенные трофеи" },
			{ "ja", "消耗した戦利品" },
			{ "zhhant", "戰利品損耗" },
			{ "zhhans", "战利品损耗" },
		};

		// Weapons and armor the console adds arrive at full condition.
		constexpr Line PART_CONSOLE_NEW[]{
			{ "en", "Console items arrive new" },
			{ "fr", "Objets neufs via la console" },
			{ "de", "Konsolengegenstände sind neu" },
			{ "it", "Oggetti da console intatti" },
			{ "es", "Objetos de consola como nuevos" },
			{ "esmx", "Objetos de consola como nuevos" },
			{ "ptbr", "Itens do console chegam novos" },
			{ "pl", "Przedmioty z konsoli są nowe" },
			{ "ru", "Предметы из консоли без износа" },
			{ "ja", "コンソールで出したアイテムは新品" },
			{ "zhhant", "主控台物品為全新" },
			{ "zhhans", "控制台物品为全新" },
		};

		// Weapons and armor lying in the world roll when their first owner
		// picks them up.
		constexpr Line PART_WORLD_LOOT[]{
			{ "en", "Loot lying around" },
			{ "fr", "Butin qui traîne" },
			{ "de", "Herumliegende Beute" },
			{ "it", "Bottino sparso" },
			{ "es", "Botín tirado por ahí" },
			{ "esmx", "Botín tirado por ahí" },
			{ "ptbr", "Saque largado por aí" },
			{ "pl", "Łupy leżące na ziemi" },
			{ "ru", "Валяющиеся трофеи" },
			{ "ja", "落ちている戦利品" },
			{ "zhhant", "散落的戰利品" },
			{ "zhhans", "散落的战利品" },
		};

		// Weapons and armor with no mod slots roll as a container or a
		// character is stocked.
		constexpr Line PART_MODLESS_LOOT[]{
			{ "en", "Loot with no mod slots" },
			{ "fr", "Butin sans emplacement de modification" },
			{ "de", "Beute ohne Mod-Plätze" },
			{ "it", "Bottino senza slot per modifiche" },
			{ "es", "Botín sin ranuras de modificación" },
			{ "esmx", "Botín sin ranuras de modificación" },
			{ "ptbr", "Saque sem espaços para modificações" },
			{ "pl", "Łupy bez miejsc na modyfikacje" },
			{ "ru", "Трофеи без слотов модификаций" },
			{ "ja", "改造スロットのない戦利品" },
			{ "zhhant", "無改造欄位的戰利品" },
			{ "zhhans", "无改造栏位的战利品" },
		};

		// Quest rewards and gifts a script hands the player arrive at full
		// condition.
		constexpr Line PART_GIFTS[]{
			{ "en", "Quest rewards arrive new" },
			{ "fr", "Récompenses de quête neuves" },
			{ "de", "Questbelohnungen sind neu" },
			{ "it", "Ricompense delle missioni intatte" },
			{ "es", "Recompensas de misión como nuevas" },
			{ "esmx", "Recompensas de misión como nuevas" },
			{ "ptbr", "Recompensas de missão chegam novas" },
			{ "pl", "Nagrody za zadania są nowe" },
			{ "ru", "Награды за задания без износа" },
			{ "ja", "クエスト報酬は新品" },
			{ "zhhant", "任務獎勵為全新" },
			{ "zhhans", "任务奖励为全新" },
		};

		constexpr PartRow PARTS[]{
			{ Part::kSpawn, PART_SPAWN },
			{ Part::kConsoleNew, PART_CONSOLE_NEW },
			{ Part::kWorldLoot, PART_WORLD_LOOT },
			{ Part::kModlessLoot, PART_MODLESS_LOOT },
			{ Part::kGifts, PART_GIFTS },
		};

		static_assert(PartsInOrder(PARTS, Part::kSpawn, Part::kGifts));
	}

	std::span<const PartRow> LootParts()
	{
		return PARTS;
	}
}
