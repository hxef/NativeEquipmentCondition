#include "Core/Text/Lines.h"

#include "Core/Parts.h"

#include <cstddef>
#include <iterator>

// The names of the parts of the Condition and Gameplay rows, see Core/Parts.h.
namespace Text
{
	namespace
	{
		// The repair discount line NEC adds to each crafting perk's
		// description.
		constexpr Line PART_PERKS[]{
			{ "en", "Perk repair discount text" },
			{ "fr", "Remise de réparation dans le texte des aptitudes" },
			{ "de", "Reparaturrabatt in Skilltexten" },
			{ "it", "Sconto riparazioni nel testo dei talenti" },
			{ "es", "Texto del descuento de reparación en extras" },
			{ "esmx", "Texto del descuento de reparación en extras" },
			{ "ptbr", "Desconto de conserto no texto das vantagens" },
			{ "pl", "Zniżka na naprawy w opisach profitów" },
			{ "ru", "Скидка на ремонт в описаниях способностей" },
			{ "ja", "Perk説明の修理割引" },
			{ "zhhant", "輔助能力修理折扣說明" },
			{ "zhhans", "辅助能力修理折扣说明" },
		};

		// Every shot wears the gun. Melee and bashes wear it through a hit
		// event no mod can take.
		constexpr Line PART_GUN_WEAR[]{
			{ "en", "Gun wear from firing" },
			{ "fr", "Usure au tir" },
			{ "de", "Abnutzung durch Schüsse" },
			{ "it", "Usura da sparo" },
			{ "es", "Desgaste de armas al disparar" },
			{ "esmx", "Desgaste de armas al disparar" },
			{ "ptbr", "Desgaste ao disparar" },
			{ "pl", "Zużycie broni od strzałów" },
			{ "ru", "Износ оружия при стрельбе" },
			{ "ja", "射撃による銃の消耗" },
			{ "zhhant", "射擊造成的槍枝損耗" },
			{ "zhhans", "射击造成的枪支损耗" },
		};

		constexpr Line PART_DAMAGE[]{
			{ "en", "Worn weapon damage" },
			{ "fr", "Dégâts des armes usées" },
			{ "de", "Schaden abgenutzter Waffen" },
			{ "it", "Danni delle armi usurate" },
			{ "es", "Daño de armas desgastadas" },
			{ "esmx", "Daño de armas desgastadas" },
			{ "ptbr", "Dano de armas desgastadas" },
			{ "pl", "Obrażenia zużytej broni" },
			{ "ru", "Урон изношенного оружия" },
			{ "ja", "消耗した武器のダメージ" },
			{ "zhhant", "損耗武器的傷害" },
			{ "zhhans", "损耗武器的伤害" },
		};

		// The damage on item cards and in the Pip-Boy.
		constexpr Line PART_CARD_DAMAGE[]{
			{ "en", "Damage shown in menus" },
			{ "fr", "Dégâts affichés dans les menus" },
			{ "de", "Schadensanzeige in Menüs" },
			{ "it", "Danni mostrati nei menu" },
			{ "es", "Daño en los menús" },
			{ "esmx", "Daño en los menús" },
			{ "ptbr", "Dano exibido nos menus" },
			{ "pl", "Obrażenia widoczne w menu" },
			{ "ru", "Урон в меню" },
			{ "ja", "メニューのダメージ表示" },
			{ "zhhant", "選單中顯示的傷害" },
			{ "zhhans", "菜单中显示的伤害" },
		};

		constexpr Line PART_ARMOR[]{
			{ "en", "Worn armor protection" },
			{ "fr", "Protection des armures usées" },
			{ "de", "Schutz abgenutzter Rüstung" },
			{ "it", "Protezione delle armature usurate" },
			{ "es", "Protección de armaduras desgastadas" },
			{ "esmx", "Protección de armaduras desgastadas" },
			{ "ptbr", "Proteção de armaduras desgastadas" },
			{ "pl", "Ochrona zużytego pancerza" },
			{ "ru", "Защита изношенной брони" },
			{ "ja", "消耗したアーマーの防御力" },
			{ "zhhant", "損耗裝甲的防護" },
			{ "zhhans", "损耗装甲的防护" },
		};

		// The resistances on item cards, in the Pip-Boy and on the paper doll.
		constexpr Line PART_CARD_ARMOR[]{
			{ "en", "Protection shown in menus" },
			{ "fr", "Protection affichée dans les menus" },
			{ "de", "Schutzanzeige in Menüs" },
			{ "it", "Protezione mostrata nei menu" },
			{ "es", "Protección en los menús" },
			{ "esmx", "Protección en los menús" },
			{ "ptbr", "Proteção exibida nos menus" },
			{ "pl", "Ochrona widoczna w menu" },
			{ "ru", "Защита в меню" },
			{ "ja", "メニューの防御力表示" },
			{ "zhhant", "選單中顯示的防護" },
			{ "zhhans", "菜单中显示的防护" },
		};

		// A broken weapon is put away, and broken gear cannot be equipped.
		constexpr Line PART_BROKEN[]{
			{ "en", "Broken gear rules" },
			{ "fr", "Règles de l'équipement brisé" },
			{ "de", "Regeln für kaputte Ausrüstung" },
			{ "it", "Regole sull'equipaggiamento rotto" },
			{ "es", "Reglas del equipo roto" },
			{ "esmx", "Reglas del equipo roto" },
			{ "ptbr", "Regras de equipamento quebrado" },
			{ "pl", "Zasady dla zepsutego sprzętu" },
			{ "ru", "Правила для сломанного снаряжения" },
			{ "ja", "壊れた装備のルール" },
			{ "zhhant", "損壞裝備規則" },
			{ "zhhans", "损坏装备规则" },
		};

		constexpr Line PART_PRICES[]{
			{ "en", "Worn item prices" },
			{ "fr", "Prix des objets usés" },
			{ "de", "Preise abgenutzter Gegenstände" },
			{ "it", "Prezzi degli oggetti usurati" },
			{ "es", "Precio de objetos desgastados" },
			{ "esmx", "Precio de objetos desgastados" },
			{ "ptbr", "Preços de itens desgastados" },
			{ "pl", "Ceny zużytych przedmiotów" },
			{ "ru", "Цены изношенных предметов" },
			{ "ja", "消耗したアイテムの価格" },
			{ "zhhant", "損耗物品價格" },
			{ "zhhans", "损耗物品价格" },
		};

		constexpr Line PART_FIRE_RATE[]{
			{ "en", "Slower fire when worn" },
			{ "fr", "Cadence de tir réduite par l'usure" },
			{ "de", "Geringere Feuerrate bei Abnutzung" },
			{ "it", "Cadenza ridotta dall'usura" },
			{ "es", "Menor cadencia de tiro por desgaste" },
			{ "esmx", "Menor cadencia de tiro por desgaste" },
			{ "ptbr", "Cadência de tiro menor com desgaste" },
			{ "pl", "Niższa szybkostrzelność przy zużyciu" },
			{ "ru", "Меньшая скорострельность при износе" },
			{ "ja", "消耗による発射速度の低下" },
			{ "zhhant", "損耗降低射速" },
			{ "zhhans", "损耗降低射速" },
		};

		// A worn automatic weapon's firing sound slows with it.
		constexpr Line PART_FIRE_SOUND[]{
			{ "en", "Slower firing sound" },
			{ "fr", "Son de tir ralenti" },
			{ "de", "Langsameres Schussgeräusch" },
			{ "it", "Suono di sparo rallentato" },
			{ "es", "Sonido de disparo más lento" },
			{ "esmx", "Sonido de disparo más lento" },
			{ "ptbr", "Som de disparo mais lento" },
			{ "pl", "Wolniejszy dźwięk strzałów" },
			{ "ru", "Замедленный звук стрельбы" },
			{ "ja", "発射音の低速化" },
			{ "zhhant", "射擊聲變慢" },
			{ "zhhans", "射击声变慢" },
		};

		constexpr Line PART_CRIT_METER[]{
			{ "en", "Fewer criticals when worn" },
			{ "fr", "Moins de coups critiques avec l'usure" },
			{ "de", "Weniger kritische Treffer bei Abnutzung" },
			{ "it", "Meno critici con l'usura" },
			{ "es", "Menos impactos críticos por desgaste" },
			{ "esmx", "Menos impactos críticos por desgaste" },
			{ "ptbr", "Menos acertos críticos com desgaste" },
			{ "pl", "Mniej trafień krytycznych przy zużyciu" },
			{ "ru", "Меньше критических атак при износе" },
			{ "ja", "消耗によるクリティカルの減少" },
			{ "zhhant", "損耗減少爆擊" },
			{ "zhhans", "损耗减少爆击" },
		};

		// An NPC's worn weapon lands fewer critical hits.
		constexpr Line PART_NPC_CRITS[]{
			{ "en", "Fewer NPC criticals" },
			{ "fr", "Moins de coups critiques des PNJ" },
			{ "de", "Weniger kritische NPC-Treffer" },
			{ "it", "Meno critici dei PNG" },
			{ "es", "Menos críticos de otros personajes" },
			{ "esmx", "Menos críticos de otros personajes" },
			{ "ptbr", "Menos acertos críticos de NPCs" },
			{ "pl", "Mniej trafień krytycznych NPC" },
			{ "ru", "Меньше критических атак у NPC" },
			{ "ja", "NPCのクリティカル減少" },
			{ "zhhant", "其他角色爆擊減少" },
			{ "zhhans", "其他角色爆击减少" },
		};

		constexpr Line PART_JAM[]{
			{ "en", "Jamming" },
			{ "fr", "Enrayage" },
			{ "de", "Ladehemmungen" },
			{ "it", "Inceppamenti" },
			{ "es", "Encasquillamientos" },
			{ "esmx", "Encasquillamientos" },
			{ "ptbr", "Emperramento" },
			{ "pl", "Zacinanie się broni" },
			{ "ru", "Заклинивание" },
			{ "ja", "弾詰まり" },
			{ "zhhant", "卡彈" },
			{ "zhhans", "卡弹" },
		};

		// A gun that fires once per reload rolls its jam as the reload ends.
		constexpr Line PART_RELOAD_JAM[]{
			{ "en", "Single shot gun jams" },
			{ "fr", "Enrayage des armes monocoup" },
			{ "de", "Ladehemmungen bei Einzelladern" },
			{ "it", "Inceppamenti armi a colpo singolo" },
			{ "es", "Encasquillamientos en armas de un solo disparo" },
			{ "esmx", "Encasquillamientos en armas de un solo disparo" },
			{ "ptbr", "Emperramento de armas de tiro único" },
			{ "pl", "Zacięcia broni jednostrzałowej" },
			{ "ru", "Заклинивание однозарядного оружия" },
			{ "ja", "単発銃の弾詰まり" },
			{ "zhhant", "單發槍枝卡彈" },
			{ "zhhans", "单发枪支卡弹" },
		};

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

		// A stack a save from before NEC kept together splits up, so each
		// item has its own condition.
		constexpr Line PART_OLD_SAVES[]{
			{ "en", "Old save stacks split up" },
			{ "fr", "Séparation des piles d'anciennes sauvegardes" },
			{ "de", "Stapel alter Spielstände aufteilen" },
			{ "it", "Pile divise nei vecchi salvataggi" },
			{ "es", "Desapilar objetos de partidas antiguas" },
			{ "esmx", "Desapilar objetos de partidas antiguas" },
			{ "ptbr", "Pilhas de jogos salvos antigos se dividem" },
			{ "pl", "Podział stosów ze starych zapisów" },
			{ "ru", "Разделение стопок из старых сохранений" },
			{ "ja", "古いセーブのスタック分割" },
			{ "zhhant", "舊存檔堆疊拆分" },
			{ "zhhans", "旧存档堆叠拆分" },
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
			{ Part::kPerks, PART_PERKS },
			{ Part::kGunWear, PART_GUN_WEAR },
			{ Part::kDamage, PART_DAMAGE },
			{ Part::kCardDamage, PART_CARD_DAMAGE },
			{ Part::kArmor, PART_ARMOR },
			{ Part::kCardArmor, PART_CARD_ARMOR },
			{ Part::kBroken, PART_BROKEN },
			{ Part::kPrices, PART_PRICES },
			{ Part::kFireRate, PART_FIRE_RATE },
			{ Part::kFireSound, PART_FIRE_SOUND },
			{ Part::kCritMeter, PART_CRIT_METER },
			{ Part::kNpcCrits, PART_NPC_CRITS },
			{ Part::kJam, PART_JAM },
			{ Part::kReloadJam, PART_RELOAD_JAM },
			{ Part::kSpawn, PART_SPAWN },
			{ Part::kConsoleNew, PART_CONSOLE_NEW },
			{ Part::kOldSaves, PART_OLD_SAVES },
			{ Part::kGifts, PART_GIFTS },
		};

		// 1 row a part, in the enum's order from kPerks to kGifts, so a part's
		// name is found by its place.
		consteval bool InOrder()
		{
			const auto first = static_cast<std::size_t>(Part::kPerks);
			for (std::size_t i = 0; i < std::size(PARTS); i++) {
				if (static_cast<std::size_t>(PARTS[i].part) != first + i) {
					return false;
				}
			}
			return std::size(PARTS) == static_cast<std::size_t>(Part::kGifts) - first + 1;
		}
		static_assert(InOrder());
	}

	std::span<const PartRow> PlayParts()
	{
		return PARTS;
	}
}
