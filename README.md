# Native Equipment Condition

An F4SE plugin that gives Fallout 4 weapons and armor a condition that wears
down with use, built on [CommonLibF4](https://github.com/libxse/commonlibf4).
It is one DLL, `NEC.dll`, and its settings file, `NEC.ini`, which says what
every setting does. Put your own changes in `NEC_custom.ini` beside it.

### Requirements
* [XMake](https://xmake.io) [3.0.0+]
* C++23 Compiler (MSVC or Clang-CL)
* Fallout 4 1.11.240 and a matching [F4SE](https://f4se.silverlock.org/)

## Getting Started
```bat
git clone --recurse-submodules https://github.com/hxef/NativeEquipmentCondition
cd NativeEquipmentCondition
```

### Build
To build the project, run the following command:
```bat
xmake build
```

> ***Note:*** *This will generate a `build/windows/` directory in the **project's root directory** with the build output.*

### Build Output (Optional)
If you want to redirect the build output, set one of the following environment variables:
- Path to a Mod Manager mods folder: `XSE_FO4_MODS_PATH`
or
- Path to a Fallout 4 install folder: `XSE_FO4_GAME_PATH`

To copy the DLL, the PDB and `NEC.ini` into a mod folder after every build instead, point `deploy_dir` at it once:
```bat
xmake f --deploy_dir="C:/MO2/mods/Native Equipment Condition"
```

### Project Generation (Optional)
If you use Visual Studio, run the following command:
```bat
xmake project -k vsxmake
```

> ***Note:*** *This will generate a `vsxmakeXXXX/` directory in the **project's root directory** using the latest version of Visual Studio installed on the system.*

**Alternatively**, if you do not use Visual Studio, you can generate a `compile_commands.json` file for use with a language server like clangd in any code editor that supports it, like vscode:
```bat
xmake project -k compile_commands
```

> ***Note:*** *You must have a language server extension installed to make use of this file. I recommend `clangd`. Do not have more than one installed at a time as they will conflict with each other. I also recommend installing the `xmake` extension if available to make building the project easier.*

### Upgrading Packages (Optional)
If you want to upgrade the project's dependencies, run the following commands:
```bat
xmake repo --update
xmake require --upgrade
```

### Building from Linux (Optional)
A Linux host cross compiles the DLL through the `contrib/linux-cross` submodule, see [its README](contrib/linux-cross/README.md) for the setup:
```sh
xmake f -p windows -a x64 -m releasedbg --toolchain=xwin-clang-cl
xmake build
```

## Documentation
[ARCHITECTURE.md](ARCHITECTURE.md) is the layout. Every header says what the game does there and what the plugin does about it. `legacy/` holds the Papyrus scripts, the ESP and the sources that need them, kept for reference and never built.

## License
[GPL-3.0](LICENSE) with the [modding exceptions](EXCEPTIONS) CommonLibF4 carries.
