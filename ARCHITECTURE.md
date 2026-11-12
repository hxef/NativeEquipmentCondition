# How the plugin is put together

Native Equipment Condition is one DLL that gives Fallout 4 weapons and armor a
condition that wears down with use. It patches about 120 places in the
engine's code, reads what the load order says about weapons, armor, recipes,
perks and leveled lists, and draws its readouts into Fallout 4's own menus. It
ships no ESP and no assets, only the DLL, `NEC.ini` beside it, 1 language file
per language in `NEC/Translations`, a page for the Mod Configuration Menu and
the Papyrus API script `NEC.pex` for other mods.

Read `src/Features.cpp` next. It lists every feature in the order it runs.
Then read the header of whichever feature you are here for. Every header opens
with what the game does there and what the plugin does about it.

## Layers

```
src/Core/       plumbing every layer may use
src/Condition/  what condition is, and what putting it right costs
src/Gameplay/   hooks that change play
src/UI/         what is drawn into Fallout 4's own menus
src/Papyrus/    the script NEC that other mods call, bound to the game's script VM
```

A file includes only from its own layer and the layers above it in that list.
`src/main.cpp`, `src/Session.cpp` and `src/Features.cpp` sit outside the ladder
and are the only files that know every feature. Every include is written from
`src`, so a file says `#include "UI/Repair/Workbench/Cost.h"` wherever it is.

## Files

```
src/
  main.cpp                   the entry point, which walks the feature table
  Session.h/.cpp             forgetting the last game and measuring the load order again, at a load, a new game or the main menu
  Features.cpp               every feature, in the order it runs
  pch.h                      what every file includes first

  Core/
    CallPatch/
      CallPatch.h/.cpp       patching a call or a function table slot, shared with another DLL or left to it
      Site.h                 where a call site or a table slot is on 1.11.240 and on OG, 1.10.163
      Link.h                 what a hook hands on to and the set it is in
      Ledger.h/.cpp          every place patched, written once its row's install ends
      Write.h/.cpp           writing a noted place into the game, the Link first
      Sets.cpp               whether a set's hooks run, and the proof that brings a shared set back
      Recheck.cpp            every noted place read back as the game runs, shared or left alone
      Changed.h/.cpp         the NEC.log lines of a place another DLL changed, and the DLL each names
      MenuWait.h/.cpp        the 5 places every open of their menu calls, and those menus' opens and closes
      Walk.cpp               following a place's hooks from the outside in, and the shape NEC builds on
      Reach.h/.cpp           reading a loaded module's image, the memory the walk is built on
      Owners.cpp             who has a place, and how NEC.log names them
      Losses.cpp             which pieces of each part are off, and why
      Summary.cpp            NEC.log's summary lines: what is left to other mods, what is shared, what that leaves of each setting
    BenchRow.h/.cpp          the row a workbench's item list has highlighted, for the repair and the SCRAP button
    Feature.h                the shape of a feature and its 4 moments
    IniText.h/.cpp           one key changed in an ini file's text, every other line kept
    ItemCards.h/.cpp         asking the Pip-Boy to rebuild its item cards
    ListFiles.h/.cpp         reading the lists of forms a mod ships in a folder under NEC, such as Exclude
    LogFiles.h/.cpp          opening NEC.log and the other logs in their own folder, and keeping those of the 10 game starts before
    LogLevel.cpp             NEC.log's level, from sLogLevel or the MCM page, declared in Settings.h
    Parts.h/.cpp             every part another mod can take from NEC, and what each setting works through
    Pieces.h/.cpp            the pieces of every part, and what each piece needs
    Places.cpp               which pieces each patched place is
    Plugin.h                 the data handler the load order is read through
    Runtime.h/.cpp           the game version NEC runs on, 1.11.240 or OG, and NEC.log's line about it
    ScriptEvents.h/.cpp      the events other mods' scripts hear, as every place that changes an item notes them, and the gate of a save load
    Settings.h/.cpp          every setting, read from NEC.ini, and a change kept in NEC_custom.ini
    SettingsLine.cpp         NEC.log's Settings line, and every switch and number in NEC.ini's order
    SettingsTuning.h         the settings under [Balance] and [HUD], which Settings.h includes
    Snapshot.h               a table other threads read while it is measured again, swapped in whole, and how 2 readings differ
    Text/
      Text.h/.cpp            the plugin's own sentences, 1 function each
      Translations.cpp       reading the player's language file, and the line each key gives
      FileText.h/.cpp        what a language file must be: its encoding and the {n} slots of each line
      Lines.h                what the files here and in Names and Menu share, and the keys the build writes from en.ini
      Repair.cpp             the repair question and the bench's sentences
      Trader.cpp             what a trader says about a repair, in the barter screen's word for caps
      Tips.cpp               the loading screen's tips
      Names/
        PartsPlay.cpp        the names of the parts that change play, up to Fertilizer from spoiled food
        PartsLoot.cpp        the names of the Worn loot parts, from Worn loot to Quest rewards arrive new
        PartsUi.cpp          the names of the parts drawn in menus
        PiecesPlay.cpp       the names of the pieces of the parts that change play, from Perk repair discount text to Damage shown in menus
        PiecesUi.cpp         the names of the pieces of the parts drawn in menus
        PiecesArmor.cpp      the names of the pieces of Worn armor protection, Protection shown in menus and Broken gear rules
        Phrases.cpp          what a setting's sentence says in place of a piece's name
      Menu/
        MenuSwitches.cpp     the MCM page's switches: help, and the bug report logs switch's name
        MenuWear.cpp         the MCM page's wear speeds, yours, NPCs' and automatic weapons': names and help
        MenuNumbers.cpp      the MCM page's other Balance numbers: names and help
        MenuHudLog.cpp       the MCM page's HUD settings and log level: names and help, and the HUD bar sliders' default
        MenuNotes.cpp        the MCM page's grey lines about parts left to other mods
        MenuPieces.cpp       a part with its pieces in brackets, what still works, and a setting's sentence with pieces off
    TraceLog.h/.cpp          the trace logs, for lines that arrive in floods
    TraceHold.h/.cpp         a script call's Papyrus trace lines, held until the call knows which try counts
    TraceWords.h/.cpp        how a trace line names an actor and a damage type, which TraceLog.h includes

  Condition/
    Condition.h/.cpp         the reading, its bounds, the share a stat keeps, what takes part, the write
    Crew.h/.cpp              the share of the NPC rates the player's companions' and settlers' gear wears at
    Equipped.h/.cpp          what an actor holds and has on, and writing to the copy in use
    Excluded.h/.cpp          the weapons and armor a patch's file in NEC/Exclude tells NEC to leave alone
    UniqueID.h/.cpp          the number that names 1 stack inside a container to a script
    Materials/
      Materials.h/.cpp       what an item is built from, the mod recipe index and the ordinary make
      Index.h                the index, for this folder
      Bill.cpp               the bill of parts and the order it is charged in
      Components.cpp         which component prices a form, a junk item by its own Components list
      Scrap.cpp              the scrap recipes the bench reads and the lists they name, read at Load, again at every save load, new game and coc from the main menu, and where a script can add to one
      Yield.h/.cpp           how many of a component a worn copy's scrap count goes in as, and what a spare is worth
    Repair.h/.cpp            what a worn item owes
    WeaponWear/
      WeaponWear.h/.cpp      which weapons take part, wearing the copy in hand
      Rate.h/.cpp            how fast a weapon wears
      Copy.h                 wearing the copy in hand, the write the player's wear and an NPC's share
      Theirs.cpp             an NPC's weapon wear, which never waits for their inventory
    ArmorWear/
      ArmorWear.h/.cpp       which armor takes part, wearing a piece somebody has on
      Mods.h/.cpp            which armor mods add protection, an effect or a bonus, the first row to load
      Skins.h/.cpp           which pieces the game wears as a race's or a character's skin, measured with the mods
      Rate.h/.cpp            how fast a piece of armor wears
      Health.h/.cpp          how much longer a copy lasts for the mods that change its Health
    CraftingPerks/
      CraftingPerks.h/.cpp   the perk behind an item, the player's rank in it
      Ladder.h/.cpp          walking a perk's ring of ranks
      Perks.h/.cpp           what every recipe says about perks: the gates, the fallbacks and the perk page lines
      Recipes.h/.cpp         which perks a recipe needs and what its mod reaches, a weapon's kind
      Description.h/.cpp     the line every rank of a crafting perk and of Scrapper grows on the perk page
    Provenance/
      Provenance.h/.cpp      where an item came from and who carries it
      Supply.h/.cpp          walking the leveled lists, where each one sits
      Care.h/.cpp            what a character is issued, where they sit
      Rank.h                 where a measurement sits against the rest

  Gameplay/
    ArmorEvents.h/.cpp       wear on the armor a blow lands on, on anybody
    ArmorRating.h/.cpp       a worn piece of armor protects less
    CritMeter.h/.cpp         fewer criticals from a worn weapon
    BrokenEquip.h/.cpp       a broken item comes off and stays off until repaired
    ItemValue.h/.cpp         a worn item is worth less
    InHand.h/.cpp            the condition of the weapon in the player's hands, measured once a frame
    Jam.h/.cpp               a worn gun can jam, and the reload call
    FireRate/
      FireRate.h/.cpp        a worn automatic weapon fires slower
      Cuts.h/.cpp            a worn blade's held attack cuts less often
    FoodSpoil/
      FoodSpoil.h/.cpp       food that spoils with time, its 4 parts, and food carried before it was on
      Food.h                 what the folder's files share
      Kinds.h/.cpp           each food's kind from the game data: raw, crop, cooked or never spoils
      FoodList.cpp           the kinds a mod gives its own food, from NEC/Food, and every food as a line of that list
      FridgeList.cpp         the containers that keep food as a fridge does, from NEC/Fridge
      FoodLog.cpp            NEC.food.log: the settings in force, what each list file does, and every food's kind
      Fridge.cpp             food going into or out of a fridge keeps its days spent at the fridge speed
      Stamp.h/.cpp           the day food started to spoil, how fresh it is, found food's start, and the door that stamps it
      Order.cpp              the stack closest to spoiling first
      Eat.cpp                spoiled food heals a tenth, nothing in Survival, and gives more rads
      Rads.cpp               the rads spoiled food adds, through the food's own rads effect
      Hunger.cpp             spoiled food fills a tenth of the hunger in Survival
      Sickness.cpp           spoiled food makes you sick 5 times as often in Survival
      Cook.cpp               cooking and chemistry stations leave spoiled food out and take fresh pieces only
      Fertilizer.cpp         a chemistry station's Fertilizer row turns 10 spoiled food into 1 Fertilizer
      Bodies.cpp             food on the dead starts to spoil
      Sweep.cpp              food whose time ran out turns spoiled as a menu opens
    Aim/
      Aim.h                  a worn gun is less accurate and carries its damage less far
      Accuracy.cpp           every shot's spread, the crosshair and the VATS hit chance
      Range.cpp              how soon a ranged hit's damage falls off
    HealthDamage/
      HealthDamage.h/.cpp    the feature and its install
      Hooks.h                what the folder's files share
      Curve.h                the damage line
      Trace.h/.cpp           telling a repeated reading from a new one
      Combat.cpp             the blow: physical damage, damage types
      Effect.cpp             the object effects a blow or a shot casts
      Blast.cpp              an explosion: its damage and its object effect
      Card.cpp               the item card's damage
    Scrapping/
      Scrapping.h/.cpp       worn gear scraps into fewer components, and the count the 2 places share
      Bench.cpp              the bench's SCRAP list, scaled by the copy the yes takes
      Workshop.cpp           workshop mode's scrap lists, scaled by the object scrapped
    SpawnCondition/
      SpawnCondition.h/.cpp  the roll at the door of every inventory
      Band.h/.cpp            the roll, either side of an item's middle or in a trader's band
      Fill.h/.cpp            a list filled from a container's record, its plain lines and its stock record
      Food.h/.cpp            found food's starting freshness
      Guards.h/.cpp          what marks a save being applied, a game recording no change, a console command, a script's gift and a restock, the folder's scope, and what the player carries
      Handover.h/.cpp        an item changing hands and SpawnDupe's copies
      Look.cpp               the first look at loot lying in the world, rolled as the player's pickup would roll it
      LooseItem.cpp          a trader's loose item, 1 roll for the copy on the barter screen and the item in the world
      Owners.h/.cpp          who a stack goes to, and which of them get it at full condition
      Roll.h/.cpp            a stack's first condition, and splitting a stack that arrived several at once
      Trace.h/.cpp           what the trace log is told about a stack
      World.h/.cpp           loot lying in the world, marked when the game makes it and rolled by its first holder
    WeaponEvents/
      WeaponEvents.h/.cpp    the fire call and the shot sink
      Hits.h/.cpp            the hit sink, and what a blow was

  UI/
    Flash.h/.cpp             safe reads, writes and calls on a menu movie's objects, their colour, boxes and points, a text field's first line, and text written for it
    MenuMovies.h/.cpp        the one doorway into Fallout 4's menu movies
    MessageBox.h/.cpp        a question with as many answers as it is given, and Tab and Escape on it
    MessageBoxAsked.h        the list of NEC's boxes the 2 message box files share
    MessageBoxMovie.cpp      the row NEC's box starts on, and the box kept solid under a message box mod
    LoadingTips.h/.cpp       the plugin's own tips on the loading screen
    InspectPrice.h/.cpp      the price on the inspect screen at a trader
    FridgeIcon.h/.cpp        the fridge icon the quick container and the item card share, made and sized to a line of text
    PerkChart.h/.cpp         every line of a perk's description on the level up perk chart
    Roles/
      Roles.h/.cpp           the tests every part shares: on screen, a part's box, a point to and from the stage, the found, noted and missing lines
      Conventions.h          the method names some menu movies share beyond the vanilla ones
      Bars.h/.cpp            every button hint list a menu draws
      Card.h/.cpp            the item card and its CND row
      Hud.h/.cpp             the ammo counter, its divider, numbers and colour, the parts beside it, where a quick container row's name starts and ends
      ContainerName.h/.cpp   the container's name at the top of the quick container
      TraderSide.h/.cpp      the trader's side of the barter screen, the bracket around their list, its title, what it draws and what the screen draws beside it
      Rollover.h/.cpp        the pickup prompt: the name and the button hints under it
      PerkCard.h/.cpp        the perk chart's card and the field its description prints in
      Boxes.h/.cpp           the parts of the bench's confirmation box, where the bench's button bar starts, the message box's background, and whether the barter screen's quantity box is open
    Mcm/
      Mcm.h/.cpp             the settings page in the Mod Configuration Menu
      Bridge.h/.cpp          the page's answers: values, changes and words
      Notes.cpp              the page's grey rows: each block's note, and what MCM asks of them
      TopList.cpp            the list at the top of the main page, of the parts no note names
      Fit.cpp                how tall a grey row may be, and a row cut to fit
      Layout.cpp             MCM's list in None mode while one of NEC's pages shows, and the HUD bar sliders' live ends
      HudBar.cpp             the HUD bar sliders: the bar's place in HUD units from the screen's top left, with the screen as their range
    Hud/
      HudParts/
        HudParts.h/.cpp      what the HUD readouts share
        Readout.cpp          the readout itself, a bar and the word CND
        Draw.cpp             the boxes and text fields a readout is built from
      HudCondition/
        HudCondition.h/.cpp          the Weapon CND bar by the ammo counter: the movie
        Frame.h/.cpp                 the frame listener, which follows the counter, draws the bar's level and fades it
        Listener.h                   the frame listener's class, which Frame.cpp and Layout.cpp share
        Layout.cpp                   the frame listener laying the bar out at its spot and size and placing it on screen
        Spot.h/.cpp                  where the bar and its word go, the divider's place or the nearest free spot, the NEC.log line, and the bar at its size
        Place.h/.cpp                 where the readout sits in the counter's parent, by the counter or standing alone from the screen's top left corner, moved by fHudBarX and fHudBarY and held whole on screen, and that place kept for the MCM page
      PowerArmorCondition/
        PowerArmorCondition.h/.cpp   the frame listener and the movie
        Dash.h/.cpp                  finding the dash through its own camera
        Layout.h/.cpp                laying the bar out in the movie
      PickupBar/
        PickupBar.h/.cpp             the Pickup CND bar under the name in the pickup prompt: its install and the movie
        Look.h/.cpp                  the item the prompt stands for, as the game builds it, and its roll on the first look
        Bar.h/.cpp                   the bar in the HUD's prompt
      QuickContainer/
        QuickContainer.h/.cpp        the feature: its install and the movie
        Rows.h/.cpp                  the rows as the game builds them
        Meters.h/.cpp                the meters on the HUD's rows
        RowLayout.h/.cpp             how a row fits its name, icons and meter
        Draw.h/.cpp                  how one meter looks
        FridgeIcon.h/.cpp            the icon beside a fridge's name
    Inventory/
      Pipboy.h/.cpp          a worn out item's or spoiled food's name faded in the Pip-Boy
      PaperDoll.h/.cpp       a CND bar over each region of the apparel tab's paper doll
      ItemCard/
        ItemCard.h/.cpp      the feature: its install and the movies
        Cards.h              what the folder's files share
        Hooks.cpp            the CND row
        Food.cpp             the CND row on food, 0 once spoiled
        Rate.cpp             the fire rate a card prints, and the sort's note of each stack
        Aim.cpp              the accuracy and range a card prints, and the sort by accuracy
        Raise.cpp            moving the row up past Damage
        FridgeIcon.cpp       the icon on the CND row of food in a fridge
        Effects.cpp          what spoiled food gives, on its card, the HP bar and STAT Effects
    Repair/
      SelectedItem.h/.cpp    the item highlighted in a menu's list
      Restore.h/.cpp         writing a repair onto one copy of an item
      RepairPrompt.h/.cpp    asking how far to bring a worn item back
      Gains.h/.cpp           what a repair adds to the card's damage or resistances by type
      ConsoleRepair.h/.cpp   setting the weapon in hand or the armor worn from the console
      ConfirmScroll.h/.cpp   the box the bench puts up, grown and scrolled
      Columns/               the repair boxes' rows in columns: the cells measured, the rows fitted to the box's room and sent as html
      Workbench/             repairing at the weapon and armor benches
      VendorRepair/          paying a trader to repair weapons, armor or clothing, their stock and the sign under their caps, above their list or left of it

  Papyrus/
    Papyrus.h                the script NEC that other mods call
    Natives.h                every function of NEC.psc that NEC.dll answers, as the script VM calls it
    Items.h/.cpp             finding the item a script names again at every call, GetHolder, GetBase, GetCount
    Handles.cpp              GetItems, GetEquippedWeapon, GetWornItem, GetWornItems
    Numbering.h/.cpp         numbering a stack for its handle
    Writer.cpp               SetCondition, Damage, Repair, Wear and RollCondition, through 1 writer
    Readers.cpp              IsTracked, WhyNotTracked, GetCondition, IsBroken and IsUnrolled
    Curves.cpp               the shares and the jam chance NEC applies at a condition, and the wear of 1 use
    Repair.cpp               the bench's bill and free mend, the mod lock, scrap, spares, a trader's limit and price
    Food.cpp                 a food's kind, days, freshness and fridge, and setting it fresh or spoiled
    Events.h/.cpp            the task that runs what a write leaves and hands the waiting events to the scripts registered for them
    Works.cpp                what a write leaves for that task: the item cards, the armor rating, putting a broken weapon away
    Pack.h/.cpp              an event's arguments packed as exactly the types its callback declares
    Log.h/.cpp               NEC.papyrus.log: each script VM built, the scripts each event reached first, and the errors in a script's calls
    Register.cpp             the bind, which F4SE runs each time the game builds its script VM
    Waiting.h/.cpp           a call that finds an inventory locked, run again at the VM's next update
    Version.cpp              GetVersion, GetVersionString, and NEC.pex's version checked against NEC.dll's
    Settings.cpp             IsFeatureOn, IsFeatureTakenOver and the settings by their MCM ids
```

The words themselves are in `publish/Translations`, 1 file per language, and
the files under `Text/` hold keys, not words. Each build writes
`Core/Text/Keys.h` from `en.ini` into the build folder and checks the other
files, see `xmake/texts.lua`.

The script's source is `publish/Scripts/Source/User/NEC.psc.in`, and its
comments are the API's document. Each build writes `NEC.psc` from it with the
version and compiles `NEC.pex` from that, see `xmake/papyrus.lua`. `xmake
apidoc` writes the document from those comments into
`publish/NEC_Papyrus_API.txt` and `publish/nexusmods/article_api.bbcode`, and
`xmake release` stops when either differs from a fresh run, see
`xmake/apidoc.lua`.

A feature that has grown past one pair of files has a folder named after its
namespace: the feature's own header and source, a private header for what the
files share, and one file per job.

## How it runs

A feature is a namespace with up to 4 entry points, see `src/Core/Feature.h`,
and one row in `src/Features.cpp`. `src/main.cpp` walks the rows:

1. **Install**, once, after every plugin has loaded and before any game
   thread runs. Code is patched. No game data exists yet.
2. **Load**, every time game data has loaded: as the game starts, and again
   after every full reset, which is the game deleting every form and loading
   every file again after a prompt about changed DLC, Creations or a save's
   load order. Everything measured from the load order is measured here first,
   and a patch's list in `Data/F4SE/Plugins/NEC/Exclude` is read, which takes
   the items it names out of everything NEC does, see
   `src/Condition/Excluded.h`.
   Just before it, `src/main.cpp` runs `Text::Load`, which reads the player's
   language file, see `src/Core/Text/Text.h`.
3. **Unload**, just before a full reset deletes every form, in reverse order,
   so a feature lets go before anything it read from does.
4. **OnMovieLoaded**, for every menu movie the game loads, through
   `src/UI/MenuMovies.cpp`.

The script VM comes apart at every full reset, so the Papyrus layer binds its
functions each time F4SE asks and keeps no VM. A function reads NEC's live
values at every call and keeps nothing for the next. An item function holds
the item's inventory as a writer while it reads or writes, and what may not
run under that lock goes as 1 task on F4SE's queue. A place that changes an
item notes its event in `src/Core/ScriptEvents`, and that task hands it on,
so no script runs inside NEC.

Outside the 4 steps, `src/Session.cpp` drops what play remembers of the last
game, the gun that jammed, the fire speeds, the weapon in hand, the HUD's last
reading and the NPC wear still waiting for a busy inventory, when a save
loads, a new game begins or the main menu opens. As a save loads, a new game
begins or a coc from the main menu leaves its loading screen, it also
measures again what a DLL mod can change after game data loads, in an order
its comments explain. Every loading screen reads the scrap lists again as it
closes, for an item a script added to one. Once a save has loaded with Worn
loot on, it sets the weapons and armor the player carries with no condition
to full.

Every row runs at every step whatever its switch says. Its hooks ask the
switch on every call and leave the game as it is while it is off. A row whose
switch is left to another mod is not installed and adds nothing to a movie,
see `src/Core/Feature.h`. Every setting is read once, as the game starts, see
`src/Core/Settings.h`, and the MCM page changes any of them on the spot, see
`src/UI/Mcm/Mcm.h`.

Every place NEC patches belongs to a part, see `src/Core/Parts.h`, the name
NEC.log and the MCM page use. Where another DLL patches a place too, NEC
shares it when it can. When it cannot, NEC leaves that place to the DLL, and
NEC.log and the MCM page name each piece that goes off with it. NEC reads
every place back as the game runs and never writes game code after it
installs, see `src/Core/CallPatch/CallPatch.h`.

Weapons and armor are the 2 kinds of item that wear. Each kind is a folder
under `src/Condition/`, one case in `Condition::WhyNoCondition`, a start in
`Condition::Start`, a reader in `src/Condition/Equipped.h`, and its own
Gameplay features, each a row in `src/Features.cpp`. The item cards, the
Pip-Boy, the quick container and the price read `Condition::Percent` and
`Condition::WearsOut`, the CND bars round with `Condition::WholePercent`, and
none of them ask what kind of item it is. The repairs and the roll at the door
ask it only where weapons and armor part ways, and a sentence that has to name
the kind asks `Condition::KindOf`. Another kind would be the same again.
