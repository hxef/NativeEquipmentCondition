# How the plugin is put together

Native Equipment Condition is one DLL that gives Fallout 4 weapons and armor a
condition that wears down with use. It patches about 100 places in the
game's own code, reads what the load order says about weapons, armor, recipes,
perks and leveled lists, and draws its readouts into the game's own menus. It
ships no ESP, no scripts and no assets, only the DLL, `NEC.ini` beside it and
a page for the Mod Configuration Menu.

Read `src/Features.cpp` next. It lists every feature in the order it runs.
Then read the header of whichever feature you are here for. Every header opens
with what the game does there and what the plugin does about it.

## Layers

```
src/Core/       plumbing every layer may use
src/Condition/  what condition is, and what putting it right costs
src/Gameplay/   hooks that change play
src/UI/         what is drawn into the game's own menus
```

A file includes only from its own layer and the layers above it in that list.
`src/main.cpp` and `src/Features.cpp` sit outside the ladder and are the only
files that know every feature. Every include is written from `src`, so a file
says `#include "UI/Repair/Workbench/Cost.h"` wherever it is.

## Files

```
src/
  main.cpp                   the entry point, which walks the feature table
  Features.cpp               every feature, in the order it runs
  pch.h                      what every file includes first

  Core/
    CallPatch/
      CallPatch.h/.cpp       patching a call or a function table slot, shared with another DLL or left to it
      Link.h                 what a hook hands on to and the set it is in
      Ledger.h/.cpp          every place patched, written once its row's install ends
      Sets.cpp               whether a set's hooks run, and the proof that brings a shared set back
      Recheck.cpp            every noted place read back as the game runs, shared or left alone
      Walk.cpp               following a place's hooks from the outside in, and the shape NEC builds on
      Reach.h/.cpp           reading a loaded module's image, the memory the walk is built on
      Owners.cpp             who has a place, and how NEC.log names them
      Losses.cpp             which pieces of each part are off, and why
      Summary.cpp            NEC.log's summary lines: what is left to other mods, what is shared, what that leaves of each setting
    Feature.h                the shape of a feature and its 4 moments
    IniText.h/.cpp           one key changed in an ini file's text, every other line kept
    ItemCards.h/.cpp         asking the Pip-Boy to rebuild its item cards
    LogFiles.h/.cpp          opening NEC.log and the 3 bug report logs, and keeping those of the 10 game starts before
    Parts.h/.cpp             every part another mod can take from NEC, and what each setting works through
    Pieces.h/.cpp            the pieces of every part, and what each piece needs
    Plugin.h                 the data handler the load order is read through
    Settings.h/.cpp          every setting, read from NEC.ini
    Text/
      Text.h/.cpp            the plugin's own sentences, in every language
      Lines.h                what the files here and in Names and Menu share
      Repair.cpp             the repair question and the bench's sentences
      Trader.cpp             what a trader says about a repair
      Tips.cpp               the loading screen's tips
      Names/
        PartsPlay.cpp        the names of the parts that change play
        PartsUi.cpp          the names of the parts drawn in menus
        PiecesPlay.cpp       the names of the pieces of the parts that change play, from Perk repair discount text to Damage shown in menus
        PiecesUi.cpp         the names of the pieces of the parts drawn in menus
        PiecesArmor.cpp      the names of the pieces of Worn armor protection, Protection shown in menus and Broken gear rules
      Menu/
        MenuSwitches.cpp     the MCM page's switches: help, and the bug report logs switch's name
        MenuNumbers.cpp      the MCM page's Balance numbers: names and help
        MenuHudLog.cpp       the MCM page's HUD numbers and log level: names and help
        MenuNotes.cpp        the MCM page's grey lines about parts left to other mods
        MenuPieces.cpp       a part with its pieces in brackets, what still works, and a setting's sentence with pieces off
    TraceLog.h/.cpp          the trace logs, for lines that arrive in floods

  Condition/
    Condition.h/.cpp         the reading, its bounds, what takes part, the write
    Equipped.h/.cpp          what an actor holds and has on, and writing to the copy in use
    Materials/
      Materials.h/.cpp       what an item is built from, the recipe index and the ordinary make
      Index.h                the index, for this folder
      Bill.cpp               the bill of parts and the order it is charged in
    Repair.h/.cpp            what a worn item owes
    WeaponWear/
      WeaponWear.h/.cpp      which weapons take part, wearing the copy in hand
      Rate.h/.cpp            how fast a weapon wears
    ArmorWear/
      ArmorWear.h/.cpp       which armor takes part, wearing a piece somebody has on
      Mods.h/.cpp            which armor mods add protection, an effect or a bonus, the first row to load
      Rate.h/.cpp            how fast a piece of armor wears
    CraftingPerks/
      CraftingPerks.h/.cpp   the perk behind an item, the player's rank in it
      Ladder.h/.cpp          walking a perk's ring of ranks
      Recipes.h/.cpp         which perks a recipe needs and what its mod reaches, a weapon's kind
      Description.h/.cpp     the line every rank grows on the perk page
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
    Jam.h/.cpp               a worn gun can jam, and the reload call
    FireRate/
      FireRate.h/.cpp        a worn automatic weapon fires slower
      Cuts.h/.cpp            a worn blade's held attack cuts less often
    HealthDamage/
      HealthDamage.h/.cpp    the feature and its install
      Hooks.h                what the folder's files share
      Curve.h                the damage line
      Trace.h/.cpp           telling a repeated reading from a new one
      Combat.cpp             the blow: physical damage, damage types
      Effect.cpp             the object effects a blow or a shot casts
      Blast.cpp              an explosion: its damage and its object effect
      Card.cpp               the item card's damage
    SpawnCondition/
      SpawnCondition.h/.cpp  the roll at the door of every inventory, and armor NEC does not wear put back to full
      Band.h/.cpp            the roll, either side of an item's middle or in a trader's band
      Guards.h/.cpp          what marks a console command, a save, a script's gift, a restock and what the player carries
      Owners.h/.cpp          who a stack goes to, and which of them get it at full condition
      Trace.h/.cpp           what the trace log is told about a stack
    WeaponEvents/
      WeaponEvents.h/.cpp    the fire call and the shot sink
      Hits.h/.cpp            the hit sink, and what a blow was

  UI/
    Flash.h/.cpp             safe reads, writes and calls on a menu movie's objects, and text written for it
    MenuMovies.h/.cpp        the one doorway into the game's menu movies
    MessageBox.h/.cpp        a question with as many answers as it is given, kept solid under a message box mod
    LoadingTips.h/.cpp       the plugin's own tips on the loading screen
    InspectPrice.h/.cpp      the price on the inspect screen at a trader
    Roles/
      Roles.h/.cpp           the tests every part shares: on screen, the found, noted and missing lines
      Conventions.h          the method names some menu movies share beyond the game's own
      Bars.h/.cpp            every button hint list a menu draws
      Card.h/.cpp            the item card and its CND row
      Hud.h/.cpp             the ammo counter and its divider, where a quick container row's name starts and ends
      Boxes.h/.cpp           the parts of the bench's confirmation box, where the bench's button bar starts, and the message box's background
    Mcm/
      Mcm.h/.cpp             the settings page in the Mod Configuration Menu
      Bridge.h/.cpp          the page's answers: values, changes and words
      Notes.cpp              the page's grey rows: the list at the top and each block's note
      Layout.cpp             MCM's list in None mode while NEC's page shows
    Hud/
      HudParts/
        HudParts.h/.cpp      what the HUD readouts share
        Readout.cpp          the readout itself, a bar and the word CND
      HudCondition.h/.cpp    the CND bar in the ammo counter
      PowerArmorCondition/
        PowerArmorCondition.h/.cpp   the frame listener and the movie
        Dash.h/.cpp                  finding the dash through its own camera
        Layout.h/.cpp                laying the bar out in the movie
      QuickContainer/
        QuickContainer.h/.cpp        the feature: its install and the movie
        Rows.h/.cpp                  the rows as the game builds them
        Meters.h/.cpp                the meters on the HUD's rows
        Draw.h/.cpp                  how one meter looks
    Inventory/
      Pipboy.h/.cpp          a worn out item's name faded in the Pip-Boy
      PaperDoll.h/.cpp       a CND bar over each region of the apparel tab's paper doll
      ItemCard/
        ItemCard.h/.cpp      the feature: its install and the movies
        Cards.h              what the folder's files share
        Hooks.cpp            the CND row
        Rate.cpp             the fire rate a card prints
        Raise.cpp            moving the row up past Damage
    Repair/
      SelectedItem.h/.cpp    the item highlighted in a menu's list
      Restore.h/.cpp         writing a repair onto one copy of an item
      RepairPrompt.h/.cpp    asking how far to bring a worn item back
      ConsoleRepair.h/.cpp   setting the weapon in hand or the armor worn from the console
      ConfirmScroll.h/.cpp   the box the bench puts up, grown and scrolled
      Workbench/             repairing at the weapon and armor benches
      VendorRepair/          paying a trader to repair weapons, armor or clothing, and their stock
```

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
   load order. Everything measured from the load order is measured here first.
3. **Unload**, just before a full reset deletes every form, in reverse order,
   so a feature lets go before anything it read from does.
4. **OnMovieLoaded**, for every menu movie the game loads, through
   `src/UI/MenuMovies.cpp`.

Outside the 4 steps, `src/main.cpp` drops what play remembers of the last
game, the gun that jammed, the fire speeds and the HUD's last reading, when a
save loads, a new game begins or the main menu opens. As a save loads or a new
game begins, it also measures again what a DLL mod can change after game data
loads, in an order its comments explain.

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
under `src/Condition/`, one case in `Condition::WhyNoCondition`, a reader in
`src/Condition/Equipped.h`, and its own Gameplay features, each a row in
`src/Features.cpp`. The item cards, the Pip-Boy, the quick container and the
price read `Condition::Percent` and `Condition::WearsOut`, the CND bars round
with `Condition::WholePercent`, and none of them ask what kind of item it is.
The repairs and the roll at the door ask it only where weapons and armor part
ways, and a sentence that has to name the kind asks `Condition::KindOf`.
Another kind would be the same again.
