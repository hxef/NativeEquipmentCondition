#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the HUD numbers and the log level on the MCM
// page, see MenuLine in Text.h.
namespace Text
{
	namespace
	{
		// sLogLevel is the one setting that is no Live one, so its row names
		// the key itself.
		constexpr Settings::Named LOG_LEVEL{ "Log", "sLogLevel" };

		// The divider line is the game's own line between the rounds in the
		// gun and the rounds carried, which the bar covers at 0.
		constexpr Line HUD_BAR_X_NAME[]{
			{ "en", "HUD bar left or right" },
			{ "fr", "Barre de l'ATH gauche ou droite" },
			{ "de", "HUD-Balken links oder rechts" },
			{ "it", "Barra interfaccia in orizzontale" },
			{ "es", "Barra del HUD a izquierda o derecha" },
			{ "esmx", "Barra del HUD a izquierda o derecha" },
			{ "ptbr", "Barra do HUD à esquerda ou direita" },
			{ "pl", "Pasek interfejsu w poziomie" },
			{ "ru", "Полоса интерфейса по горизонтали" },
			{ "ja", "HUDバーの左右位置" },
			{ "zhhant", "狀態欄狀況條左右位置" },
			{ "zhhans", "状态栏状况条左右位置" },
		};

		constexpr Line HUD_BAR_X_HELP[]{
			{ "en", "Moves the bar by the ammo counter: right above 0, left below 0. Away from 0, the game's divider line shows again." },
			{ "fr", "Déplace la barre du compteur de munitions\u00A0: à droite au-dessus de 0, à gauche en dessous. Sauf à 0, le séparateur du jeu réapparaît." },
			{ "de", "Schiebt den Balken an der Munitionsanzeige: über 0 nach rechts, unter 0 nach links. Nur bei 0 verdeckt er die Trennlinie." },
			{ "it", "Sposta la barra accanto alle munizioni: a destra sopra 0, a sinistra sotto 0. Se non è a 0, riappare la linea divisoria." },
			{ "es", "Mueve la barra junto a la munición: positivo a la derecha, negativo a la izquierda. Si no es 0, reaparece la línea divisoria del juego." },
			{ "esmx", "Mueve la barra junto a la munición: positivo a la derecha, negativo a la izquierda. Si no es 0, reaparece la línea divisoria del juego." },
			{ "ptbr", "Move a barra junto ao contador de munição: acima de 0 à direita, abaixo de 0 à esquerda. Fora do 0, o divisor do jogo reaparece." },
			{ "pl", "Przesuwa pasek przy liczniku amunicji: powyżej 0 w prawo, poniżej 0 w lewo. Przy wartości innej niż 0 znów widać linię podziału z gry." },
			{ "ru", "Сдвигает полосу у счетчика патронов: выше 0 вправо, ниже 0 влево. Если не 0, снова видна разделительная линия игры." },
			{ "ja", "弾薬カウンターのバーを、プラスで右へ、マイナスで左へ動かします。0以外では、本来の区切り線も表示されます" },
			{ "zhhant", "移動彈藥計數旁的狀況條：大於0向右，小於0向左。不為0時，遊戲原有的分隔線會重新顯示。" },
			{ "zhhans", "移动弹药计数旁的状况条：大于0向右，小于0向左。不为0时，游戏原有的分隔线会重新显示。" },
		};

		constexpr Line HUD_BAR_Y_NAME[]{
			{ "en", "HUD bar up or down" },
			{ "fr", "Barre de l'ATH haut ou bas" },
			{ "de", "HUD-Balken oben oder unten" },
			{ "it", "Barra interfaccia in verticale" },
			{ "es", "Barra del HUD arriba o abajo" },
			{ "esmx", "Barra del HUD arriba o abajo" },
			{ "ptbr", "Barra do HUD para cima ou baixo" },
			{ "pl", "Pasek interfejsu w pionie" },
			{ "ru", "Полоса интерфейса по вертикали" },
			{ "ja", "HUDバーの上下位置" },
			{ "zhhant", "狀態欄狀況條上下位置" },
			{ "zhhans", "状态栏状况条上下位置" },
		};

		constexpr Line HUD_BAR_Y_HELP[]{
			{ "en", "Moves the bar by the ammo counter: up above 0, down below 0. Away from 0, the game's divider line shows again." },
			{ "fr", "Déplace la barre du compteur de munitions\u00A0: en haut au-dessus de 0, en bas en dessous. Sauf à 0, le séparateur du jeu réapparaît." },
			{ "de", "Schiebt den Balken an der Munitionsanzeige: über 0 nach oben, unter 0 nach unten. Nur bei 0 verdeckt er die Trennlinie." },
			{ "it", "Sposta la barra accanto alle munizioni: in alto sopra 0, in basso sotto 0. Se non è a 0, riappare la linea divisoria." },
			{ "es", "Mueve la barra junto a la munición: positivo hacia arriba, negativo hacia abajo. Si no es 0, reaparece la línea divisoria del juego." },
			{ "esmx", "Mueve la barra junto a la munición: positivo hacia arriba, negativo hacia abajo. Si no es 0, reaparece la línea divisoria del juego." },
			{ "ptbr", "Move a barra junto ao contador de munição: acima de 0 sobe, abaixo de 0 desce. Fora do 0, o divisor do jogo reaparece." },
			{ "pl", "Przesuwa pasek przy liczniku amunicji: powyżej 0 w górę, poniżej 0 w dół. Przy wartości innej niż 0 znów widać linię podziału z gry." },
			{ "ru", "Сдвигает полосу у счетчика патронов: выше 0 вверх, ниже 0 вниз. Если не 0, снова видна разделительная линия игры." },
			{ "ja", "弾薬カウンターのバーを、プラスで上へ、マイナスで下へ動かします。0以外では、本来の区切り線も表示されます" },
			{ "zhhant", "移動彈藥計數旁的狀況條：大於0向上，小於0向下。不為0時，遊戲原有的分隔線會重新顯示。" },
			{ "zhhans", "移动弹药计数旁的状况条：大于0向上，小于0向下。不为0时，游戏原有的分隔线会重新显示。" },
		};

		constexpr Line POWER_ARMOR_BAR_X_NAME[]{
			{ "en", "Power armor bar left or right" },
			{ "fr", "Barre d'armure assistée gauche ou droite" },
			{ "de", "Powerrüstungs-Balken links oder rechts" },
			{ "it", "Barra armatura atomica in orizzontale" },
			{ "es", "Barra de la servoarmadura a izquierda o derecha" },
			{ "esmx", "Barra de la servoarmadura a izquierda o derecha" },
			{ "ptbr", "Barra da Armadura Potente à esquerda ou direita" },
			{ "pl", "Pasek pancerza wspomaganego w poziomie" },
			{ "ru", "Полоса силовой брони по горизонтали" },
			{ "ja", "パワーアーマーのバーの左右位置" },
			{ "zhhant", "動力裝甲狀況條左右位置" },
			{ "zhhans", "动力装甲状况条左右位置" },
		};

		constexpr Line POWER_ARMOR_BAR_X_HELP[]{
			{ "en", "Moves the bar in power armor: right above 0, left below 0. Past the edge of the screen it hides." },
			{ "fr", "Déplace la barre en armure assistée\u00A0: à droite au-dessus de 0, à gauche en dessous. Hors de l'écran, elle disparaît." },
			{ "de", "Schiebt den Balken in der Powerrüstung: über 0 nach rechts, unter 0 nach links. Über den Bildrand hinaus verschwindet er." },
			{ "it", "Sposta la barra nell'armatura atomica: a destra sopra 0, a sinistra sotto 0. Oltre il bordo dello schermo si nasconde." },
			{ "es", "Mueve la barra en la servoarmadura: positivo a la derecha, negativo a la izquierda. Si sale de la pantalla, se oculta." },
			{ "esmx", "Mueve la barra en la servoarmadura: positivo a la derecha, negativo a la izquierda. Si sale de la pantalla, se oculta." },
			{ "ptbr", "Move a barra na Armadura Potente: acima de 0 à direita, abaixo de 0 à esquerda. Além da borda da tela, ela some." },
			{ "pl", "Przesuwa pasek w pancerzu wspomaganym: powyżej 0 w prawo, poniżej 0 w lewo. Za krawędzią ekranu pasek znika." },
			{ "ru", "Сдвигает полосу в силовой броне: выше 0 вправо, ниже 0 влево. За краем экрана полоса скрывается." },
			{ "ja", "パワーアーマー着用時のバーを、プラスで右へ、マイナスで左へ動かします。画面の端を越えると隠れます" },
			{ "zhhant", "移動動力裝甲中的狀況條：大於0向右，小於0向左。超出螢幕邊緣時會隱藏。" },
			{ "zhhans", "移动动力装甲中的状况条：大于0向右，小于0向左。超出屏幕边缘时会隐藏。" },
		};

		constexpr Line POWER_ARMOR_BAR_Y_NAME[]{
			{ "en", "Power armor bar up or down" },
			{ "fr", "Barre d'armure assistée haut ou bas" },
			{ "de", "Powerrüstungs-Balken oben oder unten" },
			{ "it", "Barra armatura atomica in verticale" },
			{ "es", "Barra de la servoarmadura arriba o abajo" },
			{ "esmx", "Barra de la servoarmadura arriba o abajo" },
			{ "ptbr", "Barra da Armadura Potente para cima ou baixo" },
			{ "pl", "Pasek pancerza wspomaganego w pionie" },
			{ "ru", "Полоса силовой брони по вертикали" },
			{ "ja", "パワーアーマーのバーの上下位置" },
			{ "zhhant", "動力裝甲狀況條上下位置" },
			{ "zhhans", "动力装甲状况条上下位置" },
		};

		constexpr Line POWER_ARMOR_BAR_Y_HELP[]{
			{ "en", "Moves the bar in power armor: up above 0, down below 0. Past the edge of the screen it hides." },
			{ "fr", "Déplace la barre en armure assistée\u00A0: en haut au-dessus de 0, en bas en dessous. Hors de l'écran, elle disparaît." },
			{ "de", "Schiebt den Balken in der Powerrüstung: über 0 nach oben, unter 0 nach unten. Über den Bildrand hinaus verschwindet er." },
			{ "it", "Sposta la barra nell'armatura atomica: in alto sopra 0, in basso sotto 0. Oltre il bordo dello schermo si nasconde." },
			{ "es", "Mueve la barra en la servoarmadura: positivo hacia arriba, negativo hacia abajo. Si sale de la pantalla, se oculta." },
			{ "esmx", "Mueve la barra en la servoarmadura: positivo hacia arriba, negativo hacia abajo. Si sale de la pantalla, se oculta." },
			{ "ptbr", "Move a barra na Armadura Potente: acima de 0 sobe, abaixo de 0 desce. Além da borda da tela, ela some." },
			{ "pl", "Przesuwa pasek w pancerzu wspomaganym: powyżej 0 w górę, poniżej 0 w dół. Za krawędzią ekranu pasek znika." },
			{ "ru", "Сдвигает полосу в силовой броне: выше 0 вверх, ниже 0 вниз. За краем экрана полоса скрывается." },
			{ "ja", "パワーアーマー着用時のバーを、プラスで上へ、マイナスで下へ動かします。画面の端を越えると隠れます" },
			{ "zhhant", "移動動力裝甲中的狀況條：大於0向上，小於0向下。超出螢幕邊緣時會隱藏。" },
			{ "zhhans", "移动动力装甲中的状况条：大于0向上，小于0向下。超出屏幕边缘时会隐藏。" },
		};

		constexpr Line LOG_LEVEL_NAME[]{
			{ "en", "Log detail" },
			{ "fr", "Détail du journal" },
			{ "de", "Protokolldetails" },
			{ "it", "Dettaglio del registro" },
			{ "es", "Detalle del registro" },
			{ "esmx", "Detalle del registro" },
			{ "ptbr", "Nível de detalhe do registro" },
			{ "pl", "Szczegółowość dziennika" },
			{ "ru", "Подробность журнала" },
			{ "ja", "ログの詳細度" },
			{ "zhhant", "日誌詳細程度" },
			{ "zhhans", "日志详细程度" },
		};

		// The path and debug stay as they are in every language, debug being
		// a word NEC.ini takes.
		constexpr Line LOG_LEVEL_HELP[]{
			{ "en", "How much NEC.log writes, in Documents\\My Games\\Fallout4\\F4SE. Pick debug when you report a bug." },
			{ "fr", "Le niveau de détail de NEC.log, dans Documents\\My Games\\Fallout4\\F4SE. Choisissez debug pour signaler un bug." },
			{ "de", "Wie viel NEC.log schreibt, in Documents\\My Games\\Fallout4\\F4SE. Wähle debug, wenn du einen Fehler meldest." },
			{ "it", "Quanto scrive NEC.log, in Documents\\My Games\\Fallout4\\F4SE. Scegli debug quando segnali un bug." },
			{ "es", "Cuánto escribe NEC.log, en Documents\\My Games\\Fallout4\\F4SE. Elige debug al informar de un error." },
			{ "esmx", "Cuánto escribe NEC.log, en Documents\\My Games\\Fallout4\\F4SE. Elige debug al reportar un error." },
			{ "ptbr", "Quanto o NEC.log grava, em Documents\\My Games\\Fallout4\\F4SE. Escolha debug ao relatar um bug." },
			{ "pl", "Ile zapisuje NEC.log, w Documents\\My Games\\Fallout4\\F4SE. Przy zgłaszaniu błędu wybierz debug." },
			{ "ru", "Сколько пишет NEC.log в Documents\\My Games\\Fallout4\\F4SE. Для отчета об ошибке выберите debug." },
			{ "ja", "NEC.log(Documents\\My Games\\Fallout4\\F4SE)に書き込む量です。バグを報告するときはdebugを選んでください" },
			{ "zhhant", "NEC.log（位於Documents\\My Games\\Fallout4\\F4SE）寫入的詳細程度。回報錯誤時請選擇debug。" },
			{ "zhhans", "NEC.log（位于Documents\\My Games\\Fallout4\\F4SE）写入的详细程度。报告错误时请选择debug。" },
		};

		constexpr MenuRow ROWS[]{
			{ &Settings::fHudBarX, HUD_BAR_X_NAME, HUD_BAR_X_HELP },
			{ &Settings::fHudBarY, HUD_BAR_Y_NAME, HUD_BAR_Y_HELP },
			{ &Settings::fPowerArmorBarX, POWER_ARMOR_BAR_X_NAME, POWER_ARMOR_BAR_X_HELP },
			{ &Settings::fPowerArmorBarY, POWER_ARMOR_BAR_Y_NAME, POWER_ARMOR_BAR_Y_HELP },
			{ &LOG_LEVEL, LOG_LEVEL_NAME, LOG_LEVEL_HELP },
		};
	}

	std::span<const MenuRow> MenuHudLog()
	{
		return ROWS;
	}
}
