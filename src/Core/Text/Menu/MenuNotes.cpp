#include "Core/Text/Text.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Text/Lines.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The grey lines of the MCM page about parts left to other mods, see
// UI/Mcm/Notes.cpp, and the words that join a list. A DLL is named by its file
// name in every language. NEC.log reads the English rows of the part lines.
namespace Text
{
	namespace
	{
		// The first lines of a block's note, or of a hidden switch's in its
		// place. {0} is a part, or a part with its pieces in brackets. {1}
		// reads right after "left to", so OWNER_MOD takes that case.
		constexpr Line PART_OFF[]{
			{ "en", "{0}: off, left to {1}." },
			{ "fr", "{0}\u00A0: désactivé, laissé à {1}." },
			{ "de", "{0}: aus, {1} überlassen." },
			{ "it", "{0}: disattivato, parte lasciata a {1}." },
			{ "es", "{0}: desactivado, en manos de {1}." },
			{ "esmx", "{0}: desactivado, en manos de {1}." },
			{ "ptbr", "{0}: desativado, a cargo de {1}." },
			{ "pl", "{0}: wyłączone na rzecz {1}." },
			{ "ru", "{0}: отключено, уступлено {1}." },
			{ "ja", "{0}: 無効。{1}に任せています" },
			{ "zhhant", "{0}：已停用，交由{1}處理。" },
			{ "zhhans", "{0}：已停用，交由{1}处理。" },
		};

		// A switch or a piece that went off with a part it rides on, as
		// Jamming with Gun wear from firing.
		constexpr Line PART_SINCE[]{
			{ "en", "{0}: off, since {1} is left to {2}." },
			{ "fr", "{0}\u00A0: désactivé, car la partie {1} est laissée à {2}." },
			{ "de", "{0}: aus, da die Funktion {1} {2} überlassen ist." },
			{ "it", "{0}: disattivato, perché la parte {1} è lasciata a {2}." },
			{ "es", "{0}: desactivado, porque depende de {1}, en manos de {2}." },
			{ "esmx", "{0}: desactivado, porque depende de {1}, en manos de {2}." },
			{ "ptbr", "{0}: desativado, pois a parte {1} está a cargo de {2}." },
			{ "pl", "{0}: wyłączone, bo część {1} jest wyłączona na rzecz {2}." },
			{ "ru", "{0}: отключено, так как часть «{1}» уступлена {2}." },
			{ "ja", "{0}: 無効。{1}を{2}に任せているためです" },
			{ "zhhant", "{0}：已停用，因為「{1}」已交由{2}處理。" },
			{ "zhhans", "{0}：已停用，因为“{1}”已交由{2}处理。" },
		};

		constexpr Line PART_VERSION[]{
			{ "en", "{0}: off, as this game version ({1}) is not supported." },
			{ "fr", "{0}\u00A0: désactivé, car cette version du jeu ({1}) n'est pas prise en charge." },
			{ "de", "{0}: aus, da diese Spielversion ({1}) nicht unterstützt wird." },
			{ "it", "{0}: disattivato, perché questa versione del gioco ({1}) non è supportata." },
			{ "es", "{0}: desactivado, porque esta versión del juego ({1}) no es compatible." },
			{ "esmx", "{0}: desactivado, porque esta versión del juego ({1}) no es compatible." },
			{ "ptbr", "{0}: desativado, pois esta versão do jogo ({1}) não é compatível." },
			{ "pl", "{0}: wyłączone, bo ta wersja gry ({1}) nie jest obsługiwana." },
			{ "ru", "{0}: отключено, так как эта версия игры ({1}) не поддерживается." },
			{ "ja", "{0}: 無効。このゲームのバージョン({1})には対応していません" },
			{ "zhhant", "{0}：已停用，不支援此遊戲版本（{1}）。" },
			{ "zhhans", "{0}：已停用，不支持此游戏版本（{1}）。" },
		};

		// An owner NEC cannot name, only inside the owner list of the lines
		// above.
		constexpr Line OWNER_MOD[]{
			{ "en", "another mod" },
			{ "fr", "un autre mod" },
			{ "de", "einem anderen Mod" },
			{ "it", "un altro mod" },
			{ "es", "otro mod" },
			{ "esmx", "otro mod" },
			{ "ptbr", "outro mod" },
			{ "pl", "innego moda" },
			{ "ru", "другому моду" },
			{ "ja", "別のMOD" },
			{ "zhhant", "另一個模組" },
			{ "zhhans", "另一个模组" },
		};

		// The last 2 items of a list, and every pair before them.
		constexpr Line LIST_AND[]{
			{ "en", "{0} and {1}" },
			{ "fr", "{0} et {1}" },
			{ "de", "{0} und {1}" },
			{ "it", "{0} e {1}" },
			{ "es", "{0} y {1}" },
			{ "esmx", "{0} y {1}" },
			{ "ptbr", "{0} e {1}" },
			{ "pl", "{0} i {1}" },
			{ "ru", "{0} и {1}" },
			{ "ja", "{0}と{1}" },
			{ "zhhant", "{0}和{1}" },
			{ "zhhans", "{0}和{1}" },
		};

		constexpr Line LIST_COMMA[]{
			{ "en", "{0}, {1}" },
			{ "fr", "{0}, {1}" },
			{ "de", "{0}, {1}" },
			{ "it", "{0}, {1}" },
			{ "es", "{0}, {1}" },
			{ "esmx", "{0}, {1}" },
			{ "ptbr", "{0}, {1}" },
			{ "pl", "{0}, {1}" },
			{ "ru", "{0}, {1}" },
			{ "ja", "{0}、{1}" },
			{ "zhhant", "{0}、{1}" },
			{ "zhhans", "{0}、{1}" },
		};

		// The settings of a block that do nothing while the parts above stay
		// left to other mods, joined with commas only, as a name can hold
		// "and".
		constexpr Line NO_EFFECT[]{
			{ "en", "No effect for now: {0}." },
			{ "fr", "Sans effet pour l'instant\u00A0: {0}." },
			{ "de", "Vorerst ohne Wirkung: {0}." },
			{ "it", "Per ora senza effetto: {0}." },
			{ "es", "Sin efecto por ahora: {0}." },
			{ "esmx", "Sin efecto por ahora: {0}." },
			{ "ptbr", "Sem efeito por enquanto: {0}." },
			{ "pl", "Na razie bez efektu: {0}." },
			{ "ru", "Пока без эффекта: {0}." },
			{ "ja", "今のところ効果なし: {0}" },
			{ "zhhant", "目前沒有效果：{0}。" },
			{ "zhhans", "暂时不生效：{0}。" },
		};

		// The list at the top of the page: its first line, then 1 line a mod
		// with the parts and pieces it has.
		constexpr Line TOP_INTRO[]{
			{ "en", "Another mod took over the parts listed here, so NEC leaves them alone." },
			{ "fr", "Un autre mod a pris en charge les parties listées ici, donc NEC n'y touche pas." },
			{ "de", "Ein anderer Mod hat die hier genannten Funktionen übernommen, daher lässt NEC sie unberührt." },
			{ "it", "Un altro mod ha preso in carico le parti elencate qui, quindi NEC non le tocca." },
			{ "es", "Otro mod se encarga de las partes de esta lista, así que NEC no las toca." },
			{ "esmx", "Otro mod se encarga de las partes de esta lista, así que NEC no las toca." },
			{ "ptbr", "Outro mod assumiu as partes listadas aqui, então o NEC não mexe nelas." },
			{ "pl", "Inny mod przejął części wymienione tutaj, więc NEC ich nie rusza." },
			{ "ru", "Другой мод взял на себя перечисленные здесь части, поэтому NEC их не трогает." },
			{ "ja", "ここに挙げた機能は別のMODが引き継いだため、NECは手を加えません" },
			{ "zhhant", "其他模組接管了這裡列出的功能，因此NEC不會處理它們。" },
			{ "zhhans", "其他模组接管了这里列出的功能，因此NEC不会处理它们。" },
		};

		constexpr Line TOP_BY[]{
			{ "en", "Left to {0}: {1}." },
			{ "fr", "Laissé à {0}\u00A0: {1}." },
			{ "de", "{0} überlassen: {1}." },
			{ "it", "Parti lasciate a {0}: {1}." },
			{ "es", "En manos de {0}: {1}." },
			{ "esmx", "En manos de {0}: {1}." },
			{ "ptbr", "A cargo de {0}: {1}." },
			{ "pl", "Wyłączone na rzecz {0}: {1}." },
			{ "ru", "Уступлено {0}: {1}." },
			{ "ja", "{0}に任せています: {1}" },
			{ "zhhant", "交由{0}處理：{1}。" },
			{ "zhhans", "交由{0}处理：{1}。" },
		};

		constexpr Line TOP_MOD[]{
			{ "en", "Left to another mod: {0}." },
			{ "fr", "Laissé à un autre mod\u00A0: {0}." },
			{ "de", "Einem anderen Mod überlassen: {0}." },
			{ "it", "Parti lasciate a un altro mod: {0}." },
			{ "es", "En manos de otro mod: {0}." },
			{ "esmx", "En manos de otro mod: {0}." },
			{ "ptbr", "A cargo de outro mod: {0}." },
			{ "pl", "Wyłączone na rzecz innego moda: {0}." },
			{ "ru", "Уступлено другому моду: {0}." },
			{ "ja", "別のMODに任せています: {0}" },
			{ "zhhant", "交由另一個模組處理：{0}。" },
			{ "zhhans", "交由另一个模组处理：{0}。" },
		};

		constexpr Line TOP_VERSION[]{
			{ "en", "Off on this game version ({0}): {1}." },
			{ "fr", "Désactivé sur cette version du jeu ({0})\u00A0: {1}." },
			{ "de", "In dieser Spielversion ({0}) aus: {1}." },
			{ "it", "Parti disattivate in questa versione del gioco ({0}): {1}." },
			{ "es", "Desactivado en esta versión del juego ({0}): {1}." },
			{ "esmx", "Desactivado en esta versión del juego ({0}): {1}." },
			{ "ptbr", "Desativado nesta versão do jogo ({0}): {1}." },
			{ "pl", "Wyłączone w tej wersji gry ({0}): {1}." },
			{ "ru", "Отключено в этой версии игры ({0}): {1}." },
			{ "ja", "このゲームのバージョン({0})では無効: {1}" },
			{ "zhhant", "在此遊戲版本（{0}）停用：{1}。" },
			{ "zhhans", "在此游戏版本（{0}）下停用：{1}。" },
		};

		// In place of the 8th mod's line when more than 8 have parts.
		constexpr Line TOP_MORE[]{
			{ "en", "And {0} more mods. NEC.log names them all." },
			{ "fr", "Et {0} autres mods. NEC.log en donne la liste complète." },
			{ "de", "Und {0} weitere Mods. NEC.log nennt sie alle." },
			{ "it", "E altri {0} mod. NEC.log li elenca tutti." },
			{ "es", "Y {0} mods más. NEC.log los enumera todos." },
			{ "esmx", "Y {0} mods más. NEC.log los enumera todos." },
			{ "ptbr", "E mais {0} mods. O NEC.log lista todos." },
			{ "pl", "Liczba pozostałych modów: {0}. NEC.log wymienia je wszystkie." },
			{ "ru", "И еще модов: {0}. Полный список в NEC.log." },
			{ "ja", "ほかに{0}個のMODがあります。NEC.logに一覧があります" },
			{ "zhhant", "另有{0}個模組，完整清單見NEC.log。" },
			{ "zhhans", "另有{0}个模组，完整清单见NEC.log。" },
		};

		// The help line of every grey row.
		constexpr Line NOTE_HELP[]{
			{ "en", "To get a part back, remove the mod it is left to and restart the game." },
			{ "fr", "Pour récupérer une partie, retirez le mod auquel elle est laissée et relancez le jeu." },
			{ "de", "Um eine Funktion zurückzubekommen, entferne den Mod, dem sie überlassen ist, und starte das Spiel neu." },
			{ "it", "Per riavere una parte, rimuovi il mod a cui è lasciata e riavvia il gioco." },
			{ "es", "Para recuperar una parte, quita el mod que la tiene y reinicia el juego." },
			{ "esmx", "Para recuperar una parte, quita el mod que la tiene y reinicia el juego." },
			{ "ptbr", "Para recuperar uma parte, remova o mod que ficou com ela e reinicie o jogo." },
			{ "pl", "Aby odzyskać część, usuń mod, na rzecz którego ją wyłączono, i uruchom grę ponownie." },
			{ "ru", "Чтобы вернуть часть, удалите мод, которому она уступлена, и перезапустите игру." },
			{ "ja", "機能を有効に戻すには、その機能を任せているMODを外してゲームを再起動してください" },
			{ "zhhant", "若要恢復某項功能，請移除處理它的模組，然後重新啟動遊戲。" },
			{ "zhhans", "如需恢复某项功能，请移除处理它的模组，然后重启游戏。" },
		};

		// The last line of a grey row too tall for the page, in place of the
		// lines it leaves out.
		constexpr Line NOTE_REST[]{
			{ "en", "NEC.log names the rest." },
			{ "fr", "NEC.log donne le reste." },
			{ "de", "NEC.log nennt den Rest." },
			{ "it", "NEC.log elenca il resto." },
			{ "es", "NEC.log enumera el resto." },
			{ "esmx", "NEC.log enumera el resto." },
			{ "ptbr", "O NEC.log lista o resto." },
			{ "pl", "Resztę wymienia NEC.log." },
			{ "ru", "Остальное в NEC.log." },
			{ "ja", "NEC.logに残りの一覧があります" },
			{ "zhhant", "其餘請見NEC.log。" },
			{ "zhhans", "其余请见NEC.log。" },
		};

		// The help line of every grey row while only this game version keeps
		// parts off and no mod has any.
		constexpr Line NOTE_HELP_VERSION[]{
			{ "en", "NEC is made for game version 1.11.240." },
			{ "fr", "NEC est conçu pour la version 1.11.240 du jeu." },
			{ "de", "NEC ist für die Spielversion 1.11.240 gedacht." },
			{ "it", "NEC è pensato per la versione 1.11.240 del gioco." },
			{ "es", "NEC está pensado para la versión 1.11.240 del juego." },
			{ "esmx", "NEC está pensado para la versión 1.11.240 del juego." },
			{ "ptbr", "O NEC foi feito para a versão 1.11.240 do jogo." },
			{ "pl", "NEC jest przeznaczony dla wersji gry 1.11.240." },
			{ "ru", "NEC создан для версии игры 1.11.240." },
			{ "ja", "NECはゲームのバージョン1.11.240向けに作られています" },
			{ "zhhant", "NEC是為遊戲版本1.11.240製作的。" },
			{ "zhhans", "NEC是为游戏版本1.11.240制作的。" },
		};
	}

	std::vector<std::string> PartLines(const CallPatch::Loss& a_loss, Out a_out)
	{
		std::vector<std::string> lines;
		for (const auto& cause : a_loss.causes) {
			const auto title = PartPieces(a_loss.part, cause.pieces, a_out);
			const auto owners = CallPatch::OwnerNames(cause.owners, PickFor(OWNER_MOD, a_out));
			// No mod took it, every owner is this game version.
			if (owners.empty()) {
				lines.push_back(SayFor(a_out, PART_VERSION, title, CallPatch::GameVersion()));
			} else if (cause.part != a_loss.part) {
				lines.push_back(SayFor(a_out, PART_SINCE, title, PartPieces(cause.part, cause.taken, a_out), List(owners, a_out)));
			} else {
				lines.push_back(SayFor(a_out, PART_OFF, title, List(owners, a_out)));
			}
		}
		if (!lines.empty() && !a_loss.works.empty()) {
			lines.back() = Sentences(lines.back(), StillWorks(a_loss.works, a_out), a_out);
		}
		return lines;
	}

	std::string MenuNoEffect(std::span<const std::string> a_names, Out a_out)
	{
		return SayFor(a_out, NO_EFFECT, Commas(a_names, a_out));
	}

	std::string MenuTopIntro()
	{
		return Pick(TOP_INTRO);
	}

	std::string MenuTopOwner(const CallPatch::Owner& a_owner, std::span<const std::string> a_parts)
	{
		const auto list = Commas(a_parts, Out::kPage);
		if (!a_owner.dll.empty()) {
			return Say(TOP_BY, a_owner.dll, list);
		}
		return a_owner.sure ? Say(TOP_MOD, list) : Say(TOP_VERSION, CallPatch::GameVersion(), list);
	}

	std::string MenuTopMore(std::size_t a_count)
	{
		return Say(TOP_MORE, a_count);
	}

	std::string MenuNoteHelp()
	{
		return Pick(NOTE_HELP);
	}

	std::string MenuVersionHelp()
	{
		return Pick(NOTE_HELP_VERSION);
	}

	std::string MenuNoteRest()
	{
		return Pick(NOTE_REST);
	}

	std::string List(std::span<const std::string> a_items, Out a_out)
	{
		if (a_items.empty()) {
			return {};
		}
		std::string list = a_items.front();
		for (std::size_t i = 1; i < a_items.size(); i++) {
			list = SayFor(a_out, i + 1 == a_items.size() ? LIST_AND : LIST_COMMA, list, a_items[i]);
		}
		return list;
	}

	std::string Commas(std::span<const std::string> a_items, Out a_out)
	{
		if (a_items.empty()) {
			return {};
		}
		std::string list = a_items.front();
		for (std::size_t i = 1; i < a_items.size(); i++) {
			list = SayFor(a_out, LIST_COMMA, list, a_items[i]);
		}
		return list;
	}
}
