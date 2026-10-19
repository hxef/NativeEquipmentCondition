#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of Worn armor protection, Protection shown in menus
// and Broken gear rules, see Core/Pieces.h.
namespace Text
{
	namespace
	{
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
