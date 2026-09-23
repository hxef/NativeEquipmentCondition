# Legacy

Nothing here is built, loaded or shipped. It is kept because it is the record of
how this mod used to work, and because some of it is worth reading before the
same ground is covered again natively.

The mod is now a single F4SE DLL. It ships no ESP, no Papyrus, no SWF and no
assets of any kind.

## What is here

### `papyrus/`

The 2020 to 2023 implementation, written entirely in Papyrus on top of a quest
script and a per weapon object reference script.

- `HxfItemDegradation/WeaponQuest.psc` is the heart of it. It polled the player,
  drove the HUDFramework widget and held the repair workbench logic.
- `HxfItemDegradation/HxfItemDegradation.psc` declares three native functions the
  old DLL exported, `GetEqWeapHealth`, `DegradeEqWeapCnd` and
  `WorkbenchMenuOpened`. The current DLL exports no Papyrus natives at all.
- `HxfItemDegradation/WeaponInstance_OLD` is the 2020 attempt that wrote
  degradation penalties straight into instance data. Note that it has no
  extension, so it was never compiled.
- `HxfItemDegradation/WeaponInstance.psc.bkp` is a later backup of the same idea.
- `HUDFramework.psc` is not ours. It is the HUDFramework API stub, signatures
  only, and its own header says it must not be compiled or redistributed.

Papyrus runs on a delay and cannot see a shot land, which is the reason the whole
approach was dropped. Everything it did is now done from native code in
`src/`.

### `esp/`

`HxfItemDegradation.esp`, which carried the condition attach point keyword, 21
condition OMODs, 11 repair recipes, a condition mod collection, a test template
weapon, an instance naming rules form and a patched load screen.

### `interface/`

The HUDFramework condition widget, as a compiled SWF plus its Flash sources. The
HUD readout is now drawn from native code into the game's own HUDMenu, so no SWF
ships any more.

### `scripts/`

The compiled `.pex` output of the Papyrus above, as it was deployed.

### `src/`

C++ that the move to a DLL only mod left with nothing to do.

- `Forms.h` looked every ESP form up at startup and injected the condition attach
  point, the condition mod collection and the naming rules into every playable
  weapon in the load order. All of it is ESP bound.
- `WorkbenchHooks.h` and `WorkbenchHooks.cpp` are the six ExamineMenu hooks that
  drove repair at a workbench. They showed and hid condition recipes according to
  how worn an item was, and blocked ordinary modification of a damaged item. Every
  one of them reads the ESP's OMODs and recipes.
- `EquippedItemData.h` is a superseded way of finding and splitting the equipped
  stack. It walked the inventory by hand and carried a hardcoded wear rate of two
  percent. The engine's own `FindAndWriteStackDataForInventoryItem` does the same
  work correctly, and `Degradation::WearEquipped` uses it. This copy was never
  called.
- `EquipWatcher.h` is an event sink that looked up the equipped form and then
  threw it away. It was registered and did nothing.

## Repair

There is no repair mechanic while this stands. The plan is to rebuild it natively,
without an ESP, at which point the shape of the old recipes in `papyrus/` and
`WorkbenchHooks.cpp` is the useful part of this folder.
