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
			{ "fr", "Vos armes à feu à moins de la moitié de leur état peuvent s'enrayer jusqu'au rechargement. Désactivé\u00A0: aucun enrayage." },
			{ "de", "Deine Schusswaffen mit weniger als halbem Zustand können klemmen, bis du nachlädst. Aus: Sie klemmen nie." },
			{ "it", "Con condizioni sotto la metà, le tue armi da fuoco possono incepparsi finché non ricarichi. Se disattivato, nessun inceppamento." },
			{ "es", "Tus armas de fuego con menos de la mitad de estado pueden encasquillarse hasta que recargues. Desactivado: nunca se encasquillan." },
			{ "esmx", "Tus armas de fuego con menos de la mitad de estado pueden encasquillarse hasta que recargues. Desactivado: nunca se encasquillan." },
			{ "ptbr", "Suas armas de fogo abaixo da metade da condição podem emperrar até você recarregar. Desativado: nunca emperram." },
			{ "pl", "Twoja broń palna poniżej połowy stanu może się zaciąć. Przeładowanie usuwa zacięcie. Wyłączone: bez zacięć." },
			{ "ru", "Ваше стрелковое оружие в состоянии ниже половины может заклинить, и придется перезарядиться. Если отключено, оно не заклинивает." },
			{ "ja", "自分の銃は状態が半分未満になると弾詰まりすることがあり、リロードまで撃てません。OFFにすると、弾詰まりは起こりません" },
			{ "zhhant", "你的槍枝狀況低於一半時可能卡彈，重新裝填才能排除。關閉時，槍枝不會卡彈。" },
			{ "zhhans", "你的枪支状况低于一半时可能卡弹，重新装填才能排除。关闭时，枪支不会卡弹。" },
		};

		constexpr Line FIRE_RATE_HELP[]{
			{ "en", "Worn automatic weapons fire slower, for you and for NPCs. When off, they fire at full rate." },
			{ "fr", "Les armes automatiques usées tirent moins vite, pour vous comme pour les PNJ. Désactivé\u00A0: cadence normale." },
			{ "de", "Abgenutzte automatische Waffen feuern langsamer, bei dir und bei NPCs. Aus: Volle Feuerrate." },
			{ "it", "Le armi automatiche usurate sparano più lentamente, per te e per i PNG. Se disattivato, sparano a cadenza piena." },
			{ "es", "Las armas automáticas desgastadas disparan más despacio, las use quien las use. Desactivado: cadencia de tiro normal." },
			{ "esmx", "Las armas automáticas desgastadas disparan más despacio, las use quien las use. Desactivado: cadencia de tiro normal." },
			{ "ptbr", "Armas automáticas desgastadas atiram mais devagar, nas suas mãos e nas dos NPCs. Desativado: cadência total." },
			{ "pl", "Zużyta broń automatyczna strzela wolniej, także u NPC. Wyłączone: strzela z pełną szybkostrzelnością." },
			{ "ru", "Изношенное автоматическое оружие стреляет медленнее, у вас и у NPC. Если отключено, скорострельность полная." },
			{ "ja", "消耗したフルオートの武器は、誰が使っても発射速度が下がります。OFFにすると、本来の発射速度で撃てます" },
			{ "zhhant", "損耗的自動武器射速會變慢，你和其他角色皆然。關閉時，射速保持正常。" },
			{ "zhhans", "损耗的自动武器射速会变慢，你和其他角色都一样。关闭时，射速保持正常。" },
		};

		constexpr Line CRIT_METER_HELP[]{
			{ "en", "Worn weapons fill your critical meter slower and give NPCs fewer criticals. When off, criticals are normal." },
			{ "fr", "Une arme usée remplit moins vite votre jauge de critique et donne moins de critiques aux PNJ. Désactivé\u00A0: critiques normaux." },
			{ "de", "Abgenutzte Waffen füllen deinen Krit-Balken langsamer und geben NPCs weniger kritische Treffer. Aus: Normal." },
			{ "it", "Con armi usurate la tua barra dei critici sale più piano e i PNG fanno meno critici. Se disattivato, critici normali." },
			{ "es", "Las armas desgastadas llenan menos tu medidor de críticos y logran menos críticos en manos de otros. Desactivado: críticos normales." },
			{ "esmx", "Las armas desgastadas llenan menos tu medidor de críticos y logran menos críticos en manos de otros. Desactivado: críticos normales." },
			{ "ptbr", "Armas desgastadas enchem sua barra de crítico mais devagar e NPCs acertam menos críticos. Desativado: críticos normais." },
			{ "pl", "Zużyta broń wolniej wypełnia twój wskaźnik trafień krytycznych, a u NPC daje ich mniej. Wyłączone: jak zwykle." },
			{ "ru", "Изношенное оружие медленнее заполняет вашу шкалу КРИТ и дает NPC меньше критических атак. Если отключено, все как обычно." },
			{ "ja", "消耗した武器はクリティカルメーターが貯まりにくく、NPCのクリティカルも減ります。OFFにすると、クリティカルは通常どおりです" },
			{ "zhhant", "損耗的武器會讓你的爆擊條累積變慢，其他角色持用時也較少爆擊。關閉時，爆擊一切如常。" },
			{ "zhhans", "损耗的武器会让你的爆击条积累变慢，其他角色使用时也较少爆击。关闭时，爆击一切如常。" },
		};

		constexpr Line SPAWN_CONDITION_HELP[]{
			{ "en", "Weapons and armor you find are already worn, based on who had them. When off, they turn up new." },
			{ "fr", "Les armes et armures trouvées sont déjà usées, selon leur ancien porteur. Désactivé\u00A0: elles sont neuves." },
			{ "de", "Gefundene Waffen und Rüstungen sind schon abgenutzt, je nach Vorbesitzer. Aus: Sie sind neu." },
			{ "it", "Le armi e le armature che trovi sono già usurate, in base a chi le aveva. Se disattivato, sono nuove." },
			{ "es", "Las armas y armaduras que encuentras ya están desgastadas, según quién las tuviera. Desactivado: aparecen nuevas." },
			{ "esmx", "Las armas y armaduras que encuentras ya están desgastadas, según quién las tuviera. Desactivado: aparecen nuevas." },
			{ "ptbr", "Armas e armaduras que você encontra já vêm desgastadas, conforme quem as tinha. Desativado: vêm novas." },
			{ "pl", "Znaleziona broń i pancerze są już zużyte, zależnie od poprzedniego właściciela. Wyłączone: są nowe." },
			{ "ru", "Найденные оружие и броня уже изношены, смотря у кого они были. Если отключено, они попадаются новыми." },
			{ "ja", "見つけた武器とアーマーは、前の持ち主に応じて最初から消耗しています。OFFにすると、新品で見つかります" },
			{ "zhhant", "你找到的武器和裝甲已有損耗，程度取決於原本的持有者。關閉時，皆為全新。" },
			{ "zhhans", "你找到的武器和装甲已有损耗，程度取决于原来的持有者。关闭时，均为全新。" },
		};

		constexpr Line VENDOR_REPAIR_HELP[]{
			{ "en", "Traders who sell weapons, armor or clothing repair yours for caps. When off, only workbenches repair." },
			{ "fr", "Les marchands d'armes, d'armures ou de vêtements réparent les vôtres contre des capsules. Désactivé\u00A0: établis seulement." },
			{ "de", "Händler für Waffen, Rüstungen oder Kleidung reparieren diese gegen Kronkorken. Aus: Nur an Werkbänken." },
			{ "it", "Chi vende armi, armature o vestiti ripara i tuoi in cambio di tappi. Se disattivato, solo al banco da lavoro." },
			{ "es", "Los comerciantes de armas, armaduras o ropa reparan las tuyas por chapas. Desactivado: solo se repara en el banco de trabajo." },
			{ "esmx", "Los comerciantes de armas, armaduras o ropa reparan las tuyas por tapas. Desactivado: solo se repara en el banco de trabajo." },
			{ "ptbr", "Comerciantes de armas, armaduras ou roupas consertam as suas por tampas. Desativado: só as bancadas consertam." },
			{ "pl", "Handlarze bronią, pancerzami lub ubraniami naprawiają twoje rzeczy za kapsle. Wyłączone: tylko pracownie." },
			{ "ru", "Торговцы оружием, броней или одеждой чинят ваши вещи за крышки. Если отключено, ремонт только на верстаках." },
			{ "ja", "武器、アーマー、服を売る商人がキャップで修理してくれます。OFFにすると、作業台でしか修理できません" },
			{ "zhhant", "販售武器、裝甲或服飾的商人會收取瓶蓋，修理你的同類裝備。關閉時，只有工作台能修理。" },
			{ "zhhans", "出售武器、装甲或服饰的商人会收取瓶盖，修理你的同类装备。关闭时，只有工作台能修理。" },
		};

		constexpr Line LOADING_TIPS_HELP[]{
			{ "en", "Adds tips about wear, repairs and loot to the loading screens. When off, none of them show." },
			{ "fr", "Ajoute des astuces sur l'usure, les réparations et le butin aux écrans de chargement. Désactivé\u00A0: aucune astuce." },
			{ "de", "Zeigt Tipps zu Abnutzung, Reparaturen und Beute auf Ladebildschirmen. Aus: Keine Tipps." },
			{ "it", "Aggiunge suggerimenti su usura, riparazioni e bottino alle schermate di caricamento. Se disattivato, nessuno." },
			{ "es", "Añade consejos sobre desgaste, reparaciones y botín a las pantallas de carga. Desactivado: no aparece ninguno." },
			{ "esmx", "Agrega consejos sobre desgaste, reparaciones y botín a las pantallas de carga. Desactivado: no aparece ninguno." },
			{ "ptbr", "Adiciona dicas sobre desgaste, consertos e saque às telas de carregamento. Desativado: nenhuma aparece." },
			{ "pl", "Dodaje porady o zużyciu, naprawach i łupach na ekranach wczytywania. Wyłączone: nie pojawiają się." },
			{ "ru", "Добавляет на экраны загрузки советы об износе, ремонте и трофеях. Если отключено, их нет." },
			{ "ja", "消耗、修理、戦利品についてのヒントをロード画面に追加します。OFFにすると、どれも表示されません" },
			{ "zhhant", "在載入畫面加入損耗、修理和戰利品的相關提示。關閉時，不會顯示這些提示。" },
			{ "zhhans", "在载入画面加入损耗、修理和战利品的相关提示。关闭时，不会显示这些提示。" },
		};

		constexpr Line HUD_CONDITION_HELP[]{
			{ "en", "Shows condition bars by the ammo counter and in power armor. When off, item cards still show condition." },
			{ "fr", "Affiche des barres d'état près du compteur de munitions et en armure assistée. Désactivé\u00A0: l'état reste sur les fiches." },
			{ "de", "Zeigt Zustandsbalken an der Munitionsanzeige und in der Powerrüstung. Aus: Nur in den Gegenstandsinfos." },
			{ "it", "Mostra le condizioni con barre accanto alle munizioni e nell'armatura atomica. Se disattivato, solo nelle schede." },
			{ "es", "Muestra barras de estado junto al contador de munición y en la servoarmadura. Desactivado: el estado sigue en las fichas de objetos." },
			{ "esmx", "Muestra barras de estado junto al contador de munición y en la servoarmadura. Desactivado: el estado sigue en las fichas de objetos." },
			{ "ptbr", "Mostra barras de condição junto ao contador de munição e na Armadura Potente. Desativado: só nas fichas dos itens." },
			{ "pl", "Pokazuje paski stanu przy liczniku amunicji i w pancerzu wspomaganym. Wyłączone: stan widać na kartach przedmiotów." },
			{ "ru", "Показывает полосы состояния у счетчика патронов и в силовой броне. Если отключено, состояние все равно видно в карточках предметов." },
			{ "ja", "弾薬カウンターの横とパワーアーマー着用時に状態バーを表示します。OFFにしても、アイテム情報には状態が表示されます" },
			{ "zhhant", "在彈藥計數旁和動力裝甲中顯示狀況條。關閉時，物品資訊仍會顯示狀況。" },
			{ "zhhans", "在弹药计数旁和动力装甲中显示状况条。关闭时，物品信息仍会显示状况。" },
		};

		constexpr Line QUICK_CONTAINER_HELP[]{
			{ "en", "Shows condition in the list that pops up when you look at a container or body. When off, it shows none." },
			{ "fr", "Affiche l'état dans la liste qui apparaît quand vous visez un conteneur ou un corps. Désactivé\u00A0: état non affiché." },
			{ "de", "Zeigt den Zustand in der Liste, die beim Blick auf Behälter oder Leichen erscheint. Aus: Kein Zustand." },
			{ "it", "Mostra le condizioni nell'elenco che appare guardando contenitori e corpi. Se disattivato, non le mostra." },
			{ "es", "Muestra el estado en la lista que aparece al mirar un contenedor o un cadáver. Desactivado: no lo muestra." },
			{ "esmx", "Muestra el estado en la lista que aparece al mirar un contenedor o un cadáver. Desactivado: no lo muestra." },
			{ "ptbr", "Mostra a condição na lista que surge ao olhar para um contêiner ou corpo. Desativado: não a mostra." },
			{ "pl", "Pokazuje stan w podglądzie pojemnika lub ciała, na które patrzysz. Wyłączone: podgląd go nie pokazuje." },
			{ "ru", "Показывает состояние в списке, который всплывает при взгляде на ящик или тело. Если отключено, его там нет." },
			{ "ja", "容器や死体に照準を合わせたときの一覧に状態を表示します。OFFにすると、表示されません" },
			{ "zhhant", "看向容器或屍體時，彈出的清單會顯示狀況。關閉時則不顯示。" },
			{ "zhhans", "看向容器或尸体时，弹出的清单会显示状况。关闭时则不显示。" },
		};

		// "Base game fix" reads the same in both fixes.
		constexpr Line CONFIRM_SCROLL_HELP[]{
			{ "en", "Base game fix. The workbench confirm box shows more of a long list and scrolls. When off, it can stay small." },
			{ "fr", "Correctif du jeu de base. À l'établi, la fenêtre de confirmation s'agrandit et défile. Désactivé\u00A0: elle peut rester petite." },
			{ "de", "Grundspiel-Fix. Das Bestätigungsfenster der Werkbank wird größer und scrollt lange Listen. Aus: Es kann klein bleiben." },
			{ "it", "Correzione del gioco base. Al banco da lavoro, la finestra di conferma mostra più righe e scorre. Se disattivato, può restare piccola." },
			{ "es", "Arreglo del juego base. La ventana de confirmación del banco de trabajo crece y permite desplazar las listas largas. Desactivado: puede seguir pequeña." },
			{ "esmx", "Arreglo del juego base. La ventana de confirmación del banco de trabajo crece y permite desplazar las listas largas. Desactivado: puede seguir pequeña." },
			{ "ptbr", "Correção do jogo base. A caixa de confirmação da bancada cresce com listas longas e rola. Desativado: pode ficar pequena." },
			{ "pl", "Poprawka błędu gry. Okno potwierdzenia w pracowni jest większe i pozwala przewijać długie listy. Wyłączone: może zostać małe." },
			{ "ru", "Исправление игры. Окно подтверждения верстака больше и с прокруткой. Если отключено, оно может остаться маленьким." },
			{ "ja", "ゲームの不具合修正。作業台の確認ウィンドウが広がり、長いリストをスクロールできます。OFFにすると、小さいままの場合があります" },
			{ "zhhant", "原版遊戲修正。工作台確認視窗會放大，顯示更多長清單內容並可捲動。關閉時，視窗可能維持原本大小。" },
			{ "zhhans", "原版游戏修正。工作台确认窗口会放大，显示更多长清单内容并可滚动。关闭时，窗口可能保持原本大小。" },
		};

		constexpr Line INSPECT_PRICE_HELP[]{
			{ "en", "Base game fix. At a trader, inspecting an item shows the right buy or sell price. When off, it can be wrong." },
			{ "fr", "Correctif du jeu de base. Chez un marchand, Examiner affiche le bon prix d'achat ou de vente. Désactivé\u00A0: prix parfois faux." },
			{ "de", "Grundspiel-Fix. Beim Händler zeigt Untersuchen den richtigen Kauf- oder Verkaufspreis. Aus: Er kann falsch sein." },
			{ "it", "Correzione del gioco base. Dal commerciante, Ispeziona dà il prezzo giusto. Se disattivato, può sbagliare." },
			{ "es", "Arreglo del juego base. Al comerciar, Inspeccionar muestra el precio correcto de compra o venta. Desactivado: puede ser erróneo." },
			{ "esmx", "Arreglo del juego base. Al comerciar, Inspeccionar muestra el precio correcto de compra o venta. Desactivado: puede ser erróneo." },
			{ "ptbr", "Correção do jogo base. Ao negociar, Inspecionar mostra o preço certo de compra ou venda. Desativado: pode errar." },
			{ "pl", "Poprawka błędu gry. Przy badaniu u handlarza widać właściwą cenę kupna lub sprzedaży. Wyłączone: bywa błędna." },
			{ "ru", "Исправление игры. Осмотр у торговца показывает верную цену покупки или продажи. Если отключено, возможна ошибка." },
			{ "ja", "ゲームの不具合修正。取引中の調べる画面に正しい売買価格を表示します。OFFにすると、誤った価格が表示されることがあります" },
			{ "zhhant", "原版遊戲修正。與商人交易時，檢查物品會顯示正確的買價或賣價。關閉時，價格可能有誤。" },
			{ "zhhans", "原版游戏修正。与商人交易时，检查物品会显示正确的买价或卖价。关闭时，价格可能有误。" },
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
			{ "fr", "Écrit 3 journaux de plus à côté de NEC.log pour les rapports de bug. Ils grossissent vite, alors ne l'activez que pour signaler un bug." },
			{ "de", "Schreibt 3 Protokolle für Fehlerberichte neben NEC.log. Sie wachsen schnell, also nur für einen Fehlerbericht einschalten." },
			{ "it", "Scrive 3 registri in più accanto a NEC.log per segnalare bug. Crescono in fretta, quindi attivalo solo per segnalare un bug." },
			{ "es", "Escribe 3 registros más junto a NEC.log para informar de errores. Crecen rápido, así que actívalo solo para informar de un error." },
			{ "esmx", "Escribe 3 registros más junto a NEC.log para reportar errores. Crecen rápido, así que actívalo solo para reportar un error." },
			{ "ptbr", "Grava mais 3 registros ao lado do NEC.log para relatar bugs. Crescem rápido, então só ative para relatar um bug." },
			{ "pl", "Zapisuje 3 dodatkowe dzienniki obok NEC.log do zgłoszeń błędów. Szybko rosną, więc włączaj tylko przy zgłaszaniu błędu." },
			{ "ru", "Пишет еще 3 журнала рядом с NEC.log для отчетов об ошибках. Они быстро растут, так что в обычной игре не включайте." },
			{ "ja", "バグ報告用に、NEC.logの隣へログを3つ追加で書き込みます。すぐ大きくなるので、普段はOFFにしてください" },
			{ "zhhant", "在NEC.log旁另外寫入3個日誌，協助錯誤回報。日誌增長很快，平常遊玩時請保持關閉。" },
			{ "zhhans", "在NEC.log旁另外写入3个日志，用于错误报告。日志增长很快，平时游玩请保持关闭。" },
		};

		constexpr MenuRow ROWS[]{
			{ &Settings::bJam, {}, JAM_HELP },
			{ &Settings::bFireRate, {}, FIRE_RATE_HELP },
			{ &Settings::bCritMeter, {}, CRIT_METER_HELP },
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
