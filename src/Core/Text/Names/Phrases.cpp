#include "Core/Text/Lines.h"

#include "Core/Pieces.h"

#include <span>

// What a setting's sentence says in place of a piece's name, after "no
// effect on" and after "Still changes" or "Still works for", see
// SettingLines in Menu/MenuPieces.cpp.
namespace Text
{
	namespace
	{
		constexpr PieceWords PHRASES[]{
			{ Piece::kGunWear, PHRASE_GUN_WEAR },
			{ Piece::kFireRate, PHRASE_FIRE_RATE },
			{ Piece::kFireRateNpcGuns, PHRASE_NPC_GUNS },
			{ Piece::kFireSound, PHRASE_FIRE_SOUND },
			{ Piece::kCritMeter, PHRASE_CRIT_METER },
			{ Piece::kNpcCrits, PHRASE_NPC_CRITS },
			{ Piece::kAccuracy, PHRASE_ACCURACY },
			{ Piece::kCrosshair, PHRASE_CROSSHAIR },
			{ Piece::kVatsAccuracy, PHRASE_VATS_ACCURACY },
			{ Piece::kRange, PHRASE_RANGE },
			{ Piece::kJamShot, PHRASE_JAM_SHOT },
			{ Piece::kJamReload, PHRASE_JAM_RELOAD },
			{ Piece::kTraderRepairs, PHRASE_TRADER_REPAIRS },
			{ Piece::kMeleeWear, PHRASE_MELEE_WEAR },
			{ Piece::kMotorWear, PHRASE_MOTOR_WEAR },
			{ Piece::kWornLoot, PHRASE_WORN_LOOT },
			{ Piece::kWorldLoot, PHRASE_WORLD_LOOT },
			{ Piece::kModlessLoot, PHRASE_MODLESS_LOOT },
		};
	}

	std::span<const PieceWords> PiecePhrases()
	{
		return PHRASES;
	}
}
