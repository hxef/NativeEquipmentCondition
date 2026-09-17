# HxfItemDegradation

An F4SE plugin that adds item degradation to Fallout 4, built on
[CommonLibF4](https://github.com/libxse/commonlibf4).

Weapons lose condition as they are used, tracked through condition OMODs attached to
the item and surfaced in the workbench and examine menus.

## Requirements

- [xmake](https://xmake.io) 3.0.0+
- A C++23 compiler (MSVC or Clang-CL)
- Fallout 4 **1.11.240** and a matching build of
  [F4SE](https://f4se.silverlock.org/)

## Building

```bat
git clone --recurse-submodules https://github.com/hxef/HxfItemDegradation
cd HxfItemDegradation
xmake build
```

The output lands in `build/windows/x64/`.

If you already cloned without `--recurse-submodules`:

```bat
git submodule update --init --recursive
```

### Build output

To have the build copied into the game or a mod manager, set one of:

- `XSE_FO4_MODS_PATH`, a mod manager's mods folder, or
- `XSE_FO4_GAME_PATH`, the Fallout 4 install folder

### Visual Studio

```bat
xmake project -k vsxmake
```

This generates a `vsxmakeXXXX/` directory using the newest installed Visual Studio.

### Other editors (clangd)

```bat
xmake project -k compile_commands
```

This writes `compile_commands.json` into the project root, where clangd finds it
without further configuration. Regenerate it whenever the build configuration
changes, and note that until it exists clangd has no include paths at all and
every `RE::` name will appear undefined.

The committed `.clangd` drops the `/Yu` flag, which points at a precompiled
header only a real build produces. The `/FI` force-include of `src/pch.h` is
kept, and that is what lets the headers here resolve `RE` and `F4SE` without
including anything themselves.

## Building from Linux

The plugin is a Windows DLL wherever it is built, and a Linux host cross compiles
it. There `xmake.lua` includes `contrib/linux-cross`, a submodule holding
[commonlibf4-linux-cross](https://github.com/hxef/commonlibf4-linux-cross), so a
Linux build needs it checked out. The recursive clone above does that, and its
README says what else to install. Then configure once and build:

```sh
xmake f -p windows -a x64 -m releasedbg --toolchain=xwin-clang-cl
xmake build
```

Repeat those flags whenever you run `xmake f` again. Without them xmake
configures for Linux instead.

## Papyrus scripts

The `papyrus/` scripts are **not** built by default. Compiling them needs
`PapyrusCompiler.exe` and the base game scripts, which ship with the Creation Kit
rather than the game itself. With the CK installed and `XSE_FO4_GAME_PATH` set:

```bat
xmake build Papyrus
```

## License

[GPL-3.0](LICENSE) with the [modding exceptions](EXCEPTIONS) CommonLibF4 carries.
