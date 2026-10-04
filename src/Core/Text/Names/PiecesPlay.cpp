#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of the Condition and Gameplay parts from Perk repair
// discount text to Damage shown in menus, see Core/Pieces.h. The Pip-Boy of
// Perk repair discount text is in PiecesUi.cpp, the armor pieces are in
// PiecesArmor.cpp.
namespace Text
{
	namespace
	{
		// The perk chart, where a crafting perk says what it takes off a
		// repair.
		constexpr Line PIECE_PERK_CHART[]{
			{ "en", "the perk chart" },
			{ "fr", "le tableau des aptitudes" },
			{ "de", "Skilltabelle" },
			{ "it", "la tabella dei talenti" },
			{ "es", "el cuadro de extras" },
			{ "esmx", "el cuadro de extras" },
			{ "ptbr", "o quadro de vantagens" },
			{ "pl", "wykres profitów" },
			{ "ru", "таблица способностей" },
			{ "ja", "Perkチャート" },
			{ "zhhant", "輔助能力表" },
			{ "zhhans", "辅助能力表" },
		};

		// The game never names the plain damage type on screen, so these say
		// physical, which fits a blade and a bullet alike.
		constexpr Line PIECE_MELEE_PHYSICAL[]{
			{ "en", "physical melee damage" },
			{ "fr", "dégâts physiques au corps à corps" },
			{ "de", "Normalschaden im Nahkampf" },
			{ "it", "danni fisici in mischia" },
			{ "es", "daño físico cuerpo a cuerpo" },
			{ "esmx", "daño físico cuerpo a cuerpo" },
			{ "ptbr", "dano físico corpo a corpo" },
			{ "pl", "obrażenia fizyczne w walce wręcz" },
			{ "ru", "физический урон в ближнем бою" },
			{ "ja", "近接武器の物理ダメージ" },
			{ "zhhant", "近戰物理傷害" },
			{ "zhhans", "近战物理伤害" },
		};

		constexpr Line PIECE_MELEE_ENERGY[]{
			{ "en", "energy melee damage" },
			{ "fr", "dégâts d'énergie au corps à corps" },
			{ "de", "Energieschaden im Nahkampf" },
			{ "it", "danni da energia in mischia" },
			{ "es", "daño de energía cuerpo a cuerpo" },
			{ "esmx", "daño de energía cuerpo a cuerpo" },
			{ "ptbr", "dano de energia corpo a corpo" },
			{ "pl", "obrażenia energetyczne w walce wręcz" },
			{ "ru", "энергетический урон в ближнем бою" },
			{ "ja", "近接武器のエネルギーダメージ" },
			{ "zhhant", "近戰能量傷害" },
			{ "zhhans", "近战能量伤害" },
		};

		constexpr Line PIECE_GUN_PHYSICAL[]{
			{ "en", "physical gun damage" },
			{ "fr", "dégâts physiques des armes à feu" },
			{ "de", "Normalschaden mit Schusswaffen" },
			{ "it", "danni fisici delle armi da fuoco" },
			{ "es", "daño físico de armas de fuego" },
			{ "esmx", "daño físico de armas de fuego" },
			{ "ptbr", "dano físico de armas de fogo" },
			{ "pl", "obrażenia fizyczne od broni palnej" },
			{ "ru", "физический урон стрелкового оружия" },
			{ "ja", "銃の物理ダメージ" },
			{ "zhhant", "槍枝物理傷害" },
			{ "zhhans", "枪支物理伤害" },
		};

		constexpr Line PIECE_GUN_ENERGY[]{
			{ "en", "energy gun damage" },
			{ "fr", "dégâts d'énergie des armes à feu" },
			{ "de", "Energieschaden mit Schusswaffen" },
			{ "it", "danni da energia delle armi da fuoco" },
			{ "es", "daño de energía de armas de fuego" },
			{ "esmx", "daño de energía de armas de fuego" },
			{ "ptbr", "dano de energia de armas de fogo" },
			{ "pl", "obrażenia energetyczne od broni palnej" },
			{ "ru", "энергетический урон стрелкового оружия" },
			{ "ja", "銃のエネルギーダメージ" },
			{ "zhhant", "槍枝能量傷害" },
			{ "zhhans", "枪支能量伤害" },
		};

		// The blast of a weapon like a Fat Man in play: what its physical part
		// takes from people and creatures, its energy, radiation, cryo or fire
		// part, and what it takes from cars and other objects that can be
		// destroyed.
		constexpr Line PIECE_BLAST_PHYSICAL[]{
			{ "en", "physical explosion damage" },
			{ "fr", "dégâts physiques d'explosion" },
			{ "de", "Normalschaden durch Explosionen" },
			{ "it", "danni fisici da esplosione" },
			{ "es", "daño físico de explosión" },
			{ "esmx", "daño físico de explosión" },
			{ "ptbr", "dano físico de explosão" },
			{ "pl", "obrażenia fizyczne od wybuchów" },
			{ "ru", "физический урон от взрывов" },
			{ "ja", "爆発の物理ダメージ" },
			{ "zhhant", "爆炸物理傷害" },
			{ "zhhans", "爆炸物理伤害" },
		};

		constexpr Line PIECE_BLAST_ENERGY[]{
			{ "en", "energy explosion damage" },
			{ "fr", "dégâts d'énergie d'explosion" },
			{ "de", "Energieschaden durch Explosionen" },
			{ "it", "danni da energia delle esplosioni" },
			{ "es", "daño de energía de explosión" },
			{ "esmx", "daño de energía de explosión" },
			{ "ptbr", "dano de energia de explosão" },
			{ "pl", "obrażenia energetyczne od wybuchów" },
			{ "ru", "энергетический урон от взрывов" },
			{ "ja", "爆発のエネルギーダメージ" },
			{ "zhhant", "爆炸能量傷害" },
			{ "zhhans", "爆炸能量伤害" },
		};

		constexpr Line PIECE_BLAST_OBJECTS[]{
			{ "en", "explosion damage to objects" },
			{ "fr", "dégâts d'explosion aux objets" },
			{ "de", "Explosionsschaden an Objekten" },
			{ "it", "danni da esplosione agli oggetti" },
			{ "es", "daño de explosión a objetos" },
			{ "esmx", "daño de explosión a objetos" },
			{ "ptbr", "dano de explosão a objetos" },
			{ "pl", "obrażenia od wybuchów dla obiektów" },
			{ "ru", "урон от взрывов по объектам" },
			{ "ja", "オブジェクトへの爆発ダメージ" },
			{ "zhhant", "對物件的爆炸傷害" },
			{ "zhhans", "对物体的爆炸伤害" },
		};

		// The blast of a weapon like a Fat Man on its item card.
		constexpr Line PIECE_BLAST[]{
			{ "en", "explosion damage" },
			{ "fr", "dégâts d'explosion" },
			{ "de", "Explosionsschaden" },
			{ "it", "danni da esplosione" },
			{ "es", "daño de explosión" },
			{ "esmx", "daño de explosión" },
			{ "ptbr", "dano de explosão" },
			{ "pl", "obrażenia od wybuchów" },
			{ "ru", "урон от взрывов" },
			{ "ja", "爆発ダメージ" },
			{ "zhhant", "爆炸傷害" },
			{ "zhhans", "爆炸伤害" },
		};

		// What a hit puts on its target, like a Radium Rifle's radiation.
		constexpr Line PIECE_HIT_EFFECTS[]{
			{ "en", "weapon effects on hit" },
			{ "fr", "effets d'arme au toucher" },
			{ "de", "Waffeneffekte beim Treffer" },
			{ "it", "effetti delle armi al colpo" },
			{ "es", "efectos de arma al impactar" },
			{ "esmx", "efectos de arma al impactar" },
			{ "ptbr", "efeitos de arma ao acertar" },
			{ "pl", "efekty broni przy trafieniu" },
			{ "ru", "эффекты оружия при попадании" },
			{ "ja", "命中時の武器効果" },
			{ "zhhant", "武器命中效果" },
			{ "zhhans", "武器命中效果" },
		};

		constexpr Line PIECE_BLAST_EFFECTS[]{
			{ "en", "effects of explosions" },
			{ "fr", "effets des explosions" },
			{ "de", "Effekte von Explosionen" },
			{ "it", "effetti delle esplosioni" },
			{ "es", "efectos de explosiones" },
			{ "esmx", "efectos de explosiones" },
			{ "ptbr", "efeitos de explosões" },
			{ "pl", "efekty wybuchów" },
			{ "ru", "эффекты взрывов" },
			{ "ja", "爆発の効果" },
			{ "zhhant", "爆炸效果" },
			{ "zhhans", "爆炸效果" },
		};

		// The damage numbers on an item card.
		constexpr Line PIECE_CARD_PHYSICAL[]{
			{ "en", "physical damage" },
			{ "fr", "dégâts physiques" },
			{ "de", "Normalschaden" },
			{ "it", "danni fisici" },
			{ "es", "daño físico" },
			{ "esmx", "daño físico" },
			{ "ptbr", "dano físico" },
			{ "pl", "obrażenia fizyczne" },
			{ "ru", "физический урон" },
			{ "ja", "物理ダメージ" },
			{ "zhhant", "物理傷害" },
			{ "zhhans", "物理伤害" },
		};

		constexpr Line PIECE_CARD_ENERGY[]{
			{ "en", "energy damage" },
			{ "fr", "dégâts d'énergie" },
			{ "de", "Energieschaden" },
			{ "it", "danni da energia" },
			{ "es", "daño de energía" },
			{ "esmx", "daño de energía" },
			{ "ptbr", "dano de energia" },
			{ "pl", "obrażenia energetyczne" },
			{ "ru", "энергетический урон" },
			{ "ja", "エネルギーダメージ" },
			{ "zhhant", "能量傷害" },
			{ "zhhans", "能量伤害" },
		};

		// What a weapon's mods add to it, like a legendary's effect.
		constexpr Line PIECE_CARD_MOD_EFFECTS[]{
			{ "en", "damage from weapon mod effects" },
			{ "fr", "dégâts des effets des modules d'arme" },
			{ "de", "Schaden durch Effekte von Waffen-Mods" },
			{ "it", "danni degli effetti delle modifiche dell'arma" },
			{ "es", "daño de los efectos de los módulos del arma" },
			{ "esmx", "daño de los efectos de los módulos del arma" },
			{ "ptbr", "dano dos efeitos dos mods da arma" },
			{ "pl", "obrażenia od efektów modyfikacji broni" },
			{ "ru", "урон от эффектов модификаций оружия" },
			{ "ja", "武器モジュールの効果によるダメージ" },
			{ "zhhant", "武器改造配件效果的傷害" },
			{ "zhhans", "武器改造配件效果的伤害" },
		};

		// What the weapon itself carries, like a Radium Rifle's radiation, and
		// what the explosion of its shot carries.
		constexpr Line PIECE_CARD_OWN_EFFECTS[]{
			{ "en", "damage from the weapon's own and explosion effects" },
			{ "fr", "dégâts des effets propres à l'arme et de son explosion" },
			{ "de", "Schaden durch die Eigen- und Explosionseffekte der Waffe" },
			{ "it", "danni degli effetti propri dell'arma e della sua esplosione" },
			{ "es", "daño de los efectos propios del arma y de su explosión" },
			{ "esmx", "daño de los efectos propios del arma y de su explosión" },
			{ "ptbr", "dano dos efeitos próprios da arma e de sua explosão" },
			{ "pl", "obrażenia od własnych efektów broni i efektów wybuchów" },
			{ "ru", "урон от собственных эффектов оружия и эффектов взрывов" },
			{ "ja", "武器自体と爆発の効果によるダメージ" },
			{ "zhhant", "武器自身效果和爆炸效果的傷害" },
			{ "zhhans", "武器自身效果和爆炸效果的伤害" },
		};

		constexpr PieceWords NAMES[]{
			{ Piece::kPerksChart, PIECE_PERK_CHART },
			{ Piece::kDmgMeleePhysical, PIECE_MELEE_PHYSICAL },
			{ Piece::kDmgMeleeEnergy, PIECE_MELEE_ENERGY },
			{ Piece::kDmgGunPhysical, PIECE_GUN_PHYSICAL },
			{ Piece::kDmgGunEnergy, PIECE_GUN_ENERGY },
			{ Piece::kDmgBlastPhysical, PIECE_BLAST_PHYSICAL },
			{ Piece::kDmgBlastEnergy, PIECE_BLAST_ENERGY },
			{ Piece::kDmgBlastObjects, PIECE_BLAST_OBJECTS },
			{ Piece::kDmgHitEffects, PIECE_HIT_EFFECTS },
			{ Piece::kDmgBlastEffects, PIECE_BLAST_EFFECTS },
			{ Piece::kCardDmgPhysical, PIECE_CARD_PHYSICAL },
			{ Piece::kCardDmgEnergy, PIECE_CARD_ENERGY },
			{ Piece::kCardDmgModEffects, PIECE_CARD_MOD_EFFECTS },
			{ Piece::kCardDmgOwnEffects, PIECE_CARD_OWN_EFFECTS },
			{ Piece::kCardDmgBlast, PIECE_BLAST },
		};
	}

	std::span<const PieceWords> PlayPieces()
	{
		return NAMES;
	}
}
