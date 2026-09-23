-- include subprojects
includes("lib/commonlibf4")

-- A Linux host builds the Windows DLL through the contrib/linux-cross
-- submodule, which a Windows build never reads.
if is_host("linux") then
    includes("contrib/linux-cross")
end

-- set project constants
set_project("NativeEquipmentCondition")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- NEC.ini is read through CommonLibF4's INI support, off unless asked for.
set_config("commonlib_ini", true)

-- Copies the plugin into a folder after every build, for example a Mod
-- Organizer 2 mod. Off unless set, and remembered once configured:
--
--     xmake f --deploy_dir="C:/MO2/mods/Native Equipment Condition"
--
option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("Copy the plugin into <deploy_dir>/F4SE/Plugins after every build")
option_end()

-- define targets
-- The target name is the DLL name, so this builds NEC.dll.
target("NEC")
    add_rules("commonlibf4.plugin", {
        name = "Native Equipment Condition",
        author = "hxef",
        description = "Equipment condition for Fallout 4"
    })

    -- Every file under src, with includes written from src. legacy/ is never
    -- built.
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    -- src/Core/Text.cpp holds strings in every language the game is sold in, so
    -- the sources are UTF-8. Without this MSVC reads them in the machine's code
    -- page and the accented letters come out as rubbish.
    add_cxflags("/utf-8", { tools = { "cl", "clang_cl" } })

    -- NEC.ini ships beside the DLL. NEC_custom.ini is the player's own and is
    -- never written.
    add_installfiles("NEC.ini", { prefixdir = "F4SE/Plugins" })

    after_build(function (target)
        local dir = get_config("deploy_dir")
        if not dir or dir == "" then
            return
        end
        local plugins = path.join(dir, "F4SE", "Plugins")
        os.mkdir(plugins)
        local ini = path.join(os.projectdir(), "NEC.ini")
        for _, file in ipairs({ target:targetfile(), target:symbolfile(), ini }) do
            if file and os.isfile(file) then
                -- Written beside the old file and then renamed over it, so a game
                -- that still has the old DLL loaded keeps reading an intact copy.
                local dest = path.join(plugins, path.filename(file))
                os.cp(file, dest .. ".new")
                os.mv(dest .. ".new", dest)
            end
        end
        cprint("${bright green}deployed${clear} to %s", plugins)
    end)
