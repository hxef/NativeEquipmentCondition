Native Equipment Condition (NEC)
================================

Born as a concept in 2019, it quickly outgrew what the Creation Kit and
Papyrus could reach, so I took the standalone DLL route and built it around
one idea: weapon and armor condition that looks like it was always in the
base game.

Fallout 4 shipped a fairly deep crafting system, with scrap, components and
weapon mods, and still left out repair, a missed chance if there ever was one.
Wear and repair add another layer to the game's economy, giving all that junk
and those caps a real use, and they go hand in hand with Survival, where every
bullet and every scrap already counts.

In 2026, the game finally got the condition system it should've shipped with,
one that looks like part of the game itself and not a mod bolted on as an
afterthought. No feature creep, no bloat. It just works™, except this time it
actually does.

DOES WHAT, EXACTLY?
Your weapons and armor wear down as you use them, the way they did in
Fallout 3 and New Vegas. Worn weapons do less damage, worn armor protects
less, and both sell for less. Keep them in shape at a workbench with
components, or pay a trader in caps.

Worn gear can't be modded until it is repaired to full, so swapping in cheap
mods can't cut the repair bill.

Done natively, in one F4SE plugin: no ESP, no scripts, no new assets besides
an optional MCM page. It reads weapons, armor, recipes, perks, leveled lists
and game settings from your load order as the game loads, for easier
compatibility with other mods, including ones that add new gear.

WEAR AND TEAR
* Your weapon wears with every shot you fire and every melee hit you land.
  Bashes and power attacks wear it harder. Fast automatics like the Minigun
  wear less per shot, as if they fired at an ordinary automatic's pace.
* Every hit that lands wears the armor and clothing on the limb it struck,
  yours and NPCs' alike. Explosions wear every piece. Pieces that cover no
  limb, like a mask or a super mutant's armor, share each hit between them.
* The armor that companions and other essential NPCs come with never wears on
  them, since some of it can never be handed to you to repair. Once a piece
  has passed through your hands, it becomes yours, so it wears like any other
  piece you own, on you or on them.
* Worn weapons do less damage and worn armor protects less, both down to 66%
  at 0 condition.
* A broken piece of armor or clothing stays on and keeps its bonuses, like a
  +1 to a stat or a legendary effect, until you take it off.
* A weapon that breaks is unequipped automatically, and broken gear can't be
  equipped again until it is repaired.
* Your guns can jam once they drop below 50% condition.
* Worn automatic weapons fire slower, down to 75% of their rate at 0
  condition.
* A worn weapon fills the VATS critical meter slower, down to 50% of its rate
  at 0 condition, like in Fallout 3, where worn weapons landed fewer critical
  hits.
* Price drops faster than condition, on the Fallout 3 and New Vegas curve: an
  item at half condition is worth 35% of its full price, whether you buy it or
  sell it.

Power armor wears and is repaired at the power armor station as it always was,
but a damaged piece now sells for less, and so does a drained fusion core.

LOOT WITH A HISTORY
* Weapons and armor turn up already worn. Each gets its condition the first
  time it lands in an inventory: an NPC's, a container's, a trader's stock, or
  yours when you pick up one that was lying around. That happens only once, so
  its condition is never rerolled, no matter who carries it or where it gets
  dropped.
* The condition is a roll, weighted by where the item came from and who
  carried it. A raider's pipe gun has seen better days, while a well equipped
  faction like the Brotherhood or the Gunners keeps its gear in better shape.
  Gear lying around, and your own gear if you install mid playthrough, lands
  at around 45 to 70%, while most quest rewards arrive new, and so does
  everything companions and other essential NPCs start with.
* Legendary gear you find in chests or buy from traders arrives new.
  Legendaries dropped by enemies still turn up worn.
* About 1 item in 100 ignores all that: a raider can drop a pristine rifle,
  and a Courser a beaten one.
* NPC weapons don't lose condition from use, but NPCs still fight at the
  condition their gear is in: their worn weapons deal less damage, and their
  worn armor protects them less.

REPAIR AT THE WORKBENCH
* At the weapon or armor workbench, select a worn item and a REPAIR button
  appears. Press it, choose how far to repair it in steps of 10%, and pay in
  the components it is built from. Gear that scraps into nothing, like
  Grognak's Axe, costs what gear of its kind most often scraps into, and
  wears like it too.
* Barely worn gear, above 95%, gets a MEND button instead, which fixes it on
  the spot for free. The 95% is the Free mend above setting: 100 makes every
  repair cost components, and 0 mends everything but broken gear for free.
* With Workbench repair cost at 0, every repair is free and done on the spot,
  broken gear included.
* Crafting perks like Gun Nut, Science!, Blacksmith and Armorer make repairs
  take fewer components, down to half at the top rank. The perk behind most of
  an item's components sets the discount, and each rank's description says how
  much.
* A repair earns the same XP as crafting a mod from those components.

REPAIR AT A TRADER
* Out of spare components? Gun shops, armorers and clothiers repair their kind
  of gear for caps, and those caps stay in the trader's till, ready to buy the
  loot you haul in next. On the barter screen, select a worn item on your side
  and a REPAIR button appears if they fix that kind of gear. Click it, or
  press C on the keyboard or the left bumper on a pad.
* What a trader repairs, and how far, depends on what they restock. Plenty of
  guns on the shelves, or a few beside plenty of ammunition, grenades and
  mines, means they fix guns, and the more kinds of gun they stock, the
  further they repair them. Armor and clothing work the same way, and so do
  traders from other mods. A new settlement weapon stand stops at 30%, Lucas
  Miller's armor caravan at 70% and Ronnie Shaw's fully upgraded store at 90%,
  give or take with your level. Only the best stocked shops repair to 100%,
  and finding them is up to you.
* Traders also sell each kind of gear in better shape the further they repair
  that kind. Where they repair a kind to 30% or not at all, they sell it at
  30% to 50% condition, and the best shops at 85% to 100%. So a gun shop that
  repairs guns to 90% sells its guns in good shape, but the odd piece of armor
  on its shelves comes in much worse shape, since it doesn't repair armor.

SEE IT AT A GLANCE
* CND on the item cards, with the worn damage, resistances and fire rate.
* CND bars on the HUD ammo counter, the power armor dash and the Pip-Boy
  apparel figure, and condition meters on the quick container rows.
* Broken items fade out in the Pip-Boy lists.

LITTLE EXTRAS
* 6 loading screen tips on wear, repairs, broken gear, jams and loot, in
  the game's own style and as likely to show as any ordinary loading
  screen. A tip about a feature you switch off never shows.
* Vanilla fix: the inspect screen at a trader shows the right price for the
  side the item is on.
* Vanilla fix: a workbench confirmation box with a long list of components
  shows more rows and scrolls with the mouse wheel, the arrow keys, the D-pad
  and W and S.
* A console command for testing: "srm" brings the weapon in hand back to
  full, and "srm 25" sets it to 25%, any number from 0 to 100. Add armor, as
  in "srm armor" or "srm 25 armor", for every piece you have on that wears.

MAKE IT YOURS
* With MCM, NEC's page changes any setting on the spot. Switch off jamming,
  slower fire, worn loot, trader repairs, the HUD condition bars and more, or
  tune wear speed, damage and protection at 0 condition, prices and repair
  costs.
* Without MCM, NEC.ini in Data\F4SE\Plugins explains every setting. Copy a
  line into NEC_custom.ini beside it, under the same section heading, change
  the value and restart the game.
* Either way your changes go in NEC_custom.ini, and NEC.ini stays as it ships.
  The mod comes without a NEC_custom.ini, so an update never wipes your
  settings. With Mod Organizer 2, keep NEC_custom.ini in its own mod or in
  overwrite.

COMPATIBILITY
* No ESP, so it takes no plugin slot.
* The plugin's own text comes in every language the game ships in. The rest is
  the game's own words. Every language but English was translated by AI, not
  by a person, so some lines may read a little off.
* Both vanilla fixes under Little extras can be switched off, for a load order
  where another mod already fixes them.
* A UI replacer that moves the HUD ammo counter, the quick container, the item
  cards or the Pip-Boy can hide the condition shown there. Wear, damage and
  prices work as ever. Repairs use the workbench's own REPAIR button and a
  button on the barter bar, which replacers usually keep.
* NEC works alongside other DLL mods that change the same things. When one
  takes over something NEC changes, NEC lets it, and NEC's MCM page and
  NEC.log name that mod and what is off.

REQUIREMENTS
* Fallout 4 1.11.240
* Fallout 4 Script Extender (F4SE)
  https://www.nexusmods.com/fallout4/mods/42147
* Address Library for F4SE Plugins
  https://www.nexusmods.com/fallout4/mods/47327
* Optional: Mod Configuration Menu
  https://www.nexusmods.com/fallout4/mods/21497

INSTALLATION
Install with your mod manager, or copy the F4SE and MCM folders into
Fallout 4's Data folder. Launch the game through F4SE.

NEC.pdb, beside NEC.dll, is only there for crash logs. The game itself never
uses it. If the game ever crashes, a crash logger like Addictol can read it to
point at the exact line of NEC's code involved, which makes a bug report much
easier to fix. You can delete it without changing anything in play, but then a
crash log can only say that NEC was involved, not where. Addictol is on Nexus
Mods:
  https://www.nexusmods.com/fallout4/mods/84214

Adding it to a playthrough in progress? Weapons and armor with no condition
yet, yours included, get one as the save loads, rolled the same way. To keep
what you carry as new, set bSpawnCondition=false under [Features] in
NEC_custom.ini before you start the game. Once the save has loaded, switch
Worn loot back on in MCM, not in the ini, and save. Stored gear is still
rolled.

UNINSTALLING
Repair anything broken first, at a workbench, at a trader or, for gear you
still have on, with "srm" in the console. Fallout 4 still has the rule from
Fallout 3 and New Vegas that an item at 0 condition can't be equipped, and
once NEC is gone nothing in the game can repair it, so it stays unusable.

After that, removing the mod is safe. It has no ESP and keeps no save data of
its own. NEC keeps each item's condition in its health, one of the small tags,
called extra data, that Fallout 4 already saves on an item, like its custom
name. Power armor pieces use health too. That health is all that stays in your
save. On anything but power armor the game ignores it for damage, protection
and price, so what is left behind does nothing there, except that identical
items at different conditions stay in separate stacks in your inventory.

FOUND A BUG?
If a feature seems to do nothing, another DLL mod may have taken it over.
Check the top of NEC's MCM page, or search NEC.log for "left to".

Otherwise, turn on NEC's full logs and play until the bug happens again. With
MCM, set Log detail to debug and switch on Bug report logs. Without it, add
this to NEC_custom.ini in Data\F4SE\Plugins and restart the game:
  [Log]
  sLogLevel=debug
  bTraceLogs=true
Then share these files from Documents\My Games\Fallout4\F4SE:
* NEC.log
* NEC.trace.log, NEC.ui.trace.log and NEC.npc.trace.log
* The crash log if the game crashed, like crash-2026-10-01-21-15-31.log from
  Addictol
Say what you did right before it went wrong, and post it all on the Bugs tab
of the mod's Nexus Mods page or as an issue on GitHub (link below). The bug
report logs get big fast, so switch them off again once you're done.

SOURCE CODE
NEC is open source under the GPL 3.0 license. Browse the code, report a bug or
send a fix on GitHub:
  https://github.com/hxef/NativeEquipmentCondition

CREDITS
* The F4SE Team, for the Fallout 4 Script Extender.
* Ryan (Ryan-rsm-McKenzie), for CommonLibF4 and the Address Library for F4SE
  Plugins.
* shad0wshayd3, qudix and every contributor to libxse's CommonLibF4, the fork
  NEC is built on.
* The F4MCM authors, for the Mod Configuration Menu.
