#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of the UI parts, see Core/Pieces.h, and the phrases
// a setting's sentence uses for a piece. The menus of CND on item cards name
// the same menus' pieces of Fire rate shown in menus.
namespace Text
{
	namespace
	{
		// The Pip-Boy's perk list.
		constexpr Line PIECE_PIPBOY[]{
			{ "en", "the Pip-Boy" },
			{ "fr", "le Pip-Boy" },
			{ "de", "Pip-Boy" },
			{ "it", "il Pip-Boy" },
			{ "es", "el Pip-Boy" },
			{ "esmx", "el Pip-Boy" },
			{ "ptbr", "o Pip-Boy" },
			{ "pl", "Pip-Boy" },
			{ "ru", "Пип-бой" },
			{ "ja", "Pip-Boy" },
			{ "zhhant", "嗶嗶小子" },
			{ "zhhans", "哔哔小子" },
		};

		// The menus an item card shows in. Containers and bartering share 1,
		// so do the workbench, the power armor station and the inspect screen,
		// and so do the cooking and chemistry stations.
		constexpr Line PIECE_CONTAINERS[]{
			{ "en", "containers and traders" },
			{ "fr", "conteneurs et marchands" },
			{ "de", "Behälter und Händler" },
			{ "it", "contenitori e commercianti" },
			{ "es", "contenedores y comerciantes" },
			{ "esmx", "contenedores y comerciantes" },
			{ "ptbr", "contêineres e comerciantes" },
			{ "pl", "pojemniki i handlarze" },
			{ "ru", "ящики и торговцы" },
			{ "ja", "容器と商人" },
			{ "zhhant", "容器和商人" },
			{ "zhhans", "容器和商人" },
		};

		constexpr Line PIECE_WORKBENCHES[]{
			{ "en", "workbenches and the inspect screen" },
			{ "fr", "établis et écran d'examen" },
			{ "de", "Werkbänke und Untersuchen-Ansicht" },
			{ "it", "banchi da lavoro e schermata Ispeziona" },
			{ "es", "bancos de trabajo y pantalla de inspección" },
			{ "esmx", "bancos de trabajo y pantalla de inspección" },
			{ "ptbr", "bancadas e tela de inspeção" },
			{ "pl", "pracownie i ekran badania" },
			{ "ru", "верстаки и экран осмотра" },
			{ "ja", "作業台と調べる画面" },
			{ "zhhant", "工作台和檢查畫面" },
			{ "zhhans", "工作台和检查画面" },
		};

		constexpr Line PIECE_COOKING[]{
			{ "en", "cooking and chemistry stations" },
			{ "fr", "postes de cuisine et ateliers de chimie" },
			{ "de", "Koch- und Laborstationen" },
			{ "it", "cucine da campo e laboratori chimici" },
			{ "es", "fogones y centrales químicas" },
			{ "esmx", "estaciones de cocina y centrales químicas" },
			{ "ptbr", "estações culinárias e químicas" },
			{ "pl", "stanowiska kucharskie i chemiczne" },
			{ "ru", "пункты приготовления пищи и химлаборатории" },
			{ "ja", "クッキングステーションとケミストリーステーション" },
			{ "zhhant", "烹飪工作台和化學工作台" },
			{ "zhhans", "烹饪工作台和化学工作台" },
		};

		// The 2 moments the Pip-Boy builds an item card, with its CND row and a
		// broken item's faded name: as it lists an item, which it does again
		// when the item is added, put on or changed, and as it updates every
		// card of a kind, which NEC asks for after wear and after a repair.
		constexpr Line PIECE_PIPBOY_LISTS[]{
			{ "en", "the Pip-Boy as it lists items" },
			{ "fr", "le Pip-Boy quand il liste les objets" },
			{ "de", "Pip-Boy beim Auflisten von Gegenständen" },
			{ "it", "il Pip-Boy quando elenca gli oggetti" },
			{ "es", "el Pip-Boy al listar objetos" },
			{ "esmx", "el Pip-Boy al listar objetos" },
			{ "ptbr", "o Pip-Boy ao listar itens" },
			{ "pl", "Pip-Boy przy tworzeniu listy przedmiotów" },
			{ "ru", "Пип-бой при составлении списка предметов" },
			{ "ja", "Pip-Boyがアイテムを一覧表示するとき" },
			{ "zhhant", "嗶嗶小子列出物品時" },
			{ "zhhans", "哔哔小子列出物品时" },
		};

		constexpr Line PIECE_PIPBOY_UPDATES[]{
			{ "en", "the Pip-Boy as it updates cards" },
			{ "fr", "le Pip-Boy quand il met à jour les fiches" },
			{ "de", "Pip-Boy beim Aktualisieren der Gegenstandsinfos" },
			{ "it", "il Pip-Boy quando aggiorna le schede" },
			{ "es", "el Pip-Boy al actualizar fichas" },
			{ "esmx", "el Pip-Boy al actualizar fichas" },
			{ "ptbr", "o Pip-Boy ao atualizar fichas" },
			{ "pl", "Pip-Boy przy odświeżaniu kart" },
			{ "ru", "Пип-бой при обновлении карточек" },
			{ "ja", "Pip-Boyがアイテム情報を更新するとき" },
			{ "zhhant", "嗶嗶小子更新物品資訊時" },
			{ "zhhans", "哔哔小子更新物品信息时" },
		};

		// The mark the list over a container or a body puts on a better item.
		constexpr Line PIECE_BETTER[]{
			{ "en", "better marks in loot previews" },
			{ "fr", "marques des meilleurs objets dans l'aperçu du butin" },
			{ "de", "Bessermarkierungen in der Beutevorschau" },
			{ "it", "segni di oggetto migliore nell'anteprima del bottino" },
			{ "es", "marcas de mejor objeto en la vista previa del botín" },
			{ "esmx", "marcas de mejor objeto en la vista previa del botín" },
			{ "ptbr", "marcas de item melhor na prévia de saque" },
			{ "pl", "lepsze oznaczenia w podglądzie łupów" },
			{ "ru", "лучшие метки в предпросмотре трофеев" },
			{ "ja", "中身プレビューの良品マーク" },
			{ "zhhant", "戰利品預覽中的更佳標記" },
			{ "zhhans", "战利品预览中的更佳标记" },
		};

		// The sort by fire rate in containers and at traders.
		constexpr Line PIECE_SORT[]{
			{ "en", "sorting by fire rate" },
			{ "fr", "tri par cadence de tir" },
			{ "de", "Sortieren nach Feuerrate" },
			{ "it", "ordinamento per cadenza di fuoco" },
			{ "es", "orden por cadencia de tiro" },
			{ "esmx", "orden por cadencia de tiro" },
			{ "ptbr", "ordenação por cadência de tiro" },
			{ "pl", "sortowanie według szybkostrzelności" },
			{ "ru", "сортировка по скорострельности" },
			{ "ja", "発射速度での並べ替え" },
			{ "zhhant", "依射速排序" },
			{ "zhhans", "按射速排序" },
		};

		// The bench's lists: its item list, where a worn item is listed and
		// one too worn to modify faded, and the mod lists, shut for that item.
		constexpr Line PIECE_BENCH_ITEM_LIST[]{
			{ "en", "item list" },
			{ "fr", "liste des objets" },
			{ "de", "Gegenstandsliste" },
			{ "it", "elenco degli oggetti" },
			{ "es", "lista de objetos" },
			{ "esmx", "lista de objetos" },
			{ "ptbr", "lista de itens" },
			{ "pl", "lista przedmiotów" },
			{ "ru", "список предметов" },
			{ "ja", "アイテムの一覧" },
			{ "zhhant", "物品清單" },
			{ "zhhans", "物品清单" },
		};

		constexpr Line PIECE_BENCH_MOD_LISTS[]{
			{ "en", "mod lists" },
			{ "fr", "listes des modifications" },
			{ "de", "Mod-Listen" },
			{ "it", "elenchi delle modifiche" },
			{ "es", "listas de modificaciones" },
			{ "esmx", "listas de modificaciones" },
			{ "ptbr", "listas de modificações" },
			{ "pl", "listy modyfikacji" },
			{ "ru", "списки модификаций" },
			{ "ja", "改造の一覧" },
			{ "zhhant", "改造清單" },
			{ "zhhans", "改造清单" },
		};

		// The 2 kinds of chest a trader restocks: the merchant chest every
		// trader of a faction shares, and the chests linked to the trader
		// alone, like a settlement store's.
		constexpr Line PIECE_STOCK_MERCHANT[]{
			{ "en", "merchant chests" },
			{ "fr", "coffres de marchands" },
			{ "de", "Händlertruhen" },
			{ "it", "bauli dei commercianti" },
			{ "es", "cofres de comerciantes" },
			{ "esmx", "cofres de comerciantes" },
			{ "ptbr", "baús de comerciantes" },
			{ "pl", "skrzynie handlarzy" },
			{ "ru", "сундуки торговцев" },
			{ "ja", "商人チェスト" },
			{ "zhhant", "商人貨箱" },
			{ "zhhans", "商人货箱" },
		};

		constexpr Line PIECE_STOCK_LINKED[]{
			{ "en", "chests linked to traders" },
			{ "fr", "coffres liés aux marchands" },
			{ "de", "mit Händlern verbundene Truhen" },
			{ "it", "bauli legati ai commercianti" },
			{ "es", "cofres vinculados a comerciantes" },
			{ "esmx", "cofres vinculados a comerciantes" },
			{ "ptbr", "baús ligados a comerciantes" },
			{ "pl", "skrzynie powiązane z handlarzami" },
			{ "ru", "связанные с торговцами сундуки" },
			{ "ja", "商人に紐づくチェスト" },
			{ "zhhant", "與商人連結的貨箱" },
			{ "zhhans", "与商人关联的货箱" },
		};

		// What a setting's sentence says in place of a piece's name, after "no
		// effect on" and after "Still changes" or "Still works for".
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
			{ "en", "your guns and blades" },
			{ "fr", "vos armes à feu et vos lames" },
			{ "de", "deine Schusswaffen und Klingen" },
			{ "it", "le tue armi da fuoco e lame" },
			{ "es", "tus armas de fuego y armas blancas" },
			{ "esmx", "tus armas de fuego y armas blancas" },
			{ "ptbr", "suas armas de fogo e lâminas" },
			{ "pl", "twoja broń palna i ostrza" },
			{ "ru", "ваше стрелковое оружие и клинки" },
			{ "ja", "自分の銃と刃物" },
			{ "zhhant", "你的槍枝和刀刃" },
			{ "zhhans", "你的枪支和刀刃" },
		};

		constexpr Line PHRASE_NPC_GUNS[]{
			{ "en", "NPC guns" },
			{ "fr", "les armes à feu des PNJ" },
			{ "de", "NPC-Schusswaffen" },
			{ "it", "le armi da fuoco dei PNG" },
			{ "es", "las armas de fuego de otros personajes" },
			{ "esmx", "las armas de fuego de otros personajes" },
			{ "ptbr", "as armas de fogo dos NPCs" },
			{ "pl", "broń palna NPC" },
			{ "ru", "стрелковое оружие NPC" },
			{ "ja", "NPCの銃" },
			{ "zhhant", "其他角色的槍枝" },
			{ "zhhans", "其他角色的枪支" },
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

		// Every gun but those that fire once per reload.
		constexpr Line PHRASE_JAM_SHOT[]{
			{ "en", "all other guns" },
			{ "fr", "toutes les autres armes à feu" },
			{ "de", "alle anderen Schusswaffen" },
			{ "it", "tutte le altre armi da fuoco" },
			{ "es", "todas las demás armas de fuego" },
			{ "esmx", "todas las demás armas de fuego" },
			{ "ptbr", "todas as outras armas de fogo" },
			{ "pl", "cała pozostała broń palna" },
			{ "ru", "все остальное стрелковое оружие" },
			{ "ja", "ほかのすべての銃" },
			{ "zhhant", "其他所有槍枝" },
			{ "zhhans", "其他所有枪支" },
		};

		constexpr Line PHRASE_JAM_RELOAD[]{
			{ "en", "single shot guns" },
			{ "fr", "les armes monocoup" },
			{ "de", "Einzelschusswaffen" },
			{ "it", "le armi a colpo singolo" },
			{ "es", "las armas de un solo disparo" },
			{ "esmx", "las armas de un solo disparo" },
			{ "ptbr", "as armas de tiro único" },
			{ "pl", "broń jednostrzałowa" },
			{ "ru", "однозарядное оружие" },
			{ "ja", "単発銃" },
			{ "zhhant", "單發槍枝" },
			{ "zhhans", "单发枪支" },
		};

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

		constexpr PieceWords NAMES[]{
			{ Piece::kPerksPipboy, PIECE_PIPBOY },
			{ Piece::kCndContainers, PIECE_CONTAINERS },
			{ Piece::kCndWorkbench, PIECE_WORKBENCHES },
			{ Piece::kCndCooking, PIECE_COOKING },
			{ Piece::kCndPipboyLists, PIECE_PIPBOY_LISTS },
			{ Piece::kCndPipboyUpdates, PIECE_PIPBOY_UPDATES },
			{ Piece::kRateCardsContainers, PIECE_CONTAINERS },
			{ Piece::kRateCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kRateCardsCooking, PIECE_COOKING },
			{ Piece::kRateCardsPipboyLists, PIECE_PIPBOY_LISTS },
			{ Piece::kRateCardsPipboyUpdates, PIECE_PIPBOY_UPDATES },
			{ Piece::kRateBetter, PIECE_BETTER },
			{ Piece::kRateSort, PIECE_SORT },
			{ Piece::kBenchItemList, PIECE_BENCH_ITEM_LIST },
			{ Piece::kBenchModLists, PIECE_BENCH_MOD_LISTS },
			{ Piece::kStockMerchant, PIECE_STOCK_MERCHANT },
			{ Piece::kStockLinked, PIECE_STOCK_LINKED },
		};

		constexpr PieceWords PHRASES[]{
			{ Piece::kGunWear, PHRASE_GUN_WEAR },
			{ Piece::kFireRate, PHRASE_FIRE_RATE },
			{ Piece::kFireRateNpcGuns, PHRASE_NPC_GUNS },
			{ Piece::kFireSound, PHRASE_FIRE_SOUND },
			{ Piece::kCritMeter, PHRASE_CRIT_METER },
			{ Piece::kNpcCrits, PHRASE_NPC_CRITS },
			{ Piece::kJamShot, PHRASE_JAM_SHOT },
			{ Piece::kJamReload, PHRASE_JAM_RELOAD },
			{ Piece::kTraderRepairs, PHRASE_TRADER_REPAIRS },
			{ Piece::kMeleeWear, PHRASE_MELEE_WEAR },
		};
	}

	std::span<const PieceWords> UiPieces()
	{
		return NAMES;
	}

	std::span<const PieceWords> PiecePhrases()
	{
		return PHRASES;
	}
}
