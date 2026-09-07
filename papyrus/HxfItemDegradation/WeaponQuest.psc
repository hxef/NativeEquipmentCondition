Scriptname HxfItemDegradation:WeaponQuest extends Quest

import HxfItemDegradation:HxfItemDegradation

Group Player
	Actor Property kPlayerRef Auto Const
EndGroup

Group Tweaks
	GlobalVariable Property kDegradationMultGlb Auto Const
EndGroup

Group HUDFrameworkProps
	HUDFramework Property hud Auto Hidden
	String Property WeaponConditionWidget = "HxfItemDegradation/hxfID_WeaponConditionWidget.swf" AutoReadOnly
	Int Property WeaponConditionWidget_SetMaxHealth_Cmd = 1 AutoReadOnly
	Int Property WeaponConditionWidget_SetHealth_Cmd = 2 AutoReadOnly
	Int Property WeaponConditionWidget_SetVisibility_Cmd = 3 AutoReadOnly
	Int Property WeaponConditionWidget_SetProps_Cmd = 4 AutoReadOnly
	{param1: Pa/melee/gun, param2: health, param3: visible_flag}
	Bool Property WeaponConditionWidget_Loaded = False Auto Hidden
EndGroup

Group Repairs
	Keyword Property kWorkshopItemKeyword Auto Const
	Keyword Property kWorkshopLinkContainer Auto Const
	Keyword Property kLinkedWorkbenchContainerKeyword Auto Const
	Furniture Property kRepairWorkbenchFurn Auto Const
	Perk Property kWeaponWbRepairPromptPerk Auto Const
	MiscObject Property kRepairWeaponMisc Auto Const
	{Misc item for weapon repairs.}
	MiscObject Property kRepairPartialWeaponMisc Auto Const
	{Misc item for partial weapon repairs.}
	String Property sAddItemtoInventoryGs Auto
	{Message text for item added to player's inventory game setting.}
	Component Property kSteelCmpo Auto Const
	Container Property kWeaponsDegradedCont Auto Const
	{Container used to temporarly store weapons that are considered degraded when using the weapons workbench.}
EndGroup

Group WeaponTypeKeywords
	Keyword Property kWeaponTypeAutomatic Auto Const
	Keyword Property kWeaponTypePossiblyVerySmallClipSize Auto Const
	Keyword Property kWeaponTypeBallistic Auto Const
	Keyword Property kWeaponTypeHeavyGun Auto Const
	Keyword Property kWeaponTypeLaser Auto Const
	Keyword Property kWeaponTypePlasma Auto Const
	Keyword Property kWeaponTypeAlienBlaster Auto Const
	Keyword Property kWeaponTypePipe Auto Const
	Keyword Property kWeaponTypeRifle Auto Const
	Keyword Property kWeaponTypeSniper Auto Const
	Keyword Property kWeaponTypeShotgun Auto Const
	Keyword Property kWeaponTypeGatlingLaser Auto Const
	Keyword Property kWeaponTypeMinigun Auto Const
	Keyword Property kWeaponTypeExplosive Auto Const
	Keyword Property kWeaponTypeFatman Auto Const
	Keyword Property kWeaponTypePistol Auto Const
	Keyword Property kHasScope Auto Const
	Keyword Property kHasSilencer Auto Const
	Keyword Property kHasLegendaryExplosiveBullets Auto Const
	Keyword Property kHasLegendaryTwoShot Auto Const
	Keyword Property kHasLegendaryBleed Auto Const
	Keyword Property kWeaponTypeMelee1H Auto Const
	Keyword Property kWeaponTypeMelee2H Auto Const
	Keyword Property kWeaponTypeUnarmed Auto Const
EndGroup

Group Condition
	ObjectMod[] Property aConditionMod Auto Const
	Keyword[] Property aConditionKwd Auto Const
	Float[] Property aConditionThr Auto Const
	Quest Property kNearbyNpcsQuest Auto Const
	; WeaponInstance Property eqWeapon Auto
	Float Property fDegradationRate Auto
	Bool Property bUpdateNearbyNpcsQuest = True Auto
	Int Property iNumUpdateHealth Auto
	Keyword Property kFurnitureTypePowerArmor Auto Const
	Keyword Property kDegradedWeapKeyword Auto Const
	Bool Property bWidgetUpdating Auto
	Perk Property kWeapCndPenaltyPerk Auto Const
EndGroup

Group Jamming
	Float Property fJammingExp = 0.0740 Auto Const
	Float Property fJammingAutomaticMult = 0.5 Auto Const
	{Jamming chance multiplier for automatic weapons.}
	Bool Property bWeaponJammed Auto
	Ammo Property kOriginalAmmo Auto
	Ammo Property kJammedAmmo Auto Const
	Idle Property kFpReloadLaserMusket Auto Const
	Idle Property kFpReloadNonLM Auto Const
	Idle Property kTpReloadLaserMusket Auto Const
	Idle Property kTpRaiderReload Auto Const
	Idle Property kFpSheath Auto Const
	Idle Property kTpSheath Auto Const
	Keyword Property kAnimsLaserMusket Auto Const
	Sound Property kRifleFireDrySND Auto Const
	String Property sMissingAmmoTextGs Auto
EndGroup


Event OnQuestInit()

	hud = HUDFramework.GetInstance()
	If hud
		hud.RegisterWidget(Self, WeaponConditionWidget, afX = 0.0, afY = 0.0, abLoadNow = True, abAutoLoad = True)
	Else
		Debug.MessageBox("HUDFramework is not installed! Classic Weapon Degradation won't work properly.")
	EndIf
	
	;kPlayerRef.AddPerk(kWeaponWbRepairPromptPerk)
	;kPlayerRef.AddPerk(kWeapCndPenaltyPerk)
	
	; Event for when player crafts an full/partial repair MiscObject
	;AddInventoryEventFilter(kRepairWeaponMisc)
	;AddInventoryEventFilter(kRepairPartialWeaponMisc)
	;RegisterForRemoteEvent(kPlayerRef, "OnItemAdded")
	;RegisterForRemoteEvent(kPlayerRef, "OnItemEquipped")

	;RegisterForAnimationEvent(kPlayerRef, "weaponFire")
	;RegisterForRemoteEvent(kPlayerRef, "OnSit"); PA enter/exit
	;RegisterForRemoteEvent(kPlayerRef, "OnPlayerLoadGame")

	;RegisterForControl("Melee")
	;RegisterForControl("PrimaryAttack")
	;RegisterForControl("ReadyWeapon")
	
	;RegisterForMenuOpenCloseEvent("BarterMenu")
	;RegisterForMenuOpenCloseEvent("ContainerMenu")
	;RegisterForMenuOpenCloseEvent("ExamineMenu")
	;RegisterForMenuOpenCloseEvent("PipboyMenu")

EndEvent


Function HUD_WidgetLoaded(String asWidgetID)
	If asWidgetID == WeaponConditionWidget
		WeaponConditionWidget_Loaded = True
		hud.SendMessage(WeaponConditionWidget, WeaponConditionWidget_SetMaxHealth_Cmd, 1.0)
		If GetEqWeapHealth(kPlayerRef) >= 0
			UpdateWidgetProps()
		Else
			SetWidgetVisibility(0)
		EndIf
	EndIf
EndFunction


Function UpdateWidgetProps()
	StartTimer(0.10, 3)
EndFunction


Function SetWidgetVisibility(Float afVisible)
	If WeaponConditionWidget_Loaded
		hud.SendMessage(WeaponConditionWidget, WeaponConditionWidget_SetVisibility_Cmd, afVisible)
	EndIf
EndFunction


; Spawns an invisible repair workbench at akTargetRef(weapon workbench ref) and forces player to activate it
; Function called from the hxfID_WeaponWbRepairPrompt_Perk entry point fragment
Function OpenRepairWorkbench(ObjectReference akTargetRef)
	; Get Workshop Ref of the activated weapons workbench
	ObjectReference akTargetRefWorkshopRef = akTargetRef.GetLinkedRef(kWorkshopItemKeyword)
	; Spawn the invisibile repair workbench
	ObjectReference kRepairWb = akTargetRef.PlaceAtMe(kRepairWorkbenchFurn, 1, abDeleteWhenAble = True)
	Utility.Wait(0.25)
	If akTargetRefWorkshopRef
		; Proceed linking the spawned repair workbench to the local workshop
		kRepairWb.SetLinkedRef(akTargetRefWorkshopRef, kWorkshopItemKeyword)
		Debug.Trace("akTargetRefWorkshopRef exists " + akTargetRefWorkshopRef.GetDisplayName())
	EndIf
	kRepairWb.Activate(kPlayerRef)
EndFunction


Function IncrementNumUpdates()
	iNumUpdateHealth = iNumUpdateHealth + 1
EndFunction


Event OnTimer(Int aiTimerID)
	If aiTimerID == 1
		bUpdateNearbyNpcsQuest = True
	ElseIf aiTimerID == 2
		If !UI.IsMenuOpen("ExamineMenu")
			Return
		EndIf
		WorkbenchMenuOpened(true)
		StartTimer(0.1, 2)
	; Set Widget props
	ElseIf aiTimerID == 3
		bWidgetUpdating = True
		Float fHealth = GetEqWeapHealth(kPlayerRef)
		If WeaponConditionWidget_Loaded && fHealth >= 0
			If kPlayerRef.IsInPowerArmor()
				hud.SendMessage(WeaponConditionWidget, WeaponConditionWidget_SetProps_Cmd, 0, fHealth) ; PA
			;ElseIf eqWeapon.IsMelee()
				;hud.SendMessage(WeaponConditionWidget, WeaponConditionWidget_SetProps_Cmd, 1, fHealth) ; melee
			Else
				hud.SendMessage(WeaponConditionWidget, WeaponConditionWidget_SetProps_Cmd, 2, fHealth) ; gun
			EndIf
		EndIf
		bWidgetUpdating = False
	EndIf
EndEvent


; When player fires this weapon
Event OnAnimationEvent(ObjectReference akSource, String asEventName)
	Float weapHealth = DegradeEqWeapCnd(kPlayerRef)
	Debug.Trace("Fired weapon. Health left: " + weapHealth)
	UpdateWidgetProps()
EndEvent


; Event used for the jamming feature and nearby npc alias refilling when meleeing
Event OnControlDown(String asControl)

;/ 	If !eqWeapon
		Return
	EndIf
	
	If asControl == "Melee" && kPlayerRef.IsWeaponDrawn()
	
		ResetNearbyNpcsQuest()

	ElseIf asControl == "PrimaryAttack" && kPlayerRef.IsWeaponDrawn()

		If eqWeapon.IsMelee()
			ResetNearbyNpcsQuest()
		ElseIf bWeaponJammed
			kRifleFireDrySND.Play(kPlayerRef)
			; Force sheath the gun
			If kPlayerRef.GetAnimationVariableBool("IsFirstPerson")
				kPlayerRef.PlayIdle(kFpSheath)
			Else
				kPlayerRef.PlayIdle(kTpSheath)
			EndIf
			Debug.Notification("Your weapon has jammed. Reload!")
		EndIf

	ElseIf asControl == "ReadyWeapon"

		If bWeaponJammed
			eqWeapon.UnJamWeapon()
			
			; Play the reload anim because we don't have any ammo in the jammed state
			Idle kReloadMusket
			Idle kReloadNonMusket
			If kPlayerRef.GetAnimationVariableBool("IsFirstPerson")
				kReloadMusket = kFpReloadLaserMusket
				kReloadNonMusket = kFpReloadNonLM
			Else
				kReloadMusket = kTpReloadLaserMusket
				kReloadNonMusket = kTpRaiderReload
			EndIf

			If HasKeyword(kAnimsLaserMusket)
				kPlayerRef.PlayIdle(kReloadMusket)
			Else
				kPlayerRef.PlayIdle(kReloadNonMusket)
			EndIf

		EndIf
	EndIf /;
EndEvent


; Register fire anim event and update condition widget properties when player enters/exits PA
Event Actor.OnSit(Actor akSender, ObjectReference akFurniture)
	If akFurniture.HasKeyword(kFurnitureTypePowerArmor)
		RegisterForAnimationEvent(kPlayerRef, "weaponFire")
		Utility.Wait(0.1)
		UpdateWidgetProps()
	EndIf
EndEvent


Event Actor.OnItemEquipped(Actor akSender, Form akBaseObject, ObjectReference akReference)
	If akBaseObject as Weapon
		Float fHealth = GetEqWeapHealth(kPlayerRef)
		Debug.Trace("This actor equipped a weapon! Health: " + fHealth)
		UpdateWidgetProps()
	EndIf
EndEvent


Event OnMenuOpenCloseEvent(String asMenuName, Bool abOpening)
	If asMenuName != "ExamineMenu"
		Return
	EndIf
	
	If abOpening
		WorkbenchMenuOpened(true)
		StartTimer(0.1, 2)
		Return
	EndIf
	WorkbenchMenuOpened(false)
EndEvent

