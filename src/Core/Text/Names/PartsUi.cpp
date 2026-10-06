#include "Core/Text/Lines.h"

#include "Core/Parts.h"

#include <cstddef>
#include <iterator>

// The names of the parts of the UI rows, see Core/Parts.h.
namespace Text
{
	namespace
	{
		// The CND row NEC adds to every item card, by the game's own word.
		constexpr Line PART_CARD_CND[]{
			{ "en", "CND on item cards" },
			{ "fr", "CND sur les fiches des objets" },
			{ "de", "ZST in Gegenstandsinfos" },
			{ "it", "CON nelle schede degli oggetti" },
			{ "es", "CON en las fichas de objetos" },
			{ "esmx", "CON en las fichas de objetos" },
			{ "ptbr", "COND nas fichas dos itens" },
			{ "pl", "STN na kartach przedmiotów" },
			{ "ru", "СОСТ в карточках предметов" },
			{ "ja", "アイテム情報のCND" },
			{ "zhhant", "物品資訊顯示狀況" },
			{ "zhhans", "物品信息显示状况" },
		};

		// A worn gun's fire rate on item cards, and where a menu sorts or
		// weighs by it.
		constexpr Line PART_CARD_RATE[]{
			{ "en", "Fire rate shown in menus" },
			{ "fr", "Cadence de tir affichée dans les menus" },
			{ "de", "Feuerratenanzeige in Menüs" },
			{ "it", "Cadenza di fuoco nei menu" },
			{ "es", "Cadencia de tiro en los menús" },
			{ "esmx", "Cadencia de tiro en los menús" },
			{ "ptbr", "Cadência de tiro exibida nos menus" },
			{ "pl", "Szybkostrzelność widoczna w menu" },
			{ "ru", "Скорострельность в меню" },
			{ "ja", "メニューの発射速度表示" },
			{ "zhhant", "選單中顯示的射速" },
			{ "zhhans", "菜单中显示的射速" },
		};

		// The bars by the ammo counter and on the power armor dash.
		constexpr Line PART_HUD_BAR[]{
			{ "en", "HUD condition bars" },
			{ "fr", "Barres d'état de l'ATH" },
			{ "de", "Zustandsbalken im HUD" },
			{ "it", "Barre CON nell'interfaccia" },
			{ "es", "Barras de estado en el HUD" },
			{ "esmx", "Barras de estado en el HUD" },
			{ "ptbr", "Barras de condição no HUD" },
			{ "pl", "Paski stanu w interfejsie" },
			{ "ru", "Полосы состояния в интерфейсе" },
			{ "ja", "HUDの状態バー" },
			{ "zhhant", "狀態欄的狀況條" },
			{ "zhhans", "状态栏的状况条" },
		};

		// The list that shows when the player looks at a container or a body.
		constexpr Line PART_QUICK[]{
			{ "en", "Loot preview meters" },
			{ "fr", "Jauges de l'aperçu du butin" },
			{ "de", "Zustand in der Beutevorschau" },
			{ "it", "Indicatori nell'anteprima del bottino" },
			{ "es", "Indicadores en la vista previa del botín" },
			{ "esmx", "Indicadores en la vista previa del botín" },
			{ "ptbr", "Medidores na prévia de saque" },
			{ "pl", "Wskaźniki w podglądzie łupów" },
			{ "ru", "Индикаторы в предпросмотре трофеев" },
			{ "ja", "中身プレビューの状態メーター" },
			{ "zhhant", "戰利品預覽狀況條" },
			{ "zhhans", "战利品预览状况条" },
		};

		constexpr Line PART_BENCH[]{
			{ "en", "Workbench repairs" },
			{ "fr", "Réparations à l'établi" },
			{ "de", "Reparaturen an der Werkbank" },
			{ "it", "Riparazioni al banco da lavoro" },
			{ "es", "Reparaciones en el banco de trabajo" },
			{ "esmx", "Reparaciones en el banco de trabajo" },
			{ "ptbr", "Consertos na bancada" },
			{ "pl", "Naprawy w pracowni" },
			{ "ru", "Ремонт на верстаке" },
			{ "ja", "作業台での修理" },
			{ "zhhant", "工作台修理" },
			{ "zhhans", "工作台修理" },
		};

		// A worn item the bench would leave out listed for its repair, an item
		// too worn to modify faded, and its mod slots shut.
		constexpr Line PART_BENCH_LISTS[]{
			{ "en", "Worn items in workbench lists" },
			{ "fr", "Objets usés dans les listes de l'établi" },
			{ "de", "Abgenutzte Gegenstände in Werkbanklisten" },
			{ "it", "Oggetti usurati negli elenchi del banco da lavoro" },
			{ "es", "Objetos desgastados en las listas del banco de trabajo" },
			{ "esmx", "Objetos desgastados en las listas del banco de trabajo" },
			{ "ptbr", "Itens desgastados nas listas da bancada" },
			{ "pl", "Zużyte przedmioty na listach pracowni" },
			{ "ru", "Изношенные предметы в списках верстака" },
			{ "ja", "作業台の一覧の消耗したアイテム" },
			{ "zhhant", "工作台清單中的損耗物品" },
			{ "zhhans", "工作台清单中的损耗物品" },
		};

		constexpr Line PART_SCROLL[]{
			{ "en", "Scrolling component lists" },
			{ "fr", "Défilement des listes de composants" },
			{ "de", "Scrollbare Komponentenlisten" },
			{ "it", "Elenchi dei componenti scorrevoli" },
			{ "es", "Listas de componentes desplazables" },
			{ "esmx", "Listas de componentes desplazables" },
			{ "ptbr", "Rolagem das listas de componentes" },
			{ "pl", "Przewijane listy komponentów" },
			{ "ru", "Прокрутка списка компонентов" },
			{ "ja", "部品リストのスクロール" },
			{ "zhhant", "元件清單捲動" },
			{ "zhhans", "元件清单滚动" },
		};

		constexpr Line PART_TRADER_REPAIRS[]{
			{ "en", "Trader repairs" },
			{ "fr", "Réparations chez les marchands" },
			{ "de", "Reparaturen beim Händler" },
			{ "it", "Riparazioni dai commercianti" },
			{ "es", "Reparaciones de comerciantes" },
			{ "esmx", "Reparaciones de comerciantes" },
			{ "ptbr", "Consertos com comerciantes" },
			{ "pl", "Naprawy u handlarzy" },
			{ "ru", "Ремонт у торговцев" },
			{ "ja", "商人による修理" },
			{ "zhhant", "商人修理" },
			{ "zhhans", "商人修理" },
		};

		// A trader who repairs a kind of gear sells their own in better shape.
		constexpr Line PART_STOCK[]{
			{ "en", "Trader stock condition" },
			{ "fr", "État du stock des marchands" },
			{ "de", "Zustand der Händlerware" },
			{ "it", "Condizioni della merce in vendita" },
			{ "es", "Estado de la mercancía de comerciantes" },
			{ "esmx", "Estado de la mercancía de comerciantes" },
			{ "ptbr", "Condição do estoque dos comerciantes" },
			{ "pl", "Stan towaru handlarzy" },
			{ "ru", "Состояние товаров торговцев" },
			{ "ja", "商人の在庫の状態" },
			{ "zhhant", "商人貨品狀況" },
			{ "zhhans", "商人货品状况" },
		};

		constexpr Line PART_INSPECT[]{
			{ "en", "Inspect price fix" },
			{ "fr", "Prix correct à l'examen" },
			{ "de", "Korrekter Preis beim Untersuchen" },
			{ "it", "Prezzo corretto in Ispeziona" },
			{ "es", "Precio correcto al inspeccionar" },
			{ "esmx", "Precio correcto al inspeccionar" },
			{ "ptbr", "Preço correto ao inspecionar" },
			{ "pl", "Poprawna cena przy badaniu" },
			{ "ru", "Верная цена при осмотре" },
			{ "ja", "調べる画面の価格修正" },
			{ "zhhant", "檢查畫面價格修正" },
			{ "zhhans", "检查画面价格修正" },
		};

		// srm is the console command, the same in every language.
		constexpr Line PART_SRM[]{
			{ "en", "Console repair (srm)" },
			{ "fr", "Réparation par console (srm)" },
			{ "de", "Konsolenreparatur (srm)" },
			{ "it", "Riparazione da console (srm)" },
			{ "es", "Reparación por consola (srm)" },
			{ "esmx", "Reparación por consola (srm)" },
			{ "ptbr", "Conserto pelo console (srm)" },
			{ "pl", "Naprawa z konsoli (srm)" },
			{ "ru", "Ремонт через консоль (srm)" },
			{ "ja", "コンソールでの修理(srm)" },
			{ "zhhant", "主控台修理（srm）" },
			{ "zhhans", "控制台修理（srm）" },
		};

		constexpr Line PART_TIPS[]{
			{ "en", "Loading screen tips" },
			{ "fr", "Astuces des écrans de chargement" },
			{ "de", "Tipps auf Ladebildschirmen" },
			{ "it", "Suggerimenti nei caricamenti" },
			{ "es", "Consejos en las pantallas de carga" },
			{ "esmx", "Consejos en las pantallas de carga" },
			{ "ptbr", "Dicas nas telas de carregamento" },
			{ "pl", "Porady na ekranach wczytywania" },
			{ "ru", "Советы на экранах загрузки" },
			{ "ja", "ロード画面のヒント" },
			{ "zhhant", "載入畫面提示" },
			{ "zhhans", "载入画面提示" },
		};

		constexpr PartRow PARTS[]{
			{ Part::kCardCnd, PART_CARD_CND },
			{ Part::kCardRate, PART_CARD_RATE },
			{ Part::kHudBar, PART_HUD_BAR },
			{ Part::kQuick, PART_QUICK },
			{ Part::kBench, PART_BENCH },
			{ Part::kBenchLists, PART_BENCH_LISTS },
			{ Part::kScroll, PART_SCROLL },
			{ Part::kTraderRepairs, PART_TRADER_REPAIRS },
			{ Part::kStock, PART_STOCK },
			{ Part::kInspect, PART_INSPECT },
			{ Part::kSrm, PART_SRM },
			{ Part::kTips, PART_TIPS },
		};

		// 1 row a part, in the enum's order from kCardCnd to kTips, so a
		// part's name is found by its place.
		consteval bool InOrder()
		{
			const auto first = static_cast<std::size_t>(Part::kCardCnd);
			for (std::size_t i = 0; i < std::size(PARTS); i++) {
				if (static_cast<std::size_t>(PARTS[i].part) != first + i) {
					return false;
				}
			}
			return std::size(PARTS) == static_cast<std::size_t>(Part::kTips) - first + 1;
		}
		static_assert(InOrder());
	}

	std::span<const PartRow> UiParts()
	{
		return PARTS;
	}
}
