#include "Condition/Crew.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>

namespace Crew
{
	namespace
	{
		// -------------------------------------------------------------------
		// Who they are
		// -------------------------------------------------------------------

		constexpr auto COMPANION = "a companion of yours";
		constexpr auto SETTLER = "a settler of yours";

		float Kept(const Settings::Live<float>& a_slider)
		{
			return std::clamp(a_slider.GetValue(), 0.0F, 1.0F);
		}

		// Whether a_actor belongs to a settlement the player owns, by the
		// actor value WorkshopPlayerOwned. The game keeps no form for it in
		// any plugin, it is built in. WorkshopNPCScript.UpdatePlayerOwnership
		// writes it when WorkshopParent assigns an actor to a workshop, sets
		// it to 0 on removal or death, and keeps it on a provisioner along
		// its route. So a settler a mod ties to a workshop without
		// WorkshopParent never gets it and wears at the NPC rates.
		bool Settler(const RE::Actor& a_actor)
		{
			const auto* values = RE::ActorValue::GetSingleton();
			return values && values->workshopPlayerOwned && a_actor.GetActorValue(*values->workshopPlayerOwned) > 0.0F;
		}
	}

	// -------------------------------------------------------------------
	// The share
	// -------------------------------------------------------------------

	Share Of(const RE::Actor& a_actor, Gear a_gear)
	{
		const auto armor = a_gear == Gear::kArmor;
		if (a_actor.IsPlayerRef()) {
			return { 1.0F, nullptr };
		}
		// The flag FollowersScript sets for a companion and Dogmeat, kept
		// while one waits.
		if (a_actor.IsPlayerTeammate()) {
			return { Kept(armor ? Settings::fCompanionArmorWearRateMult : Settings::fCompanionWearRateMult), COMPANION };
		}
		if (Settler(a_actor)) {
			return { Kept(armor ? Settings::fSettlerArmorWearRateMult : Settings::fSettlerWearRateMult), SETTLER };
		}
		return { 1.0F, nullptr };
	}

	bool Wears(const Share& a_share)
	{
		if (!a_share.who) {
			return true;
		}
		if (a_share.mult <= 0.0F) {
			TraceLog::Npc::Line("wear", "{:s} at x{:.2f}, so no wear", a_share.who, a_share.mult);
			return false;
		}
		TraceLog::Npc::Line("wear", "{:s}, so x{:.2f} on top of the NPC rate", a_share.who, a_share.mult);
		return true;
	}
}
