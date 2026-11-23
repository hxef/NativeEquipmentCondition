#pragma once

// The settings under [HUD]: where the CND bars sit and when they show.
// Included by Settings.h only, after Tuning.h, in NEC.ini's order. Private to
// this folder.
namespace Settings
{
	// HUD. Where the CND bars sit, how big they are drawn and when the one by
	// the ammo counter hides. Read every frame, so a change shows at once. The
	// 6 place numbers move a bar from its own place in units of the HUD, which
	// is 1280 across and 720 down on any screen, and 0 and 0 is its own place.
	// The 6 size numbers count from 0.5 to 3 times a bar's own length or
	// thickness, see HudParts::Readout::Size.

	// How far right of its own spot by the ammo counter the Weapon CND bar
	// sits, below 0 to the left. Off its spot, the divider shows again.
	// HudCondition.h says how it stays on screen and what the MCM page shows.
	inline Live<float> fHudBarX{ "HUD", "fHudBarX", 0.0F };

	// How far up from its own spot the Weapon CND bar sits, below 0 down.
	inline Live<float> fHudBarY{ "HUD", "fHudBarY", 0.0F };

	// How long and how thick the Weapon CND bar is drawn.
	inline Live<float> fHudBarLength{ "HUD", "fHudBarLength", 1.0F };
	inline Live<float> fHudBarThickness{ "HUD", "fHudBarThickness", 1.0F };

	// Whether the Weapon CND bar hides with the ammo counter while the counter
	// has rounds to count, see HudCondition.h.
	inline Live<bool> bHudBarHidesWithCounter{ "HUD", "bHudBarHidesWithCounter", false };

	// Whether the Weapon CND bar stands alone at the place the MCM page shows
	// while a HUD mod hides the ammo counter, see HudCondition.h. When off, it
	// hides with the counter.
	inline Live<bool> bHudBarStandsAlone{ "HUD", "bHudBarStandsAlone", false };

	// Whether the CND word shows beside the Weapon CND bar. When off, only the
	// bar shows, at the same place.
	inline Live<bool> bHudBarShowsWord{ "HUD", "bHudBarShowsWord", true };

	// Mirrors the Weapon CND bar, so it fills from the left and the word sits
	// before it, see HudCondition.h.
	inline Live<bool> bHudBarFlipped{ "HUD", "bHudBarFlipped", false };

	// Whether the Weapon CND percent reads in 1 line, CND 87.4%, at the ammo
	// divider too, where it reads in 2 lines when off. Anywhere else it is 1
	// line either way, see Lines in HudCondition/Table.h.
	inline Live<bool> bHudBarPercentOneLine{ "HUD", "bHudBarPercentOneLine", false };

	// How far right of the dash's ammo digits the Power armor CND bar sits,
	// below 0 to the left. It still sways with the dash, see
	// PowerArmorCondition.h.
	inline Live<float> fPowerArmorBarX{ "HUD", "fPowerArmorBarX", 0.0F };

	// How far up from the dash's ammo digits it sits, below 0 down.
	inline Live<float> fPowerArmorBarY{ "HUD", "fPowerArmorBarY", 0.0F };

	// How long and how thick the Power armor CND bar is drawn.
	inline Live<float> fPowerArmorBarLength{ "HUD", "fPowerArmorBarLength", 1.0F };
	inline Live<float> fPowerArmorBarThickness{ "HUD", "fPowerArmorBarThickness", 1.0F };

	// Whether the CND word shows beside the Power armor CND bar. When off,
	// only the bar shows, at the same place.
	inline Live<bool> bPowerArmorBarShowsWord{ "HUD", "bPowerArmorBarShowsWord", true };

	// Mirrors the Power armor CND bar, so it fills from the left and keeps the
	// left end of the dash's ammo box, see PowerArmorCondition.h.
	inline Live<bool> bPowerArmorBarFlipped{ "HUD", "bPowerArmorBarFlipped", false };

	// How the Pickup CND shows under the name in the prompt of a weapon,
	// armor or food outside a container, see Cnd and PickupBar.h.
	inline CndStep iPickupCnd{ "HUD", "iPickupCnd", Cnd::kBar };

	// How far right of its own place under the name the Pickup CND bar sits,
	// below 0 to the left.
	inline Live<float> fPickupBarX{ "HUD", "fPickupBarX", 0.0F };

	// How far up from its own place it sits, below 0 down.
	inline Live<float> fPickupBarY{ "HUD", "fPickupBarY", 0.0F };

	// How long and how thick the Pickup CND bar is drawn.
	inline Live<float> fPickupBarLength{ "HUD", "fPickupBarLength", 1.0F };
	inline Live<float> fPickupBarThickness{ "HUD", "fPickupBarThickness", 1.0F };

	// Whether the CND word shows beside the Pickup CND bar. When off, only
	// the bar shows, at the same place.
	inline Live<bool> bPickupBarShowsWord{ "HUD", "bPickupBarShowsWord", true };

	// Mirrors the Pickup CND bar, so it fills from the left and the word sits
	// before it. It stays centred under the name.
	inline Live<bool> bPickupBarFlipped{ "HUD", "bPickupBarFlipped", false };

	// Whether the Pickup CND bar draws the penalty mark the other bars draw.
	// Off as it ships, since the mark helps most on the weapon in hand.
	inline Live<bool> bPickupPenaltyMark{ "HUD", "bPickupPenaltyMark", false };

	// Mirrors the quick container's CND meters, so each fills from the right.
	inline Live<bool> bQuickContainerBarFlipped{ "HUD", "bQuickContainerBarFlipped", false };

	// Puts each quick container row's CND at its right edge, see QuickContainer.h.
	inline Live<bool> bQuickContainerCndAtRight{ "HUD", "bQuickContainerCndAtRight", true };

	// How thick the CND meter on item cards is drawn, 1 for 65% of its row's
	// text height.
	inline Live<float> fItemCardBarThickness{ "HUD", "fItemCardBarThickness", 1.0F };

	// Mirrors the bar of an item card's CND row, so it fills from the right.
	inline Live<bool> bItemCardBarFlipped{ "HUD", "bItemCardBarFlipped", false };

	// How the CND over each body part of the Pip-Boy apparel figure shows,
	// see Cnd and PaperDoll.h.
	inline CndStep iApparelFigureCnd{ "HUD", "iApparelFigureCnd", Cnd::kBar };

	// How thick the CND bars on the Pip-Boy apparel figure are drawn.
	inline Live<float> fPipboyBarThickness{ "HUD", "fPipboyBarThickness", 1.0F };

	// Mirrors the CND bars on the Pip-Boy apparel figure, so each fills from
	// the right.
	inline Live<bool> bPipboyBarFlipped{ "HUD", "bPipboyBarFlipped", false };

	// How wide the mark where penalties start is on CND bars, 1 for its normal
	// width.
	inline Live<float> fPenaltyMarkWidth{ "HUD", "fPenaltyMarkWidth", 1.0F };

	// Draws that mark as a plain line instead of 2 notches.
	inline Live<bool> bPenaltyMarkAsLine{ "HUD", "bPenaltyMarkAsLine", false };

	// How far right of its own place on the barter screen the trader repair
	// sign sits, below 0 to the left. At 0 and 0 NEC places it itself, see
	// VendorRepair/Sign.h.
	inline Live<float> fTraderSignX{ "HUD", "fTraderSignX", 0.0F };

	// How far up from its own place it sits, below 0 down.
	inline Live<float> fTraderSignY{ "HUD", "fTraderSignY", 0.0F };
}
