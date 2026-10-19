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
