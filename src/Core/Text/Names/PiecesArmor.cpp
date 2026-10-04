#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of Worn armor protection, Protection shown in menus
// and Broken gear rules, see Core/Pieces.h.
namespace Text
{
	namespace
	{
		// Worn armor in play, by the game's own resistance names.
		constexpr Line PIECE_ARMOR_DR[]{
			{ "en", "damage resistance" },
			{ "fr", "résistance aux dégâts" },
			{ "de", "Schadensresistenz" },
			{ "it", "resistenza ai danni" },
			{ "es", "resistencia al daño" },
			{ "esmx", "resistencia al daño" },
			{ "ptbr", "resistência a danos" },
			{ "pl", "odporność na obrażenia" },
			{ "ru", "сопротивление урону" },
			{ "ja", "ダメージ耐性" },
			{ "zhhant", "傷害抗性" },
			{ "zhhans", "伤害抗性" },
		};

		constexpr Line PIECE_ARMOR_ENERGY_RAD[]{
			{ "en", "energy and radiation resistance" },
			{ "fr", "résistance à l'énergie et aux radiations" },
			{ "de", "Energie- und Strahlungsresistenz" },
			{ "it", "resistenza all'energia e alle radiazioni" },
			{ "es", "resistencia a la energía y a la radiación" },
			{ "esmx", "resistencia a la energía y a la radiación" },
			{ "ptbr", "resistência a energia e a radiação" },
			{ "pl", "odporność na energię i promieniowanie" },
			{ "ru", "сопротивление энергии и радиации" },
			{ "ja", "エネルギー耐性と放射能耐性" },
			{ "zhhant", "能量與輻射抗性" },
			{ "zhhans", "能量与辐射抗性" },
		};

		// The best item mark a list in a container, at a trader or at a
		// workbench puts on the piece of each kind it rates highest.
		constexpr Line PIECE_ARMOR_BEST_MARK[]{
			{ "en", "best item marks in menus" },
			{ "fr", "marques du meilleur objet dans les menus" },
			{ "de", "Bestmarkierungen in Menüs" },
			{ "it", "segni di oggetto migliore nei menu" },
			{ "es", "marcas de mejor objeto en los menús" },
			{ "esmx", "marcas de mejor objeto en los menús" },
			{ "ptbr", "marcas de item melhor nos menus" },
			{ "pl", "najlepsze oznaczenia w menu" },
			{ "ru", "лучшие метки в меню" },
			{ "ja", "メニューの最良品マーク" },
			{ "zhhant", "選單中的最佳標記" },
			{ "zhhans", "菜单中的最佳标记" },
		};

		// The rating an NPC weighs a piece by when the game asks what it is
		// worth to them, and in a fight. Nothing on screen shows it.
		constexpr Line PIECE_ARMOR_NPC_RANKING[]{
			{ "en", "how NPCs rank armor" },
			{ "fr", "comment les PNJ classent l'armure" },
			{ "de", "wie NPCs Rüstungen bewerten" },
			{ "it", "come i PNG classificano l'armatura" },
			{ "es", "cómo otros personajes valoran la armadura" },
			{ "esmx", "cómo otros personajes valoran la armadura" },
			{ "ptbr", "como os NPCs classificam a armadura" },
			{ "pl", "jak NPC oceniają pancerz" },
			{ "ru", "как NPC оценивают броню" },
			{ "ja", "NPCのアーマー評価" },
			{ "zhhant", "其他角色評估裝甲的方式" },
			{ "zhhans", "其他角色评估装甲的方式" },
		};

		// The resistances an item card shows.
		constexpr Line PIECE_CARD_ARMOR_MENUS[]{
			{ "en", "item cards outside the Pip-Boy" },
			{ "fr", "fiches hors du Pip-Boy" },
			{ "de", "Gegenstandsinfos außerhalb des Pip-Boys" },
			{ "it", "schede fuori dal Pip-Boy" },
			{ "es", "fichas fuera del Pip-Boy" },
			{ "esmx", "fichas fuera del Pip-Boy" },
			{ "ptbr", "fichas fora do Pip-Boy" },
			{ "pl", "karty przedmiotów poza Pip-Boyem" },
			{ "ru", "карточки предметов вне Пип-боя" },
			{ "ja", "Pip-Boy以外のアイテム情報" },
			{ "zhhant", "嗶嗶小子以外的物品資訊" },
			{ "zhhans", "哔哔小子以外的物品信息" },
		};

		constexpr Line PIECE_CARD_ARMOR_COMPARE[]{
			{ "en", "comparison with equipped armor" },
			{ "fr", "comparaison avec l'armure équipée" },
			{ "de", "Vergleich mit angelegter Rüstung" },
			{ "it", "confronto con l'armatura equipaggiata" },
			{ "es", "comparación con la armadura equipada" },
			{ "esmx", "comparación con la armadura equipada" },
			{ "ptbr", "comparação com a armadura equipada" },
			{ "pl", "porównanie z założonym pancerzem" },
			{ "ru", "сравнение с надетой броней" },
			{ "ja", "装備中のアーマーとの比較" },
			{ "zhhant", "與已裝備裝甲的比較" },
			{ "zhhans", "与已装备装甲的比较" },
		};

		constexpr Line PIECE_CARD_ARMOR_PIPBOY[]{
			{ "en", "Pip-Boy item cards" },
			{ "fr", "fiches du Pip-Boy" },
			{ "de", "Pip-Boy-Gegenstandsinfos" },
			{ "it", "schede del Pip-Boy" },
			{ "es", "fichas del Pip-Boy" },
			{ "esmx", "fichas del Pip-Boy" },
			{ "ptbr", "fichas do Pip-Boy" },
			{ "pl", "karty przedmiotów w Pip-Boyu" },
			{ "ru", "карточки предметов в Пип-бое" },
			{ "ja", "Pip-Boyのアイテム情報" },
			{ "zhhant", "嗶嗶小子的物品資訊" },
			{ "zhhans", "哔哔小子的物品信息" },
		};

		// The resistance numbers by the figure on the Pip-Boy's apparel tab.
		constexpr Line PIECE_CARD_ARMOR_DOLL[]{
			{ "en", "the Pip-Boy apparel figure" },
			{ "fr", "la silhouette des vêtements du Pip-Boy" },
			{ "de", "Kleidungsfigur im Pip-Boy" },
			{ "it", "la figura abbigliamento del Pip-Boy" },
			{ "es", "la figura de ropa del Pip-Boy" },
			{ "esmx", "la figura de ropa del Pip-Boy" },
			{ "ptbr", "a figura de roupas do Pip-Boy" },
			{ "pl", "sylwetka ubioru w Pip-Boyu" },
			{ "ru", "фигура одежды в Пип-бое" },
			{ "ja", "Pip-Boyの服装の人型図" },
			{ "zhhant", "嗶嗶小子的服裝人形圖" },
			{ "zhhans", "哔哔小子的服装人形图" },
		};

		// A broken piece comes off when asked, and goes back on only once
		// repaired.
		constexpr Line PIECE_TAKE_OFF[]{
			{ "en", "taking broken gear off" },
			{ "fr", "retirer l'équipement brisé" },
			{ "de", "Ablegen kaputter Ausrüstung" },
			{ "it", "togliere l'equipaggiamento rotto" },
			{ "es", "quitarse el equipo roto" },
			{ "esmx", "quitarse el equipo roto" },
			{ "ptbr", "tirar equipamento quebrado" },
			{ "pl", "zdejmowanie zepsutego sprzętu" },
			{ "ru", "снятие сломанного снаряжения" },
			{ "ja", "壊れた装備を脱ぐこと" },
			{ "zhhant", "脫下損壞的裝備" },
			{ "zhhans", "脱下损坏的装备" },
		};

		constexpr Line PIECE_PUT_BACK[]{
			{ "en", "putting broken gear back on" },
			{ "fr", "remettre l'équipement brisé" },
			{ "de", "Wiederanlegen kaputter Ausrüstung" },
			{ "it", "rimettere l'equipaggiamento rotto" },
			{ "es", "volver a ponerse el equipo roto" },
			{ "esmx", "volver a ponerse el equipo roto" },
			{ "ptbr", "recolocar equipamento quebrado" },
			{ "pl", "zakładanie zepsutego sprzętu z powrotem" },
			{ "ru", "надевание сломанного снаряжения обратно" },
			{ "ja", "壊れた装備を再び装備すること" },
			{ "zhhant", "重新穿上損壞的裝備" },
			{ "zhhans", "重新穿上损坏的装备" },
		};

		constexpr PieceWords NAMES[]{
			{ Piece::kArmorDr, PIECE_ARMOR_DR },
			{ Piece::kArmorEnergyRad, PIECE_ARMOR_ENERGY_RAD },
			{ Piece::kArmorBestMark, PIECE_ARMOR_BEST_MARK },
			{ Piece::kArmorNpcRanking, PIECE_ARMOR_NPC_RANKING },
			{ Piece::kCardArmorMenus, PIECE_CARD_ARMOR_MENUS },
			{ Piece::kCardArmorCompare, PIECE_CARD_ARMOR_COMPARE },
			{ Piece::kCardArmorPipboy, PIECE_CARD_ARMOR_PIPBOY },
			{ Piece::kCardArmorDoll, PIECE_CARD_ARMOR_DOLL },
			{ Piece::kBrokenTakeOff, PIECE_TAKE_OFF },
			{ Piece::kBrokenPutBack, PIECE_PUT_BACK },
		};
	}

	std::span<const PieceWords> ArmorPieces()
	{
		return NAMES;
	}
}
