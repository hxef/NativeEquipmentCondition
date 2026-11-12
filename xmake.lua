-- include subprojects
includes("lib/commonlibf4")

-- A Linux host builds the Windows DLL through the contrib/linux-cross
-- submodule, which a Windows build never reads.
if is_host("linux") then
    includes("contrib/linux-cross")
end

-- set project constants
set_project("NativeEquipmentCondition")
set_version("1.9.1")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- NEC.ini is read through CommonLibF4's INI support, off unless asked for.
set_config("commonlib_ini", true)

-- Copies the plugin into a folder after every build, for example a Mod
-- Organizer 2 mod, into F4SE/Plugins, F4SE/Plugins/NEC/Translations and
-- MCM/Config/NEC. Off unless set, and remembered once configured:
--
--     xmake f --deploy_dir="C:/MO2/mods/Native Equipment Condition"
--
option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("Copy the plugin into <deploy_dir>/F4SE/Plugins and <deploy_dir>/MCM/Config/NEC after every build")
option_end()

-- Every build that changes NEC.psc compiles NEC.pex with the Creation Kit's
-- Papyrus compiler, under wine on Linux, see xmake/papyrus.lua. The game
-- folder and F4SE's script sources are found where Steam and F4SE put them,
-- or where these say:
--
--     xmake f --game_dir="C:/Steam/steamapps/common/Fallout 4" --f4se_scripts="C:/MO2/mods/F4SE/Scripts/Source"
--
option("game_dir")
    set_default("")
    set_showmenu(true)
    set_description("The Fallout 4 folder that holds Papyrus Compiler/PapyrusCompiler.exe and Data/Scripts/Source/Base/Base.zip")
option_end()

option("f4se_scripts")
    set_default("")
    set_showmenu(true)
    set_description("The folder that holds F4SE's F4SE.psc and ScriptObject.psc")
option_end()

-- define targets
-- The target name is the DLL name, so this builds NEC.dll.
target("NEC")
    -- old_gen adds the F4SEPlugin_Query export F4SE 0.6.x asks on 1.10.163.
    add_rules("commonlibf4.plugin", {
        name = "Native Equipment Condition",
        author = "hxef",
        description = "Equipment condition for Fallout 4",
        options = { old_gen = true }
    })

    -- Every file under src, with includes written from src. legacy/ is never
    -- built.
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

    -- The sources are UTF-8, and so is the English compiled in from en.ini.
    -- Without this MSVC reads them in the machine's code page and any letter
    -- past ASCII comes out wrong.
    add_cxflags("/utf-8", { tools = { "cl", "clang_cl" } })

    -- Core/Text/Keys.h and NEC.psc are written into the generated folder
    -- first, see xmake/texts.lua. NEC.psc gets the version as text, such as
    -- 1.9.1, and as 1 number, such as 10901, in its comments and in what
    -- GetScriptVersion returns. xmake/version.lua makes both, for xmake
    -- apidoc too.
    add_configfiles("publish/Scripts/Source/User/NEC.psc.in", { prefixdir = "papyrus" })
    on_config(function (target)
        target:add("includedirs", target:autogendir())
        import("version", { rootdir = path.join(os.projectdir(), "xmake") })
        for name, value in pairs(version.configvars(target:version())) do
            target:set("configvar", name, value)
        end
    end)
    before_build(function (target)
        import("texts", { rootdir = path.join(os.projectdir(), "xmake") })
        texts.build(target:autogendir())
    end)

    -- NEC.ini and NEC_Papyrus_API.txt ship beside the DLL, the language files
    -- in NEC/Translations beside them, the vanilla fridge list in NEC/Fridge,
    -- an example file in NEC/Exclude, NEC/Food, NEC/Fridge and NEC/Traders,
    -- and the MCM page's config.json in the folder MCM reads, all kept in
    -- publish with the rest of what the archive carries.
    -- NEC_custom.ini is the player's own, and NEC writes a key there only when
    -- the MCM page changes it.
    add_installfiles("publish/NEC.ini", { prefixdir = "F4SE/Plugins" })
    add_installfiles("publish/NEC_Papyrus_API.txt", { prefixdir = "F4SE/Plugins" })
    add_installfiles("publish/Translations/*.ini", { prefixdir = "F4SE/Plugins/NEC/Translations" })
    add_installfiles("publish/Exclude/*.ini", { prefixdir = "F4SE/Plugins/NEC/Exclude" })
    add_installfiles("publish/Food/*.ini", { prefixdir = "F4SE/Plugins/NEC/Food" })
    add_installfiles("publish/Fridge/*.ini", { prefixdir = "F4SE/Plugins/NEC/Fridge" })
    add_installfiles("publish/Traders/*.ini", { prefixdir = "F4SE/Plugins/NEC/Traders" })
    add_installfiles("publish/MCM/Config/NEC/config.json", { prefixdir = "MCM/Config/NEC" })

    -- Records what the DLL was built from for xmake release, see
    -- xmake/sources.lua, compiles NEC.pex, see xmake/papyrus.lua, then
    -- deploys when deploy_dir is set.
    after_build(function (target)
        import("sources", { rootdir = path.join(os.projectdir(), "xmake") })
        sources.write(target:targetfile())
        import("papyrus", { rootdir = path.join(os.projectdir(), "xmake") })
        local pex = papyrus.build(target)

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
            { path.join(publish, "NEC_Papyrus_API.txt"), plugins },
            { path.join(publish, "MCM", "Config", "NEC", "config.json"), mcm },
        }
        for _, folder in ipairs({ "Translations", "Exclude", "Food", "Fridge", "Traders" }) do
            for _, file in ipairs(os.files(path.join(publish, folder, "*.ini"))) do
                table.insert(files, { file, path.join(plugins, "NEC", folder) })
            end
        end
        if pex then
            table.insert(files, { pex, path.join(dir, "Scripts") })
        end
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

-- Packs build/NativeEquipmentCondition-<version>.zip, for any site, from the
-- last build. The archive holds F4SE/Plugins with NEC.dll, NEC.pdb, NEC.ini and
-- NEC_Papyrus_API.txt, F4SE/Plugins/NEC/Translations with the 12 language files,
-- F4SE/Plugins/NEC/Fridge with the vanilla fridge list, an example file in
-- F4SE/Plugins/NEC/Exclude, NEC/Food, NEC/Fridge and NEC/Traders,
-- MCM/Config/NEC with the MCM page's config.json, Scripts with NEC.pex,
-- Scripts/Source/User with NEC.psc, and README.txt at the root, so Mod
-- Organizer 2 installs it as it is. The PDB lets a player's crash log name the
-- file and line in NEC.dll.
-- It stops before packing on any fault of a language file, which a plain build
-- only warns about, and when the API document differs from what xmake apidoc
-- writes.
--
-- It never builds. It ships the DLL, PDB and NEC.pex the last build made, byte
-- for byte, and stops when they are missing or a file the DLL is made from
-- differs from the one the build read, naming each such file, so build first.
-- A DLL built before xmake/sources.lua existed is judged by file times
-- instead. --lastbuild packs them anyway, for a change that never reaches the
-- DLL, like a comment or this task.
--
-- With --nexus it packs build/nexusmods/NativeEquipmentCondition-<version>.zip
-- instead, without README.txt, since Nexus Mods shows the readme in a section
-- of its own. It also checks this version's file description and changelog in
-- publish/nexusmods and prints both, ready to paste into the upload form.
-- --outputdir writes the zip into another folder, so a second pack leaves the
-- one in build alone.
--
--     xmake release
--     xmake release --nexus
--     xmake release --nexus --outputdir=../spare
--
task("release")
    set_category("plugin")
    set_menu({
        usage = "xmake release [--nexus] [--lastbuild] [--outputdir=DIR]",
        description = "Pack the release archive from the last build",
        options = {
            { nil, "nexus", "k", nil, "Pack it for Nexus Mods, without README.txt, and check the texts in publish/nexusmods" },
            { nil, "lastbuild", "k", nil, "Pack the last build even when a source changed after it" },
            { nil, "outputdir", "kv", nil, "Write the zip into this folder instead of build or build/nexusmods" },
        },
    })
    on_run(function ()
        import("core.base.option")
        import("core.project.config")
        import("core.project.project")
        import("utils.archive")

        config.load()
        if config.mode() ~= "releasedbg" then
            raise("release packs the optimized build, run xmake f -m releasedbg first")
        end

        -- The lines under the line that holds only a_version, up to the next
        -- version line, without the blank lines around them.
        local function block(a_file, a_version)
            local lines = {}
            local inside = false
            for line in (io.readfile(a_file) .. "\n"):gmatch("(.-)\r?\n") do
                if line:match("^%d+%.%d+%.%d+$") then
                    if inside then
                        break
                    end
                    inside = line == a_version
                elseif inside then
                    table.insert(lines, line)
                end
            end
            return (table.concat(lines, "\n"):gsub("^%s+", ""):gsub("%s+$", ""))
        end

        local version = project.version()
        local nexus = option.get("nexus")
        local description, changelog
        if nexus then
            -- Checked first, so a text to fix stops the release early. Nexus
            -- Mods takes at most 255 characters for a file's description,
            -- and every text here is plain ASCII, 1 byte each. The
            -- description is 1 sentence from "v<version>", a blank line and
            -- "OG/AE only", the newlines counted.
            local texts = path.join(os.projectdir(), "publish", "nexusmods")
            description = block(path.join(texts, "file_descriptions.txt"), version)
            changelog = block(path.join(texts, "changelog.txt"), version)
            if description:sub(1, #("v" .. version)) ~= "v" .. version then
                raise("publish/nexusmods/file_descriptions.txt needs a %s block that starts \"v%s\"", version, version)
            end
            if description:sub(-#"\n\nOG/AE only") ~= "\n\nOG/AE only" then
                raise("the %s file description ends in a blank line and \"OG/AE only\"", version)
            end
            if #description > 255 then
                raise("the %s file description is %d characters, and Nexus Mods takes 255", version, #description)
            end
            if changelog == "" then
                raise("publish/nexusmods/changelog.txt needs a %s block", version)
            end
        end

        import("texts", { rootdir = path.join(os.projectdir(), "xmake") })
        texts.check()
        import("apidoc", { rootdir = path.join(os.projectdir(), "xmake") })
        apidoc.check()

        -- The rule that makes NEC a DLL sets its kind only when a build
        -- configures it, so here target:targetfile() still reads NEC.exe.
        -- The last build's files are found by path in the target's folder.
        local target = project.target("NEC")
        local dll = path.join(target:targetdir(), target:basename() .. ".dll")
        local pdb = path.join(target:targetdir(), target:basename() .. ".pdb")

        -- A build run from inside this task fails, since the task has loaded
        -- the project already, so it asks for one instead.
        if not os.isfile(dll) or not os.isfile(pdb) then
            raise("%s and its PDB are missing, run xmake to build NEC first", path.relative(dll, os.projectdir()))
        end
        import("papyrus", { rootdir = path.join(os.projectdir(), "xmake") })
        local pex, psc = papyrus.built(target)

        -- What changed since the last build, or nil when nothing did.
        import("sources", { rootdir = path.join(os.projectdir(), "xmake") })
        local built = sources.compare(dll)
        local stale
        if built and #built.differ > 0 then
            local shown = table.slice(built.differ, 1, 10)
            local more = #built.differ - #shown
            stale = string.format("the sources differ from the last build's at %s: %s%s", built.commit, table.concat(shown, ", "),
                more > 0 and string.format(" and %d more", more) or "")
        elseif not built then
            local newer = sources.newer(math.min(os.mtime(dll), os.mtime(pdb)))
            if newer then
                stale = newer .. " is newer than the last build, which has no record of its sources"
            end
        end
        if stale and not option.get("lastbuild") then
            raise("%s. Run xmake to build NEC first, or pass --lastbuild when the change never reaches the DLL", stale)
        end
        if stale then
            cprint("packing the DLL and PDB of the last build, although %s", stale)
        elseif built then
            cprint("packing the DLL and PDB of the last build at %s, since every source is as it was built", built.commit)
        else
            cprint("packing the DLL and PDB of the last build, since no source changed after it")
        end

        -- Staged in a fresh temporary folder, so nothing under build changes
        -- but the zip.
        local stage = os.tmpfile() .. ".dir"
        local plugins = path.join(stage, "F4SE", "Plugins")
        local mcm = path.join(stage, "MCM", "Config", "NEC")
        os.mkdir(plugins)
        os.mkdir(mcm)
        os.cp(dll, plugins)
        os.cp(pdb, plugins)
        os.cp(path.join(os.projectdir(), "publish", "NEC.ini"), plugins)
        os.cp(path.join(os.projectdir(), "publish", "NEC_Papyrus_API.txt"), plugins)
        os.mkdir(path.join(plugins, "NEC", "Translations"))
        os.cp(path.join(os.projectdir(), "publish", "Translations", "*.ini"), path.join(plugins, "NEC", "Translations"))
        for _, folder in ipairs({ "Exclude", "Food", "Fridge", "Traders" }) do
            os.mkdir(path.join(plugins, "NEC", folder))
            os.cp(path.join(os.projectdir(), "publish", folder, "*.ini"), path.join(plugins, "NEC", folder))
        end
        os.cp(path.join(os.projectdir(), "publish", "MCM", "Config", "NEC", "config.json"), mcm)
        os.mkdir(path.join(stage, "Scripts", "Source", "User"))
        os.cp(pex, path.join(stage, "Scripts"))
        os.cp(psc, path.join(stage, "Scripts", "Source", "User"))
        local packed = { "F4SE", "MCM", "Scripts" }
        local into = option.get("outputdir")
        if not into then
            into = nexus and path.join(config.builddir(), "nexusmods") or config.builddir()
        end
        if not nexus then
            os.cp(path.join(os.projectdir(), "publish", "README.txt"), stage)
            table.insert(packed, "README.txt")
        end

        local file = path.absolute(path.join(into, "NativeEquipmentCondition-" .. version .. ".zip"))
        os.mkdir(into)
        os.tryrm(file)
        -- A zip stores each file's time twice, once in local time and once in
        -- UTC, so the gap between the 2 gives away the packer's time zone. On
        -- Linux, TZ set to UTC makes both the same.
        os.setenv("TZ", "UTC")
        archive.archive(file, packed, { curdir = stage })
        cprint("${bright green}packed${clear} %s", file)

        if nexus then
            cprint("${bright}File description${clear}, %d of 255 characters:\n%s\n", #description, description)
            cprint("${bright}Changelog${clear}:\n%s", changelog)
        end
    end)
task_end()

-- Writes the Papyrus API's document from the comments of NEC.psc.in into
-- publish/NEC_Papyrus_API.txt and publish/nexusmods/article_api.bbcode, see
-- xmake/apidoc.lua. Commit both with the change to NEC.psc.in or to the
-- version, since xmake release stops when either differs from what this
-- writes.
--
--     xmake apidoc
--
task("apidoc")
    set_category("plugin")
    set_menu({
        usage = "xmake apidoc",
        description = "Write the Papyrus API's document from NEC.psc.in",
        options = {},
    })
    on_run(function ()
        import("apidoc", { rootdir = path.join(os.projectdir(), "xmake") })
        apidoc.write()
    end)
task_end()
