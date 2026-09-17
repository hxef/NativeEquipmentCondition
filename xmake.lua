-- include subprojects
includes("lib/commonlibf4")

-- A Linux host builds the Windows DLL through the contrib/linux-cross
-- submodule, which a Windows build never reads.
if is_host("linux") then
    includes("contrib/linux-cross")
end

-- set project constants
set_project("HxfItemDegradation")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- define targets
target("HxfItemDegradation")
    add_rules("commonlibf4.plugin", {
        name = "HxfItemDegradation",
        author = "hxef",
        description = "Item degradation for Fallout 4"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

-- Papyrus scripts.
--
-- Not built by default: compiling Papyrus needs PapyrusCompiler.exe and the base
-- game scripts, which ship with the Creation Kit rather than the game. Once the CK
-- is installed and XSE_FO4_GAME_PATH is set, build it explicitly:
--
--     xmake build Papyrus
--
target("Papyrus")
    set_default(false)
    add_rules("commonlibf4.papyrus")
    add_files("papyrus/HxfItemDegradation/*.psc")
    -- HUDFramework.psc is an import dependency, not one of our scripts.
    add_values("commonlib.papyrus.imports", "papyrus")
