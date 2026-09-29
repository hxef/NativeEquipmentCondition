# How the plugin is put together

Native Equipment Condition is one DLL that gives Fallout 4 weapons and armor a
condition that wears down with use. It patches a few dozen places in the
game's own code, reads what the load order says about weapons, armor, recipes,
perks and leveled lists, and draws its readouts into the game's own menus. It
ships no ESP, no scripts and no assets, only the DLL and `NEC.ini` beside it.

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
    CallPatch.h              redirecting one call instruction
    Feature.h                the shape of a feature and its 4 moments
    ItemCards.h/.cpp         asking the Pip-Boy to rebuild its item cards
    Plugin.h                 the data handler the load order is read through
    Settings.h/.cpp          every setting, read from NEC.ini
    Text/
      Text.h/.cpp            the plugin's own sentences, in every language
      Lines.h                what the folder's files share
      Repair.cpp             the repair question and the bench's sentences
      Trader.cpp             what a trader says about a repair
      Tips.cpp               the loading screen's tips
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
    FireRate.h/.cpp          a worn automatic weapon fires slower
    BrokenEquip.h/.cpp       a broken item comes off and stays off until repaired
    ItemValue.h/.cpp         a worn item is worth less
    Jam.h/.cpp               a worn gun can jam
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
      SpawnCondition.h/.cpp  the roll at the door of every inventory
      Band.h/.cpp            the roll, either side of an item's middle or in a trader's band
      Guards.h/.cpp          what marks a console command, a save, a script's gift and a restock
    WeaponEvents/
      WeaponEvents.h/.cpp    the fire and reload calls, and the shot sink
      Hits.h/.cpp            the hit sink, and what a blow was

  UI/
    Flash.h/.cpp             reading and reaching into a menu movie's objects
    MenuMovies.h/.cpp        the one doorway into the game's menu movies
    MessageBox.h/.cpp        a question with as many answers as it is given
    LoadingTips.h/.cpp       the plugin's own tips on the loading screen
    InspectPrice.h/.cpp      the price on the inspect screen at a trader
    Hud/
      HudParts.h/.cpp        what the HUD readouts share
      HudCondition.h/.cpp    the CND bar in the ammo counter
      PowerArmorCondition/
        PowerArmorCondition.h/.cpp   the frame listener and the movie
        Dash.h/.cpp                  finding the dash through its own camera
        Layout.h/.cpp                laying the bar out in the movie
      QuickContainer/
        QuickContainer.h/.cpp        the feature: its install and the movie
        Rows.h/.cpp                  the rows as the game builds them
        Meters.h/.cpp                the meters on the HUD's rows
    Inventory/
      Pipboy.h/.cpp          a worn out item's name faded in the Pip-Boy
      PaperDoll.h/.cpp       a CND bar over each region of the apparel tab's paper doll
      ItemCard/
        ItemCard.h/.cpp      the feature: its install and the movies
        Cards.h              what the folder's files share
        Hooks.cpp            the CND row, and the fire rate a card prints
        Raise.cpp            moving the row up past Damage
    Repair/
      SelectedItem.h/.cpp    the item highlighted in a menu's list
      Restore.h/.cpp         writing a repair onto one copy of an item
      RepairPrompt.h/.cpp    asking how far to bring a worn item back
      ConfirmScroll.h/.cpp   the box the bench puts up, scrolled
      ConsoleRepair.h/.cpp   setting the weapon in hand or the armor worn from the console
      Workbench/             repairing at the weapon and armor benches
      VendorRepair/          paying a trader to repair weapons, armor or clothing, and their stock
```

A feature that has grown past one pair of files has a folder named after its
namespace: the feature's own header and source, a private header for what the
files share, and one file per job.

## How it runs

A feature is a namespace with up to 4 entry points, see `src/Core/Feature.h`,
and one row in `src/Features.cpp`. `src/main.cpp` walks the rows:

1. **Install**, once, while the plugin loads. Code is patched. No game data
   exists yet.
2. **Load**, every time game data has loaded: as the game starts, and again
   after every full reset, which is the game deleting every form and loading
   every file again after a prompt about changed DLC, Creations or a save's
   load order. Everything measured from the load order is measured here.
3. **Unload**, just before a full reset deletes every form, in reverse order,
   so a feature lets go before anything it read from does.
4. **OnMovieLoaded**, for every menu movie the game loads, through
   `src/UI/MenuMovies.cpp`.

A feature whose switch in `NEC.ini` is off is passed over at every step. The
switches are read as the game starts, and the Balance numbers again on every
save load, see `src/Core/Settings.h`.

Weapons and armor are the 2 kinds of item that wear. Each kind is a folder
under `src/Condition/`, one case in `Condition::WhyNoCondition`, a reader in
`src/Condition/Equipped.h`, and its own Gameplay features, each a row in
`src/Features.cpp`. The item cards, the Pip-Boy, the quick container and the
price read `Condition::Percent` and `Condition::WearsOut` and never ask what
kind of item it is. The repairs and the roll at the door ask it only where
weapons and armor part ways, and a sentence that has to name the kind asks
`Condition::KindOf`. Another kind would be the same again.
