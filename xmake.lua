-- include subprojects
includes("lib/commonlibf4")

-- A Linux host builds the Windows DLL through the contrib/linux-cross
-- submodule, which a Windows build never reads.
if is_host("linux") then
    includes("contrib/linux-cross")
end

-- set project constants
set_project("NativeEquipmentCondition")
set_version("1.1.1")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- NEC.ini is read through CommonLibF4's INI support, off unless asked for.
set_config("commonlib_ini", true)

-- Copies the plugin into a folder after every build, for example a Mod
-- Organizer 2 mod, into F4SE/Plugins and MCM/Config/NEC. Off unless set, and
-- remembered once configured:
--
--     xmake f --deploy_dir="C:/MO2/mods/Native Equipment Condition"
--
option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("Copy the plugin into <deploy_dir>/F4SE/Plugins and <deploy_dir>/MCM/Config/NEC after every build")
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

    -- NEC.ini ships beside the DLL, and the MCM page's config.json in the
    -- folder MCM reads, both kept in publish with the rest of what the archive
    -- carries. NEC_custom.ini is the player's own, and NEC writes a key there
    -- only when the MCM page changes it.
    add_installfiles("publish/NEC.ini", { prefixdir = "F4SE/Plugins" })
    add_installfiles("publish/MCM/Config/NEC/config.json", { prefixdir = "MCM/Config/NEC" })

    after_build(function (target)
        local dir = get_config("deploy_dir")
        if not dir or dir == "" then
            return
        end
        local plugins = path.join(dir, "F4SE", "Plugins")
        local mcm = path.join(dir, "MCM", "Config", "NEC")
        local publish = path.join(os.projectdir(), "publish")
        local files = {
            { target:targetfile(), plugins },
            { target:symbolfile(), plugins },
            { path.join(publish, "NEC.ini"), plugins },
            { path.join(publish, "MCM", "Config", "NEC", "config.json"), mcm },
        }
        for _, entry in ipairs(files) do
            local file, into = entry[1], entry[2]
            if file and os.isfile(file) then
                -- Written beside the old file and then renamed over it, so a game
                -- that still has the old DLL loaded keeps reading an intact copy.
                os.mkdir(into)
                local dest = path.join(into, path.filename(file))
                os.cp(file, dest .. ".new")
                os.mv(dest .. ".new", dest)
            end
        end
        cprint("${bright green}deployed${clear} to %s", dir)
    end)

-- Builds the plugin and packs build/NativeEquipmentCondition-<version>.zip for
-- Nexus Mods. The archive holds F4SE/Plugins with NEC.dll, NEC.pdb and NEC.ini,
-- MCM/Config/NEC with the MCM page's config.json, and README.txt at the root,
-- so Mod Organizer 2 installs it as it is. The PDB lets a player's crash log
-- name the file and line in NEC.dll.
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
        local mcm = path.join(stage, "MCM", "Config", "NEC")
        os.tryrm(stage)
        os.mkdir(plugins)
        os.mkdir(mcm)
        local target = project.target("NEC")
        os.cp(target:targetfile(), plugins)
        os.cp(target:symbolfile(), plugins)
        os.cp(path.join(os.projectdir(), "publish", "NEC.ini"), plugins)
        os.cp(path.join(os.projectdir(), "publish", "MCM", "Config", "NEC", "config.json"), mcm)
        os.cp(path.join(os.projectdir(), "publish", "README.txt"), stage)

        local file = path.absolute(path.join(config.builddir(), "NativeEquipmentCondition-" .. project.version() .. ".zip"))
        os.tryrm(file)
        -- A zip stores each file's time twice, once in local time and once in
        -- UTC, so the gap between the 2 gives away the packer's time zone. On
        -- Linux, TZ set to UTC makes both the same.
        os.setenv("TZ", "UTC")
        archive.archive(file, { "F4SE", "MCM", "README.txt" }, { curdir = stage })
        cprint("${bright green}packed${clear} %s", file)
    end)
task_end()
