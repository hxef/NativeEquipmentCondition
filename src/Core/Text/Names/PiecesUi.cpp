#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// The names of the pieces of the UI parts, see Core/Pieces.h. The menus of
// CND on item cards name the same menus' pieces of Fire rate shown in menus,
// Accuracy shown in menus and Range shown in menus. The phrases a setting's
// sentence uses for a piece are in Phrases.cpp.
namespace Text
{
	namespace
	{
		constexpr PieceWords NAMES[]{
			{ Piece::kPerksPipboy, PIECE_PIPBOY },
			{ Piece::kCndContainers, PIECE_CONTAINERS },
			{ Piece::kCndWorkbench, PIECE_WORKBENCHES },
			{ Piece::kCndCooking, PIECE_COOKING },
			{ Piece::kCndPipboy, PIECE_PIPBOY },
			{ Piece::kRateCardsContainers, PIECE_CONTAINERS },
			{ Piece::kRateCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kRateCardsCooking, PIECE_COOKING },
			{ Piece::kRateCardsPipboy, PIECE_PIPBOY },
			{ Piece::kRateBetter, PIECE_BETTER },
			{ Piece::kRateSort, PIECE_SORT_RATE },
			{ Piece::kAccuracyCardsContainers, PIECE_CONTAINERS },
			{ Piece::kAccuracyCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kAccuracyCardsCooking, PIECE_COOKING },
			{ Piece::kAccuracyCardsPipboy, PIECE_PIPBOY },
			{ Piece::kAccuracySort, PIECE_SORT_ACCURACY },
			{ Piece::kRangeCardsContainers, PIECE_CONTAINERS },
			{ Piece::kRangeCardsWorkbench, PIECE_WORKBENCHES },
			{ Piece::kRangeCardsCooking, PIECE_COOKING },
			{ Piece::kRangeCardsPipboy, PIECE_PIPBOY },
			{ Piece::kBenchItemList, PIECE_BENCH_ITEM_LIST },
			{ Piece::kBenchModLists, PIECE_BENCH_MOD_LISTS },
			{ Piece::kStockMerchant, PIECE_STOCK_MERCHANT },
			{ Piece::kStockLinked, PIECE_STOCK_LINKED },
		};
	}

	std::span<const PieceWords> UiPieces()
	{
		return NAMES;
	}
}
