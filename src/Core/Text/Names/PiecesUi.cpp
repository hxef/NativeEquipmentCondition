#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of the UI parts, see Core/Pieces.h. The menus of
// CND on item cards name the same menus' pieces of Fire rate shown in menus,
// Accuracy shown in menus and Range shown in menus. The phrases a setting's
// sentence uses for a piece are in Phrases.cpp.
namespace Text
{
	namespace
	{
		// The Pip-Boy's perk list, its item cards and a broken item's faded
		// name.
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
		constexpr Line PIECE_SORT_RATE[]{
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

		// The sort by accuracy in containers and at traders.
		constexpr Line PIECE_SORT_ACCURACY[]{
			{ "en", "sorting by accuracy" },
			{ "fr", "tri par précision" },
			{ "de", "Sortieren nach Präzision" },
			{ "it", "ordinamento per precisione" },
			{ "es", "orden por precisión" },
			{ "esmx", "orden por precisión" },
			{ "ptbr", "ordenação por precisão" },
			{ "pl", "sortowanie według celności" },
			{ "ru", "сортировка по меткости" },
			{ "ja", "精度での並べ替え" },
			{ "zhhant", "依準度排序" },
			{ "zhhans", "按准度排序" },
		};

		constexpr PieceWords NAMES[]{
			{ Piece::kPerksPipboy, PIECE_PIPBOY },
			{ Piece::kCndContainers, PIECE_CONTAINERS },
			{ Piece::kCndWorkbench, PIECE_WORKBENCHES },
			{ Piece::kCndCooking, PIECE_COOKING },
			{ Piece::kCndPipboy, PIECE_PIPBOY },
			{ Piece::kRateCardsContainers, PIECE_CONTAINERS },
			{ Piece::kRateCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kRateCardsCooking, PIECE_COOKING },
			{ Piece::kRateCardsPipboy, PIECE_PIPBOY },
			{ Piece::kRateBetter, PIECE_BETTER },
			{ Piece::kRateSort, PIECE_SORT_RATE },
			{ Piece::kAccuracyCardsContainers, PIECE_CONTAINERS },
			{ Piece::kAccuracyCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kAccuracyCardsCooking, PIECE_COOKING },
			{ Piece::kAccuracyCardsPipboy, PIECE_PIPBOY },
			{ Piece::kAccuracySort, PIECE_SORT_ACCURACY },
			{ Piece::kRangeCardsContainers, PIECE_CONTAINERS },
			{ Piece::kRangeCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kRangeCardsCooking, PIECE_COOKING },
			{ Piece::kRangeCardsPipboy, PIECE_PIPBOY },
			{ Piece::kBenchItemList, PIECE_BENCH_ITEM_LIST },
			{ Piece::kBenchModLists, PIECE_BENCH_MOD_LISTS },
			{ Piece::kStockMerchant, PIECE_STOCK_MERCHANT },
			{ Piece::kStockLinked, PIECE_STOCK_LINKED },
		};
	}

	std::span<const PieceWords> UiPieces()
	{
		return NAMES;
	}
}
