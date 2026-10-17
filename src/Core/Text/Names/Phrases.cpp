#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// What a setting's sentence says in place of a piece's name, after "no
// effect on" and after "Still changes" or "Still works for", see
// SettingLines in Menu/MenuPieces.cpp. A phrase names what its piece
// changes, such as the fire rate of NPC guns and not the guns alone, since
// Weapon penalties below names many pieces in 1 sentence.
namespace Text
{
	namespace
	{
		// -------------------------------------------------------------------
		// Wear, fire rate and crits
		// -------------------------------------------------------------------

		constexpr Line PHRASE_GUN_WEAR[]{
			{ "en", "gun wear from firing" },
			{ "fr", "l'usure au tir" },
			{ "de", "Waffenabnutzung durch Schüsse" },
			{ "it", "l'usura da sparo" },
			{ "es", "el desgaste de armas al disparar" },
			{ "esmx", "el desgaste de armas al disparar" },
			{ "ptbr", "o desgaste ao disparar" },
			{ "pl", "zużycie broni od strzałów" },
			{ "ru", "износ оружия от стрельбы" },
			{ "ja", "射撃による銃の消耗" },
			{ "zhhant", "射擊造成的槍枝損耗" },
			{ "zhhans", "射击造成的枪支损耗" },
		};

		constexpr Line PHRASE_FIRE_RATE[]{
			{ "en", "the fire rate of your guns and blades" },
			{ "fr", "la cadence de vos armes à feu et de vos lames" },
			{ "de", "die Feuerrate deiner Schusswaffen und Klingen" },
			{ "it", "la cadenza delle tue armi da fuoco e lame" },
			{ "es", "la cadencia de tus armas de fuego y armas blancas" },
			{ "esmx", "la cadencia de tus armas de fuego y armas blancas" },
			{ "ptbr", "a cadência das suas armas de fogo e lâminas" },
			{ "pl", "szybkostrzelność twojej broni palnej i ostrzy" },
			{ "ru", "скорострельность вашего стрелкового оружия и клинков" },
			{ "ja", "自分の銃と刃物の発射速度" },
			{ "zhhant", "你的槍枝和刀刃的射速" },
			{ "zhhans", "你的枪支和刀刃的射速" },
		};

		constexpr Line PHRASE_NPC_GUNS[]{
			{ "en", "the fire rate of NPC guns" },
			{ "fr", "la cadence des armes à feu des PNJ" },
			{ "de", "die Feuerrate von NPC-Schusswaffen" },
			{ "it", "la cadenza delle armi da fuoco dei PNG" },
			{ "es", "la cadencia de las armas de fuego de otros personajes" },
			{ "esmx", "la cadencia de las armas de fuego de otros personajes" },
			{ "ptbr", "a cadência das armas de fogo dos NPCs" },
			{ "pl", "szybkostrzelność broni palnej NPC" },
			{ "ru", "скорострельность стрелкового оружия NPC" },
			{ "ja", "NPCの銃の発射速度" },
			{ "zhhant", "其他角色槍枝的射速" },
			{ "zhhans", "其他角色枪支的射速" },
		};

		constexpr Line PHRASE_FIRE_SOUND[]{
			{ "en", "the firing sound" },
			{ "fr", "le son de tir" },
			{ "de", "das Schussgeräusch" },
			{ "it", "il suono di sparo" },
			{ "es", "el sonido de disparo" },
			{ "esmx", "el sonido de disparo" },
			{ "ptbr", "o som de disparo" },
			{ "pl", "dźwięk strzałów" },
			{ "ru", "звук стрельбы" },
			{ "ja", "発射音" },
			{ "zhhant", "射擊聲" },
			{ "zhhans", "射击声" },
		};

		constexpr Line PHRASE_CRIT_METER[]{
			{ "en", "your critical meter" },
			{ "fr", "votre jauge de critique" },
			{ "de", "deinen Krit-Balken" },
			{ "it", "la tua barra dei critici" },
			{ "es", "tu medidor de críticos" },
			{ "esmx", "tu medidor de críticos" },
			{ "ptbr", "sua barra de crítico" },
			{ "pl", "twój wskaźnik trafień krytycznych" },
			{ "ru", "ваша шкала КРИТ" },
			{ "ja", "自分のクリティカルメーター" },
			{ "zhhant", "你的爆擊條" },
			{ "zhhans", "你的爆击条" },
		};

		constexpr Line PHRASE_NPC_CRITS[]{
			{ "en", "NPC criticals" },
			{ "fr", "les critiques des PNJ" },
			{ "de", "kritische NPC-Treffer" },
			{ "it", "i critici dei PNG" },
			{ "es", "los críticos de otros personajes" },
			{ "esmx", "los críticos de otros personajes" },
			{ "ptbr", "os críticos dos NPCs" },
			{ "pl", "trafienia krytyczne NPC" },
			{ "ru", "критические атаки NPC" },
			{ "ja", "NPCのクリティカル" },
			{ "zhhant", "其他角色的爆擊" },
			{ "zhhans", "其他角色的爆击" },
		};

		// -------------------------------------------------------------------
		// Accuracy, range and jams
		// -------------------------------------------------------------------

		constexpr Line PHRASE_ACCURACY[]{
			{ "en", "the spread of shots" },
			{ "fr", "la dispersion des tirs" },
			{ "de", "die Streuung der Schüsse" },
			{ "it", "la dispersione dei colpi" },
			{ "es", "la dispersión de los disparos" },
			{ "esmx", "la dispersión de los disparos" },
			{ "ptbr", "a dispersão dos disparos" },
			{ "pl", "rozrzut strzałów" },
			{ "ru", "разброс выстрелов" },
			{ "ja", "射撃のばらつき" },
			{ "zhhant", "射擊的散布" },
			{ "zhhans", "射击的散布" },
		};

		constexpr Line PHRASE_CROSSHAIR[]{
			{ "en", "the crosshair" },
			{ "fr", "le réticule" },
			{ "de", "das Fadenkreuz" },
			{ "it", "il mirino" },
			{ "es", "el punto de mira" },
			{ "esmx", "el punto de mira" },
			{ "ptbr", "o retículo" },
			{ "pl", "celownik" },
			{ "ru", "прицел" },
			{ "ja", "照準マーカー" },
			{ "zhhant", "準星" },
			{ "zhhans", "准星" },
		};

		constexpr Line PHRASE_VATS_ACCURACY[]{
			{ "en", "the VATS hit chance" },
			{ "fr", "les chances de toucher en SVAV" },
			{ "de", "die V.A.T.S.-Trefferchance" },
			{ "it", "la probabilità di colpire nello S.P.A.V." },
			{ "es", "la probabilidad de impacto en V.A.T.S." },
			{ "esmx", "la probabilidad de impacto en V.A.T.S." },
			{ "ptbr", "a chance de acerto no VATS" },
			{ "pl", "szansa trafienia w V.A.T.S." },
			{ "ru", "шанс попадания в VATS" },
			{ "ja", "V.A.T.S.の命中率" },
			{ "zhhant", "VATS命中率" },
			{ "zhhans", "VATS命中率" },
		};

		constexpr Line PHRASE_RANGE[]{
			{ "en", "damage at range" },
			{ "fr", "les dégâts à distance" },
			{ "de", "den Schaden auf Distanz" },
			{ "it", "i danni a distanza" },
			{ "es", "el daño a distancia" },
			{ "esmx", "el daño a distancia" },
			{ "ptbr", "o dano à distância" },
			{ "pl", "obrażenia na dystans" },
			{ "ru", "урон на дистанции" },
			{ "ja", "遠距離のダメージ" },
			{ "zhhant", "遠距離傷害" },
			{ "zhhans", "远距离伤害" },
		};

		// Every gun but those that fire once per reload.
		constexpr Line PHRASE_JAM_SHOT[]{
			{ "en", "jams of all other guns" },
			{ "fr", "l'enrayage de toutes les autres armes à feu" },
			{ "de", "Ladehemmungen aller anderen Schusswaffen" },
			{ "it", "gli inceppamenti di tutte le altre armi da fuoco" },
			{ "es", "los encasquillamientos de todas las demás armas de fuego" },
			{ "esmx", "los encasquillamientos de todas las demás armas de fuego" },
			{ "ptbr", "o emperramento de todas as outras armas de fogo" },
			{ "pl", "zacięcia całej pozostałej broni palnej" },
			{ "ru", "заклинивание всего остального стрелкового оружия" },
			{ "ja", "ほかのすべての銃の弾詰まり" },
			{ "zhhant", "其他所有槍枝的卡彈" },
			{ "zhhans", "其他所有枪支的卡弹" },
		};

		constexpr Line PHRASE_JAM_RELOAD[]{
			{ "en", "jams of single shot guns" },
			{ "fr", "l'enrayage des armes monocoup" },
			{ "de", "Ladehemmungen von Einzelschusswaffen" },
			{ "it", "gli inceppamenti delle armi a colpo singolo" },
			{ "es", "los encasquillamientos de las armas de un solo disparo" },
			{ "esmx", "los encasquillamientos de las armas de un solo disparo" },
			{ "ptbr", "o emperramento das armas de tiro único" },
			{ "pl", "zacięcia broni jednostrzałowej" },
			{ "ru", "заклинивание однозарядного оружия" },
			{ "ja", "単発銃の弾詰まり" },
			{ "zhhant", "單發槍枝的卡彈" },
			{ "zhhans", "单发枪支的卡弹" },
		};

		// -------------------------------------------------------------------
		// Repairs and melee
		// -------------------------------------------------------------------

		constexpr Line PHRASE_TRADER_REPAIRS[]{
			{ "en", "repairs" },
			{ "fr", "les réparations" },
			{ "de", "Reparaturen" },
			{ "it", "le riparazioni" },
			{ "es", "las reparaciones" },
			{ "esmx", "las reparaciones" },
			{ "ptbr", "os consertos" },
			{ "pl", "naprawy" },
			{ "ru", "ремонт" },
			{ "ja", "修理" },
			{ "zhhant", "修理" },
			{ "zhhans", "修理" },
		};

		// Melee hits and bashes, which wear a weapon whatever another mod
		// takes.
		constexpr Line PHRASE_MELEE_WEAR[]{
			{ "en", "melee wear" },
			{ "fr", "l'usure au corps à corps" },
			{ "de", "Nahkampfabnutzung" },
			{ "it", "l'usura in mischia" },
			{ "es", "el desgaste cuerpo a cuerpo" },
			{ "esmx", "el desgaste cuerpo a cuerpo" },
			{ "ptbr", "o desgaste corpo a corpo" },
			{ "pl", "zużycie w walce wręcz" },
			{ "ru", "износ в ближнем бою" },
			{ "ja", "近接攻撃による消耗" },
			{ "zhhant", "近戰損耗" },
			{ "zhhans", "近战损耗" },
		};

		// A motor blade's cuts, such as the Ripper's, which wear it whatever
		// another mod takes.
		constexpr Line PHRASE_MOTOR_WEAR[]{
			{ "en", "motor blade wear" },
			{ "fr", "l'usure des lames à moteur" },
			{ "de", "Abnutzung von Motorklingen" },
			{ "it", "l'usura delle lame a motore" },
			{ "es", "el desgaste de armas blancas con motor" },
			{ "esmx", "el desgaste de armas blancas con motor" },
			{ "ptbr", "o desgaste das lâminas motorizadas" },
			{ "pl", "zużycie ostrzy z silnikiem" },
			{ "ru", "износ клинков с мотором" },
			{ "ja", "モーター付き刃物の消耗" },
			{ "zhhant", "電動刀刃的損耗" },
			{ "zhhans", "电动刀刃的损耗" },
		};

		// -------------------------------------------------------------------
		// Worn loot
		// -------------------------------------------------------------------

		// Worn loot's core part has the setting's own name, so the setting's
		// sentence names its 3 pieces with these words instead.
		constexpr Line PHRASE_WORN_LOOT[]{
			{ "en", "loot in containers and on NPCs" },
			{ "fr", "le butin des conteneurs et des PNJ" },
			{ "de", "Beute in Behältern und bei NPCs" },
			{ "it", "il bottino nei contenitori e sui PNG" },
			{ "es", "el botín de contenedores y de otros personajes" },
			{ "esmx", "el botín de contenedores y de otros personajes" },
			{ "ptbr", "o saque em contêineres e com NPCs" },
			{ "pl", "łupy w pojemnikach i u NPC" },
			{ "ru", "трофеи в ящиках и у NPC" },
			{ "ja", "容器とNPCの戦利品" },
			{ "zhhant", "容器和其他角色身上的戰利品" },
			{ "zhhans", "容器和其他角色身上的战利品" },
		};

		constexpr Line PHRASE_WORLD_LOOT[]{
			{ "en", "loot lying around" },
			{ "fr", "le butin qui traîne" },
			{ "de", "herumliegende Beute" },
			{ "it", "il bottino sparso" },
			{ "es", "el botín tirado por ahí" },
			{ "esmx", "el botín tirado por ahí" },
			{ "ptbr", "o saque largado por aí" },
			{ "pl", "łupy leżące na ziemi" },
			{ "ru", "валяющиеся трофеи" },
			{ "ja", "落ちている戦利品" },
			{ "zhhant", "散落的戰利品" },
			{ "zhhans", "散落的战利品" },
		};

		constexpr Line PHRASE_MODLESS_LOOT[]{
			{ "en", "loot with no mod slots" },
			{ "fr", "le butin sans emplacement de modification" },
			{ "de", "Beute ohne Mod-Plätze" },
			{ "it", "il bottino senza slot per modifiche" },
			{ "es", "el botín sin ranuras de modificación" },
			{ "esmx", "el botín sin ranuras de modificación" },
			{ "ptbr", "o saque sem espaços para modificações" },
			{ "pl", "łupy bez miejsc na modyfikacje" },
			{ "ru", "трофеи без слотов модификаций" },
			{ "ja", "改造スロットのない戦利品" },
			{ "zhhant", "無改造欄位的戰利品" },
			{ "zhhans", "无改造栏位的战利品" },
		};

		// -------------------------------------------------------------------
		// Every phrase
		// -------------------------------------------------------------------

		constexpr PieceWords PHRASES[]{
			{ Piece::kGunWear, PHRASE_GUN_WEAR },
			{ Piece::kFireRate, PHRASE_FIRE_RATE },
			{ Piece::kFireRateNpcGuns, PHRASE_NPC_GUNS },
			{ Piece::kFireSound, PHRASE_FIRE_SOUND },
			{ Piece::kCritMeter, PHRASE_CRIT_METER },
			{ Piece::kNpcCrits, PHRASE_NPC_CRITS },
			{ Piece::kAccuracy, PHRASE_ACCURACY },
			{ Piece::kCrosshair, PHRASE_CROSSHAIR },
			{ Piece::kVatsAccuracy, PHRASE_VATS_ACCURACY },
			{ Piece::kRange, PHRASE_RANGE },
			{ Piece::kJamShot, PHRASE_JAM_SHOT },
			{ Piece::kJamReload, PHRASE_JAM_RELOAD },
			{ Piece::kTraderRepairs, PHRASE_TRADER_REPAIRS },
			{ Piece::kMeleeWear, PHRASE_MELEE_WEAR },
			{ Piece::kMotorWear, PHRASE_MOTOR_WEAR },
			{ Piece::kWornLoot, PHRASE_WORN_LOOT },
			{ Piece::kWorldLoot, PHRASE_WORLD_LOOT },
			{ Piece::kModlessLoot, PHRASE_MODLESS_LOOT },
		};
	}

	std::span<const PieceWords> PiecePhrases()
	{
		return PHRASES;
	}
}
