#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the wear speeds, the first Balance numbers on
// the MCM page, see MenuLine in Text.h. Each NPC row follows the player's,
// and the automatic weapon row follows both weapon rows.
namespace Text
{
	namespace
	{
		constexpr Line WEAR_RATE_NAME[]{
			{ "en", "Weapon wear speed" },
			{ "fr", "Vitesse d'usure des armes" },
			{ "de", "Abnutzungstempo von Waffen" },
			{ "it", "Velocità di usura delle armi" },
			{ "es", "Velocidad de desgaste de armas" },
			{ "esmx", "Velocidad de desgaste de armas" },
			{ "ptbr", "Velocidade de desgaste das armas" },
			{ "pl", "Szybkość zużycia broni" },
			{ "ru", "Скорость износа оружия" },
			{ "ja", "武器の消耗速度" },
			{ "zhhant", "武器損耗速度" },
			{ "zhhans", "武器损耗速度" },
		};

		constexpr Line WEAR_RATE_HELP[]{
			{ "en", "How fast your weapons wear. 1 is normal, 2 is twice as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure de vos armes. 1 est normal, 2 est 2 fois plus rapide et 0 arrête l'usure." },
			{ "de", "Wie schnell sich deine Waffen abnutzen. 1 ist normal, 2 doppelt so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le tue armi. 1 è normale, 2 è il doppio e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan tus armas. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan tus armas. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as suas armas se desgastam. 1 é o normal, 2 é o dobro e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywa się twoja broń. 1 to normalnie, 2 to 2 razy szybciej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается ваше оружие. 1 означает обычную скорость, 2 вдвое быстрее, а 0 останавливает износ." },
			{ "ja", "自分の武器が消耗する速さです。1で通常、2で2倍の速さ、0で消耗しなくなります" },
			{ "zhhant", "你的武器損耗的速度。1為正常，2為2倍速度，0則停止損耗。" },
			{ "zhhans", "你的武器损耗的速度。1为正常，2为2倍速度，0则停止损耗。" },
		};

		constexpr Line NPC_WEAR_RATE_NAME[]{
			{ "en", "NPC weapon wear speed" },
			{ "fr", "Vitesse d'usure des armes des PNJ" },
			{ "de", "Abnutzungstempo von NPC-Waffen" },
			{ "it", "Velocità di usura delle armi dei PNG" },
			{ "es", "Velocidad de desgaste de armas de otros personajes" },
			{ "esmx", "Velocidad de desgaste de armas de otros personajes" },
			{ "ptbr", "Velocidade de desgaste das armas dos NPCs" },
			{ "pl", "Szybkość zużycia broni NPC" },
			{ "ru", "Скорость износа оружия NPC" },
			{ "ja", "NPCの武器の消耗速度" },
			{ "zhhant", "其他角色的武器損耗速度" },
			{ "zhhans", "其他角色的武器损耗速度" },
		};

		constexpr Line NPC_WEAR_RATE_HELP[]{
			{ "en", "How fast NPC weapons wear. 1 is normal, 0.33 is a third as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure des armes des PNJ. 1 est normal, 0.33 est 3 fois plus lent et 0 arrête l'usure." },
			{ "de", "Wie schnell sich NPC-Waffen abnutzen. 1 ist normal, 0.33 ein Drittel so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le armi dei PNG. 1 è normale, 0.33 è un terzo e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan las armas de otros personajes. 1 es normal, 0.33 es 3 veces más lento y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan las armas de otros personajes. 1 es normal, 0.33 es 3 veces más lento y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as armas dos NPCs se desgastam. 1 é o normal, 0.33 é um terço da velocidade e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywa się broń NPC. 1 to normalnie, 0.33 to 3 razy wolniej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается оружие NPC. 1 означает обычную скорость, 0.33 втрое медленнее, а 0 останавливает износ." },
			{ "ja", "NPCの武器が消耗する速さです。1で通常、0.33で3分の1の速さ、0で消耗しなくなります" },
			{ "zhhant", "其他角色的武器損耗的速度。1為正常，0.33為三分之一速度，0則停止損耗。" },
			{ "zhhans", "其他角色的武器损耗的速度。1为正常，0.33为三分之一速度，0则停止损耗。" },
		};

		constexpr Line AUTO_WEAR_RATE_NAME[]{
			{ "en", "Automatic weapon wear speed" },
			{ "fr", "Vitesse d'usure des armes automatiques" },
			{ "de", "Abnutzungstempo von Automatikwaffen" },
			{ "it", "Velocità di usura delle armi automatiche" },
			{ "es", "Velocidad de desgaste de armas automáticas" },
			{ "esmx", "Velocidad de desgaste de armas automáticas" },
			{ "ptbr", "Velocidade de desgaste das armas automáticas" },
			{ "pl", "Szybkość zużycia broni automatycznej" },
			{ "ru", "Скорость износа автоматического оружия" },
			{ "ja", "オートマチック武器の消耗速度" },
			{ "zhhant", "自動武器損耗速度" },
			{ "zhhans", "自动武器损耗速度" },
		};

		constexpr Line AUTO_WEAR_RATE_HELP[]{
			{ "en", "How fast automatic weapons wear as they fire or cut, on top of the weapon wear speeds above. 1 is normal, 0.5 is half as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure des armes automatiques au tir ou à la coupe, en plus des vitesses d'usure des armes ci-dessus. 1 est normal, 0.5 est 2 fois plus lent et 0 arrête l'usure." },
			{ "de", "Wie schnell sich Automatikwaffen beim Schießen oder Schneiden abnutzen, zusätzlich zu den Waffentempos oben. 1 ist normal, 0.5 halb so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le armi automatiche sparando o tagliando, oltre alle velocità delle armi qui sopra. 1 è normale, 0.5 è la metà e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan las armas automáticas al disparar o cortar, además de las velocidades de armas de arriba. 1 es normal, 0.5 es la mitad y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan las armas automáticas al disparar o cortar, además de las velocidades de armas de arriba. 1 es normal, 0.5 es la mitad y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as armas automáticas se desgastam ao disparar ou cortar, além das velocidades das armas acima. 1 é o normal, 0.5 é a metade e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywa się broń automatyczna przy strzelaniu lub cięciu, na dodatek do szybkości broni powyżej. 1 to normalnie, 0.5 to 2 razy wolniej, a 0 wyłącza zużycie." },
			{ "ru", "Скорость износа автоматического оружия от стрельбы и резки, поверх скоростей выше. 1 обычная, 0.5 вдвое медленнее, 0 без износа." },
			{ "ja", "オートマチック武器が射撃や切断で消耗する速さで、上の武器の消耗速度に掛け合わされます。1で通常、0.5で半分の速さ、0で消耗しなくなります" },
			{ "zhhant", "自動武器射擊或切割時損耗的速度，疊加在上方的武器損耗速度之上。1為正常，0.5為一半速度，0則停止損耗。" },
			{ "zhhans", "自动武器射击或切割时损耗的速度，叠加在上方的武器损耗速度之上。1为正常，0.5为一半速度，0则停止损耗。" },
		};

		constexpr Line ARMOR_WEAR_RATE_NAME[]{
			{ "en", "Armor wear speed" },
			{ "fr", "Vitesse d'usure des armures" },
			{ "de", "Abnutzungstempo von Rüstungen" },
			{ "it", "Velocità di usura delle armature" },
			{ "es", "Velocidad de desgaste de armaduras" },
			{ "esmx", "Velocidad de desgaste de armaduras" },
			{ "ptbr", "Velocidade de desgaste das armaduras" },
			{ "pl", "Szybkość zużycia pancerzy" },
			{ "ru", "Скорость износа брони" },
			{ "ja", "アーマーの消耗速度" },
			{ "zhhant", "裝甲損耗速度" },
			{ "zhhans", "装甲损耗速度" },
		};

		constexpr Line ARMOR_WEAR_RATE_HELP[]{
			{ "en", "How fast your armor wears. 1 is normal, 2 is twice as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure de vos armures. 1 est normal, 2 est 2 fois plus rapide et 0 arrête l'usure." },
			{ "de", "Wie schnell sich deine Rüstung abnutzt. 1 ist normal, 2 doppelt so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le tue armature. 1 è normale, 2 è il doppio e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgasta tu armadura. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgasta tu armadura. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido a sua armadura se desgasta. 1 é o normal, 2 é o dobro e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywa się twój pancerz. 1 to normalnie, 2 to 2 razy szybciej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается ваша броня. 1 означает обычную скорость, 2 вдвое быстрее, а 0 останавливает износ." },
			{ "ja", "自分のアーマーが消耗する速さです。1で通常、2で2倍の速さ、0で消耗しなくなります" },
			{ "zhhant", "你的裝甲損耗的速度。1為正常，2為2倍速度，0則停止損耗。" },
			{ "zhhans", "你的装甲损耗的速度。1为正常，2为2倍速度，0则停止损耗。" },
		};

		constexpr Line NPC_ARMOR_WEAR_RATE_NAME[]{
			{ "en", "NPC armor wear speed" },
			{ "fr", "Vitesse d'usure des armures des PNJ" },
			{ "de", "Abnutzungstempo von NPC-Rüstungen" },
			{ "it", "Velocità di usura delle armature dei PNG" },
			{ "es", "Velocidad de desgaste de armaduras de otros personajes" },
			{ "esmx", "Velocidad de desgaste de armaduras de otros personajes" },
			{ "ptbr", "Velocidade de desgaste das armaduras dos NPCs" },
			{ "pl", "Szybkość zużycia pancerzy NPC" },
			{ "ru", "Скорость износа брони NPC" },
			{ "ja", "NPCのアーマーの消耗速度" },
			{ "zhhant", "其他角色的裝甲損耗速度" },
			{ "zhhans", "其他角色的装甲损耗速度" },
		};

		constexpr Line NPC_ARMOR_WEAR_RATE_HELP[]{
			{ "en", "How fast NPC armor wears. 1 is normal, 0.33 is a third as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure des armures des PNJ. 1 est normal, 0.33 est 3 fois plus lent et 0 arrête l'usure." },
			{ "de", "Wie schnell sich NPC-Rüstungen abnutzen. 1 ist normal, 0.33 ein Drittel so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le armature dei PNG. 1 è normale, 0.33 è un terzo e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan las armaduras de otros personajes. 1 es normal, 0.33 es 3 veces más lento y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan las armaduras de otros personajes. 1 es normal, 0.33 es 3 veces más lento y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as armaduras dos NPCs se desgastam. 1 é o normal, 0.33 é um terço da velocidade e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywają się pancerze NPC. 1 to normalnie, 0.33 to 3 razy wolniej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается броня NPC. 1 означает обычную скорость, 0.33 втрое медленнее, а 0 останавливает износ." },
			{ "ja", "NPCのアーマーが消耗する速さです。1で通常、0.33で3分の1の速さ、0で消耗しなくなります" },
			{ "zhhant", "其他角色的裝甲損耗的速度。1為正常，0.33為三分之一速度，0則停止損耗。" },
			{ "zhhans", "其他角色的装甲损耗的速度。1为正常，0.33为三分之一速度，0则停止损耗。" },
		};

		constexpr MenuRow ROWS[]{
			{ &Settings::fWearRateMult, WEAR_RATE_NAME, WEAR_RATE_HELP },
			{ &Settings::fNpcWearRateMult, NPC_WEAR_RATE_NAME, NPC_WEAR_RATE_HELP },
			{ &Settings::fAutoWearRateMult, AUTO_WEAR_RATE_NAME, AUTO_WEAR_RATE_HELP },
			{ &Settings::fArmorWearRateMult, ARMOR_WEAR_RATE_NAME, ARMOR_WEAR_RATE_HELP },
			{ &Settings::fNpcArmorWearRateMult, NPC_ARMOR_WEAR_RATE_NAME, NPC_ARMOR_WEAR_RATE_HELP },
		};
	}

	std::span<const MenuRow> MenuWear()
	{
		return ROWS;
	}
}
