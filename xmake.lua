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

    -- The files in src/Core/Text hold strings in every language the game is
    -- sold in, so the sources are UTF-8. Without this MSVC reads them in the
    -- machine's code page and the accented letters come out wrong.
    add_cxflags("/utf-8", { tools = { "cl", "clang_cl" } })

    -- NEC.ini ships beside the DLL, kept in publish with the rest of what the
    -- archive carries. NEC_custom.ini is the player's own and is never
    -- written.
    add_installfiles("publish/NEC.ini", { prefixdir = "F4SE/Plugins" })

    after_build(function (target)
        local dir = get_config("deploy_dir")
        if not dir or dir == "" then
            return
        end
        local plugins = path.join(dir, "F4SE", "Plugins")
        os.mkdir(plugins)
        local ini = path.join(os.projectdir(), "publish", "NEC.ini")
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

-- Builds the plugin and packs build/NativeEquipmentCondition-<version>.zip for
-- Nexus Mods. The archive holds F4SE/Plugins with NEC.dll, NEC.pdb and NEC.ini,
-- and README.txt at the root, so Mod Organizer 2 installs it as it is. The PDB
-- lets a player's crash log name the file and line in NEC.dll.
--
--     xmake release
--
task("release")
    set_category("plugin")
    set_menu({ usage = "xmake release", description = "Build NEC and pack the Nexus Mods archive" })
    on_run(function ()
        import("core.base.task")
        import("core.project.config")
        import("core.project.project")
        import("utils.archive")

        config.load()
        if config.mode() ~= "releasedbg" then
            raise("release packs the optimized build, run xmake f -m releasedbg first")
        end
        task.run("build", { target = "NEC" })

        local stage = path.join(config.builddir(), "release")
        local plugins = path.join(stage, "F4SE", "Plugins")
        os.tryrm(stage)
        os.mkdir(plugins)
        local target = project.target("NEC")
        os.cp(target:targetfile(), plugins)
        os.cp(target:symbolfile(), plugins)
        os.cp(path.join(os.projectdir(), "publish", "NEC.ini"), plugins)
        os.cp(path.join(os.projectdir(), "publish", "README.txt"), stage)

        local file = path.absolute(path.join(config.builddir(), "NativeEquipmentCondition-" .. project.version() .. ".zip"))
        os.tryrm(file)
        -- A zip stores each file's time twice, once in local time and once in
        -- UTC, so the gap between the 2 gives away the packer's time zone. On
        -- Linux, TZ set to UTC makes both the same.
        os.setenv("TZ", "UTC")
        archive.archive(file, { "F4SE", "README.txt" }, { curdir = stage })
        cprint("${bright green}packed${clear} %s", file)
    end)
task_end()
