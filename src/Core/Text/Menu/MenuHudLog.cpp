#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the HUD numbers and the log level on the MCM
// page, see MenuLine in Text.h, and the HUD bar sliders' Default sentence.
namespace Text
{
	namespace
	{
		// sLogLevel is the one setting that is no Live one, so its row names
		// the key itself.
		constexpr Settings::Named LOG_LEVEL{ "Log", "sLogLevel" };

		// The 2 HUD bar sliders show where the bar starts, see Mcm/HudBar.cpp.
		// The page puts HUD_BAR_DEFAULT after their help.
		constexpr Line HUD_BAR_X_NAME[]{
			{ "en", "HUD bar from left" },
			{ "fr", "Barre de l'ATH depuis la gauche" },
			{ "de", "HUD-Balken von links" },
			{ "it", "Barra interfaccia da sinistra" },
			{ "es", "Barra del HUD desde la izquierda" },
			{ "esmx", "Barra del HUD desde la izquierda" },
			{ "ptbr", "Barra do HUD a partir da esquerda" },
			{ "pl", "Pasek interfejsu od lewej" },
			{ "ru", "Полоса интерфейса от левого края" },
			{ "ja", "HUDバーの画面左端からの位置" },
			{ "zhhant", "狀態欄狀況條距左緣" },
			{ "zhhans", "状态栏状况条距左边缘" },
		};

		constexpr Line HUD_BAR_X_HELP[]{
			{ "en", "Where the ammo counter's bar and CND word start, from the left. Same at any resolution." },
			{ "fr", "Où commencent la barre des munitions et son mot, depuis la gauche, à toute résolution." },
			{ "de", "Wo Balken und Wort an der Munitionsanzeige beginnen, von links, bei jeder Auflösung." },
			{ "it", "Dove iniziano barra e parola accanto alle munizioni, da sinistra, a ogni risoluzione." },
			{ "es", "Dónde empiezan la barra y la palabra CON, desde la izquierda, en toda resolución." },
			{ "esmx", "Dónde empiezan la barra y la palabra CON, desde la izquierda, en toda resolución." },
			{ "ptbr", "Onde começam a barra da munição e sua palavra, da esquerda, em qualquer resolução." },
			{ "pl", "Gdzie zaczyna się pasek przy amunicji z napisem, od lewej, w każdej rozdzielczości." },
			{ "ru", "Начало полосы с надписью у патронов, слева, при любом разрешении." },
			{ "ja", "弾薬のバーと文字の左端からの位置で、解像度によらず同じです" },
			{ "zhhant", "彈藥計數旁狀況條與文字的起點，從左緣算起，任何解析度皆同。" },
			{ "zhhans", "弹药计数旁状况条与文字的起点，从左边缘算起，任何分辨率皆同。" },
		};

		constexpr Line HUD_BAR_Y_NAME[]{
			{ "en", "HUD bar from top" },
			{ "fr", "Barre de l'ATH depuis le haut" },
			{ "de", "HUD-Balken von oben" },
			{ "it", "Barra interfaccia dall'alto" },
			{ "es", "Barra del HUD desde arriba" },
			{ "esmx", "Barra del HUD desde arriba" },
			{ "ptbr", "Barra do HUD a partir do topo" },
			{ "pl", "Pasek interfejsu od góry" },
			{ "ru", "Полоса интерфейса от верхнего края" },
			{ "ja", "HUDバーの画面上端からの位置" },
			{ "zhhant", "狀態欄狀況條距上緣" },
			{ "zhhans", "状态栏状况条距上边缘" },
		};

		constexpr Line HUD_BAR_Y_HELP[]{
			{ "en", "Where the ammo counter's bar and CND word start, from the top. Same at any resolution." },
			{ "fr", "Où commencent la barre des munitions et son mot, depuis le haut, à toute résolution." },
			{ "de", "Wo Balken und Wort an der Munitionsanzeige beginnen, von oben, bei jeder Auflösung." },
			{ "it", "Dove iniziano barra e parola accanto alle munizioni, dall'alto, a ogni risoluzione." },
			{ "es", "Dónde empiezan la barra y la palabra CON, desde arriba, en toda resolución." },
			{ "esmx", "Dónde empiezan la barra y la palabra CON, desde arriba, en toda resolución." },
			{ "ptbr", "Onde começam a barra da munição e sua palavra, do topo, em qualquer resolução." },
			{ "pl", "Gdzie zaczyna się pasek przy amunicji z napisem, od góry, w każdej rozdzielczości." },
			{ "ru", "Начало полосы с надписью у патронов, сверху, при любом разрешении." },
			{ "ja", "弾薬のバーと文字の上端からの位置で、解像度によらず同じです" },
			{ "zhhant", "彈藥計數旁狀況條與文字的起點，從上緣算起，任何解析度皆同。" },
			{ "zhhans", "弹药计数旁状况条与文字的起点，从上边缘算起，任何分辨率皆同。" },
		};

		constexpr Line HUD_BAR_DEFAULT[]{
			{ "en", "Default: {0}." },
			{ "fr", "Par défaut\u00A0: {0}." },
			{ "de", "Standard: {0}." },
			{ "it", "Predefinito: {0}." },
			{ "es", "Predeterminado: {0}." },
			{ "esmx", "Predeterminado: {0}." },
			{ "ptbr", "Padrão: {0}." },
			{ "pl", "Domyślnie: {0}." },
			{ "ru", "По умолчанию: {0}." },
			{ "ja", "初期値は{0}です" },
			{ "zhhant", "預設為{0}。" },
			{ "zhhans", "默认为{0}。" },
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
			{ "fr", "Déplace la barre en armure assistée\u00A0: à droite au-dessus de 0, à gauche en dessous. Hors écran, cachée." },
			{ "de", "Powerrüstungs-Balken: über 0 nach rechts, unter 0 nach links. Außerhalb des Bilds verschwindet er." },
			{ "it", "Sposta la barra nell'armatura atomica: a destra sopra 0, a sinistra sotto 0. Fuori schermo si nasconde." },
			{ "es", "Barra en la servoarmadura: positivo a la derecha, negativo a la izquierda. Fuera de la pantalla se oculta." },
			{ "esmx", "Barra en la servoarmadura: positivo a la derecha, negativo a la izquierda. Fuera de la pantalla se oculta." },
			{ "ptbr", "Move a barra na Armadura Potente: acima de 0 à direita, abaixo de 0 à esquerda. Fora da tela, ela some." },
			{ "pl", "Przesuwa pasek w pancerzu wspomaganym: powyżej 0 w prawo, poniżej 0 w lewo. Poza ekranem znika." },
			{ "ru", "Сдвигает полосу в силовой броне: выше 0 вправо, ниже 0 влево. За краем экрана полоса скрывается." },
			{ "ja", "パワーアーマーのバーをプラスで右、マイナスで左へ動かします。画面外で隠れます" },
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
			{ "fr", "Déplace la barre en armure assistée\u00A0: en haut au-dessus de 0, en bas en dessous. Hors écran, cachée." },
			{ "de", "Powerrüstungs-Balken: über 0 nach oben, unter 0 nach unten. Außerhalb des Bilds verschwindet er." },
			{ "it", "Sposta la barra nell'armatura atomica: in alto sopra 0, in basso sotto 0. Fuori schermo si nasconde." },
			{ "es", "Barra en la servoarmadura: positivo hacia arriba, negativo hacia abajo. Fuera de la pantalla se oculta." },
			{ "esmx", "Barra en la servoarmadura: positivo hacia arriba, negativo hacia abajo. Fuera de la pantalla se oculta." },
			{ "ptbr", "Move a barra na Armadura Potente: acima de 0 sobe, abaixo de 0 desce. Além da borda da tela, ela some." },
			{ "pl", "Przesuwa pasek w pancerzu wspomaganym: powyżej 0 w górę, poniżej 0 w dół. Poza ekranem znika." },
			{ "ru", "Сдвигает полосу в силовой броне: выше 0 вверх, ниже 0 вниз. За краем экрана полоса скрывается." },
			{ "ja", "パワーアーマーのバーをプラスで上、マイナスで下へ動かします。画面外で隠れます" },
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
			{ "fr", "Détail de NEC.log, dans Documents\\My Games\\Fallout4\\F4SE. Choisissez debug pour signaler un bug." },
			{ "de", "Wie viel NEC.log schreibt, in Documents\\My Games\\Fallout4\\F4SE. Wähle debug für Fehlerberichte." },
			{ "it", "Quanto scrive NEC.log, in Documents\\My Games\\Fallout4\\F4SE. Scegli debug quando segnali un bug." },
			{ "es", "Cuánto escribe NEC.log, en Documents\\My Games\\Fallout4\\F4SE. Elige debug al informar de un error." },
			{ "esmx", "Cuánto escribe NEC.log, en Documents\\My Games\\Fallout4\\F4SE. Elige debug al reportar un error." },
			{ "ptbr", "Quanto o NEC.log grava, em Documents\\My Games\\Fallout4\\F4SE. Escolha debug ao relatar um bug." },
			{ "pl", "Ile zapisuje NEC.log, w Documents\\My Games\\Fallout4\\F4SE. Przy zgłaszaniu błędu wybierz debug." },
			{ "ru", "Сколько пишет NEC.log в Documents\\My Games\\Fallout4\\F4SE. Для отчета об ошибке выберите debug." },
			{ "ja", "NEC.log(Documents\\My Games\\Fallout4\\F4SE)の詳細度です。バグ報告はdebugで" },
			{ "zhhant", "NEC.log（位於Documents\\My Games\\Fallout4\\F4SE）的詳細程度。回報錯誤時請選debug。" },
			{ "zhhans", "NEC.log（位于Documents\\My Games\\Fallout4\\F4SE）的详细程度。报告错误时请选debug。" },
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

	std::string MenuHudBarDefault(std::string_view a_help, std::int32_t a_number)
	{
		return Sentences(a_help, Say(HUD_BAR_DEFAULT, a_number), Out::kPage);
	}
}
