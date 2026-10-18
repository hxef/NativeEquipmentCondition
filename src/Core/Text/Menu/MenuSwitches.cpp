#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The help lines of the switches on the MCM page, and the name and help of the
// bug report logs switch, see MenuLine in Text.h. A help line says what the
// switch does and what off leaves, in 1 short line.
namespace Text
{
	namespace
	{
		constexpr Line JAM_HELP[]{
			{ "en", "Your guns below half condition can jam until you reload. When off, they never jam." },
			{ "fr", "Sous la moitié de leur état, vos armes à feu peuvent s'enrayer jusqu'au rechargement. Désactivé\u00A0: jamais." },
			{ "de", "Deine Schusswaffen unter halbem Zustand können klemmen, bis du nachlädst. Aus: Sie klemmen nie." },
			{ "it", "Con meno di metà condizioni, le tue armi da fuoco possono incepparsi fino alla ricarica. Se disattivato, mai." },
			{ "es", "Bajo la mitad de estado, tus armas de fuego pueden encasquillarse hasta recargar. Desactivado: nunca." },
			{ "esmx", "Bajo la mitad de estado, tus armas de fuego pueden encasquillarse hasta recargar. Desactivado: nunca." },
			{ "ptbr", "Abaixo da metade da condição, suas armas de fogo podem emperrar até recarregar. Desativado: nunca." },
			{ "pl", "Twoja broń palna poniżej połowy stanu może się zaciąć do przeładowania. Wyłączone: bez zacięć." },
			{ "ru", "Ваше оружие ниже половины состояния может заклинить до перезарядки. Если отключено, нет." },
			{ "ja", "自分の銃は状態半分未満で弾詰まりすることがあり、リロードで直ります。OFFで無効" },
			{ "zhhant", "你的槍枝狀況低於一半時可能卡彈，重新裝填才能排除。關閉時，槍枝不會卡彈。" },
			{ "zhhans", "你的枪支状况低于一半时可能卡弹，重新装填才能排除。关闭时，枪支不会卡弹。" },
		};

		constexpr Line SPAWN_CONDITION_HELP[]{
			{ "en", "Weapons and armor you find are already worn, based on who had them. When off, they turn up new." },
			{ "fr", "Les armes et armures trouvées sont déjà usées, selon leur ancien porteur. Désactivé\u00A0: elles sont neuves." },
			{ "de", "Gefundene Waffen und Rüstungen sind schon abgenutzt, je nach Vorbesitzer. Aus: Sie sind neu." },
			{ "it", "Le armi e le armature che trovi sono già usurate, in base a chi le aveva. Se disattivato, sono nuove." },
			{ "es", "Las armas y armaduras halladas ya están desgastadas, según quién las tuviera. Desactivado: nuevas." },
			{ "esmx", "Las armas y armaduras halladas ya están desgastadas, según quién las tuviera. Desactivado: nuevas." },
			{ "ptbr", "Armas e armaduras encontradas já vêm desgastadas, conforme quem as tinha. Desativado: vêm novas." },
			{ "pl", "Znaleziona broń i pancerze są już zużyte, zależnie od poprzedniego właściciela. Wyłączone: są nowe." },
			{ "ru", "Найденные оружие и броня уже изношены, смотря у кого они были. Если отключено, они новые." },
			{ "ja", "見つけた武器とアーマーは、前の持ち主に応じて消耗しています。OFFにすると新品です" },
			{ "zhhant", "你找到的武器和裝甲已有損耗，程度取決於原本的持有者。關閉時，皆為全新。" },
			{ "zhhans", "你找到的武器和装甲已有损耗，程度取决于原来的持有者。关闭时，均为全新。" },
		};

		constexpr Line VENDOR_REPAIR_HELP[]{
			{ "en", "Traders who sell weapons, armor or clothing repair yours for caps. When off, only workbenches repair." },
			{ "fr", "Les marchands d'armes, d'armures ou de vêtements réparent contre des capsules. Désactivé\u00A0: établis seuls." },
			{ "de", "Händler für Waffen, Rüstungen oder Kleidung reparieren diese gegen Kronkorken. Aus: Nur an Werkbänken." },
			{ "it", "Chi vende armi, armature o vestiti ripara i tuoi in cambio di tappi. Se disattivato, solo al banco da lavoro." },
			{ "es", "Los comerciantes de armas, armaduras o ropa reparan las tuyas por chapas. Desactivado: solo el banco." },
			{ "esmx", "Los comerciantes de armas, armaduras o ropa reparan las tuyas por tapas. Desactivado: solo el banco." },
			{ "ptbr", "Comerciantes de armas, armaduras ou roupas consertam as suas por tampas. Desativado: só bancadas." },
			{ "pl", "Handlarze bronią, pancerzami lub ubraniami naprawiają twoje rzeczy za kapsle. Wyłączone: tylko pracownie." },
			{ "ru", "Торговцы оружием, броней или одеждой чинят их за крышки. Если отключено, только верстаки." },
			{ "ja", "武器、アーマー、服を売る商人がキャップで修理します。OFFでは作業台のみです" },
			{ "zhhant", "販售武器、裝甲或服飾的商人會收瓶蓋修理你的同類裝備。關閉時，只有工作台能修理。" },
			{ "zhhans", "出售武器、装甲或服饰的商人会收瓶盖修理你的同类装备。关闭时，只有工作台能修理。" },
		};

		constexpr Line LOADING_TIPS_HELP[]{
			{ "en", "Adds tips about wear, repairs and loot to the loading screens. When off, none of them show." },
			{ "fr", "Ajoute des astuces sur l'usure, les réparations et le butin aux écrans de chargement. Désactivé\u00A0: aucune." },
			{ "de", "Zeigt Tipps zu Abnutzung, Reparaturen und Beute auf Ladebildschirmen. Aus: Keine Tipps." },
			{ "it", "Aggiunge consigli su usura, riparazioni e bottino alle schermate di caricamento. Se disattivato, nessuno." },
			{ "es", "Añade consejos sobre desgaste, reparaciones y botín a las pantallas de carga. Desactivado: ninguno." },
			{ "esmx", "Agrega consejos sobre desgaste, reparaciones y botín a las pantallas de carga. Desactivado: ninguno." },
			{ "ptbr", "Adiciona dicas sobre desgaste, consertos e saque às telas de carregamento. Desativado: nenhuma aparece." },
			{ "pl", "Dodaje porady o zużyciu, naprawach i łupach na ekranach wczytywania. Wyłączone: nie pojawiają się." },
			{ "ru", "Добавляет на экраны загрузки советы об износе, ремонте и трофеях. Если отключено, их нет." },
			{ "ja", "消耗、修理、戦利品のヒントをロード画面に追加します。OFFにすると表示されません" },
			{ "zhhant", "在載入畫面加入損耗、修理和戰利品的相關提示。關閉時，不會顯示這些提示。" },
			{ "zhhans", "在载入画面加入损耗、修理和战利品的相关提示。关闭时，不会显示这些提示。" },
		};

		constexpr Line HUD_CONDITION_HELP[]{
			{ "en", "Shows condition bars by the ammo counter and in power armor. When off, item cards still show condition." },
			{ "fr", "Affiche des barres d'état près des munitions et en armure assistée. Désactivé\u00A0: l'état reste sur les fiches." },
			{ "de", "Zeigt Zustandsbalken an der Munitionsanzeige und in der Powerrüstung. Aus: Nur in den Gegenstandsinfos." },
			{ "it", "Barre di condizioni accanto alle munizioni e nell'armatura atomica. Se disattivato, solo nelle schede." },
			{ "es", "Muestra barras de estado junto a la munición y en la servoarmadura. Desactivado: solo en las fichas." },
			{ "esmx", "Muestra barras de estado junto a la munición y en la servoarmadura. Desactivado: solo en las fichas." },
			{ "ptbr", "Mostra barras de condição junto à munição e na Armadura Potente. Desativado: só nas fichas dos itens." },
			{ "pl", "Pokazuje paski stanu przy amunicji i w pancerzu wspomaganym. Wyłączone: stan tylko na kartach." },
			{ "ru", "Полосы состояния у патронов и в силовой броне. Если отключено, состояние лишь в карточках." },
			{ "ja", "弾薬の横とパワーアーマーに状態バーを表示します。OFFでもアイテム情報に出ます" },
			{ "zhhant", "在彈藥計數旁和動力裝甲中顯示狀況條。關閉時，物品資訊仍會顯示狀況。" },
			{ "zhhans", "在弹药计数旁和动力装甲中显示状况条。关闭时，物品信息仍会显示状况。" },
		};

		constexpr Line QUICK_CONTAINER_HELP[]{
			{ "en", "Shows condition in the list that pops up when you look at a container or body. When off, it shows none." },
			{ "fr", "Affiche l'état dans la liste qui apparaît en visant un conteneur ou un corps. Désactivé\u00A0: état non affiché." },
			{ "de", "Zeigt den Zustand in der Liste, die beim Blick auf Behälter oder Leichen erscheint. Aus: Kein Zustand." },
			{ "it", "Mostra le condizioni nell'elenco che appare guardando contenitori e corpi. Se disattivato, non le mostra." },
			{ "es", "Muestra el estado en la lista que aparece al mirar un contenedor o un cadáver. Desactivado: no lo muestra." },
			{ "esmx", "Muestra el estado en la lista que aparece al mirar un contenedor o un cadáver. Desactivado: no lo muestra." },
			{ "ptbr", "Mostra a condição na lista que surge ao olhar para um contêiner ou corpo. Desativado: não a mostra." },
			{ "pl", "Pokazuje stan w podglądzie pojemnika lub ciała, na które patrzysz. Wyłączone: podgląd go nie pokazuje." },
			{ "ru", "Показывает состояние в списке при взгляде на ящик или тело. Если отключено, его там нет." },
			{ "ja", "容器や死体を見たときの一覧に状態を表示します。OFFにすると表示されません" },
			{ "zhhant", "看向容器或屍體時，彈出的清單會顯示狀況。關閉時則不顯示。" },
			{ "zhhans", "看向容器或尸体时，弹出的清单会显示状况。关闭时则不显示。" },
		};

		// "Base game fix" reads the same in both fixes.
		constexpr Line CONFIRM_SCROLL_HELP[]{
			{ "en", "Base game fix. The workbench confirm box shows more of a long list and scrolls. When off, it can stay small." },
			{ "fr", "Correctif du jeu de base. À l'établi, la confirmation s'agrandit et défile. Désactivé\u00A0: parfois petite." },
			{ "de", "Grundspiel-Fix. Das Bestätigungsfenster der Werkbank wächst und scrollt. Aus: Es kann klein bleiben." },
			{ "it", "Correzione del gioco base. Al banco, la conferma si allarga e scorre. Se disattivato, può restare piccola." },
			{ "es", "Arreglo del juego base. La confirmación del banco crece y se desplaza. Desactivado: puede ser pequeña." },
			{ "esmx", "Arreglo del juego base. La confirmación del banco crece y se desplaza. Desactivado: puede ser pequeña." },
			{ "ptbr", "Correção do jogo base. A confirmação da bancada cresce e rola. Desativado: pode ficar pequena." },
			{ "pl", "Poprawka błędu gry. Potwierdzenie w pracowni jest większe i przewija listy. Wyłączone: bywa małe." },
			{ "ru", "Исправление игры. Окно подтверждения больше, с прокруткой. Если отключено, бывает маленьким." },
			{ "ja", "ゲームの不具合修正。作業台の確認画面が広がりスクロール可。OFFでは小さいことも" },
			{ "zhhant", "原版遊戲修正。工作台確認視窗會放大並可捲動。關閉時，視窗可能維持原本大小。" },
			{ "zhhans", "原版游戏修正。工作台确认窗口会放大并可滚动。关闭时，窗口可能保持原本大小。" },
		};

		constexpr Line INSPECT_PRICE_HELP[]{
			{ "en", "Base game fix. At a trader, inspecting an item shows the right buy or sell price. When off, it can be wrong." },
			{ "fr", "Correctif du jeu de base. Chez un marchand, Examiner affiche le bon prix. Désactivé\u00A0: prix parfois faux." },
			{ "de", "Grundspiel-Fix. Beim Händler zeigt Untersuchen den richtigen Preis. Aus: Er kann falsch sein." },
			{ "it", "Correzione del gioco base. Dal commerciante, Ispeziona dà il prezzo giusto. Se disattivato, può sbagliare." },
			{ "es", "Arreglo del juego base. Al comerciar, Inspeccionar muestra el precio correcto. Desactivado: puede fallar." },
			{ "esmx", "Arreglo del juego base. Al comerciar, Inspeccionar muestra el precio correcto. Desactivado: puede fallar." },
			{ "ptbr", "Correção do jogo base. Ao negociar, Inspecionar mostra o preço certo. Desativado: pode errar." },
			{ "pl", "Poprawka błędu gry. Badanie u handlarza pokazuje właściwą cenę. Wyłączone: bywa błędna." },
			{ "ru", "Исправление игры. Осмотр у торговца дает верную цену. Если отключено, возможна ошибка." },
			{ "ja", "ゲームの不具合修正。取引中に調べると正しい価格が出ます。OFFでは誤ることも" },
			{ "zhhant", "原版遊戲修正。與商人交易時，檢查物品會顯示正確的買賣價。關閉時，價格可能有誤。" },
			{ "zhhans", "原版游戏修正。与商人交易时，检查物品会显示正确的买卖价。关闭时，价格可能有误。" },
		};

		constexpr Line TRACE_LOGS_NAME[]{
			{ "en", "Bug report logs" },
			{ "fr", "Journaux pour rapports de bug" },
			{ "de", "Protokolle für Fehlerberichte" },
			{ "it", "Registri per segnalare bug" },
			{ "es", "Registros para informar de errores" },
			{ "esmx", "Registros para reportar errores" },
			{ "ptbr", "Registros para relatar bugs" },
			{ "pl", "Dzienniki do zgłoszeń błędów" },
			{ "ru", "Журналы для отчетов об ошибках" },
			{ "ja", "バグ報告用ログ" },
			{ "zhhant", "錯誤回報日誌" },
			{ "zhhans", "错误报告日志" },
		};

		constexpr Line TRACE_LOGS_HELP[]{
			{ "en", "Writes 3 more logs next to NEC.log for bug reports. They grow fast, so leave this off unless you report a bug." },
			{ "fr", "Écrit 3 journaux de plus près de NEC.log. Ils grossissent vite\u00A0: activez-les seulement pour un bug." },
			{ "de", "Schreibt 3 Protokolle mehr neben NEC.log. Sie wachsen schnell, also nur für Fehlerberichte einschalten." },
			{ "it", "Scrive 3 registri in più accanto a NEC.log. Crescono in fretta, quindi attivalo solo per segnalare un bug." },
			{ "es", "Escribe 3 registros más junto a NEC.log. Crecen rápido, así que actívalo solo para informar de un error." },
			{ "esmx", "Escribe 3 registros más junto a NEC.log. Crecen rápido, así que actívalo solo para reportar un error." },
			{ "ptbr", "Grava mais 3 registros ao lado do NEC.log. Crescem rápido, então só ative para relatar um bug." },
			{ "pl", "Zapisuje 3 dodatkowe dzienniki obok NEC.log. Szybko rosną, więc włączaj tylko przy zgłaszaniu błędu." },
			{ "ru", "Пишет еще 3 журнала рядом с NEC.log. Они быстро растут, так что включайте только для отчета." },
			{ "ja", "バグ報告用にNEC.logの隣へログを3つ追加します。すぐ大きくなるので普段はOFFに" },
			{ "zhhant", "在NEC.log旁另外寫入3個日誌，協助錯誤回報。日誌增長很快，平常遊玩時請保持關閉。" },
			{ "zhhans", "在NEC.log旁另外写入3个日志，用于错误报告。日志增长很快，平时游玩请保持关闭。" },
		};

		constexpr MenuRow ROWS[]{
			{ &Settings::bJam, {}, JAM_HELP },
			{ &Settings::bSpawnCondition, {}, SPAWN_CONDITION_HELP },
			{ &Settings::bVendorRepair, {}, VENDOR_REPAIR_HELP },
			{ &Settings::bLoadingTips, {}, LOADING_TIPS_HELP },
			{ &Settings::bHudCondition, {}, HUD_CONDITION_HELP },
			{ &Settings::bQuickContainer, {}, QUICK_CONTAINER_HELP },
			{ &Settings::bConfirmScroll, {}, CONFIRM_SCROLL_HELP },
			{ &Settings::bInspectPrice, {}, INSPECT_PRICE_HELP },
			{ &Settings::bTraceLogs, TRACE_LOGS_NAME, TRACE_LOGS_HELP },
		};
	}

	std::span<const MenuRow> MenuSwitches()
	{
		return ROWS;
	}
}
