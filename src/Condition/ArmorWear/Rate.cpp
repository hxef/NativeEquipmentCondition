#include "Condition/ArmorWear/Rate.h"

#include "Condition/Materials/Materials.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

namespace ArmorWear
{
	namespace
	{
		// Health a piece loses from one hit as hard as the ordinary weapon's,
		// before fArmorWearRateMult. 100 such hits take it from new to broken,
		// a few fights for a chest piece and much longer for a leg piece. That
		// is far fewer than a gun's 1100 shots, since a fighter fires far more
		// shots than they take hits. A raider shot 5 times in the chest drops a
		// chest piece a few percent worse than one shot in the head, a
		// difference worth choosing loot by.
		constexpr float RATE_AT_REFERENCE = 0.01F;
	}

	float BlowsToBreak()
	{
		return 1.0F / (RATE_AT_REFERENCE * Settings::fArmorWearRateMult.GetValue());
	}

	float Rate(const RE::TESObjectARMO& a_armor, const RE::ExtraDataList* a_extra, float a_damage, bool a_theirs)
	{
		// What the piece is built from, see Materials.h, and what an ordinary
		// weapon hits for, see WeaponWear.h. Both are read from the game as it
		// is loaded.
		const auto quality = Materials::Quality(a_armor, a_extra);
		const auto reference = WeaponWear::ReferenceDamage();

		// Harder hits wear faster, better made pieces wear slower. Both are
		// measured against the ordinary weapon and the ordinary piece, so how
		// much damage a piece can take in its life depends on what it is built
		// from, and how hard each hit is decides how fast that runs out. The
		// ordinary piece is a plain one, see Materials::ReferenceQuality: a
		// plain steel helmet wears a little faster, plain leather slower, and
		// every upgrade slows either one, since adhesive and fiberglass are
		// worth more than steel or leather.
		// fArmorWearRateMult stops at 0, since below it a piece would gain
		// condition from every blow.
		const auto mult = Settings::fArmorWearRateMult.GetValue();
		const auto wear = RATE_AT_REFERENCE * (mult > 0.0F ? mult : 0.0F) *
		                  (a_damage / reference) * (Materials::ReferenceQuality(Condition::Kind::kArmor) / quality);

		TraceLog::For(a_theirs).Line("rate", "a blow of {:.0f} on armor at quality {:.1f} costs {:.6f}", a_damage, quality, wear);
		return wear;
	}
}
