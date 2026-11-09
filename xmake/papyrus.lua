-- NEC.pex, the compiled script NEC, for the build and the release.
--
-- The build writes NEC.psc into the generated folder from
-- publish/Scripts/Source/User/NEC.psc.in with the version from xmake.lua,
-- see add_configfiles there. Each build that changes it compiles it with the
-- Creation Kit's Papyrus compiler, which the game folder holds, then writes
-- NEC.pex with its 3 header stamps blank, since they would name this
-- machine's folder, user and computer, and the game prints the first in every
-- script error. A build that cannot compile it fails and leaves no NEC.pex,
-- so no release packs an old one.

import("lib.detect.find_program")
import("utils.archive")

-- The script's folder under the generated folder, and what it holds.
local FOLDER = "papyrus"
local COMPILED = "compiled"
local SCRIPTS = "Scripts"
local RECORD = "NEC.pex.from"

-- What a .pex starts with: 0xFA57C0DE, little endian.
local MAGIC = "\222\192\087\250"

-- The 3 stamps NEC.pex carries: the source's name, no user and no computer.
local STAMPS = { "NEC.psc", "", "" }

-- The rule that makes NEC a DLL points the config folder at the generated
-- folder only when a build configures it, which xmake release never does, so
-- the build and the release both look in the generated folder.
local function folder(a_target)
    return path.join(a_target:autogendir(), FOLDER)
end

-- The game folder: --game_dir, the one Steam's installer writes in
-- the registry, or Steam's own folders on Linux. nil when none holds the
-- compiler.
local function game_folder()
    local function holds(a_dir)
        return a_dir and os.isfile(path.join(a_dir, "Papyrus Compiler", "PapyrusCompiler.exe")) and a_dir or nil
    end
    local set = get_config("game_dir")
    if set and set ~= "" then
        return holds(set) or raise("--game_dir=%s holds no Papyrus Compiler/PapyrusCompiler.exe", set)
    end
    if is_host("windows") then
        return holds(try { function ()
            return winos.registry_query("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Fallout4;installed path")
        end })
    end
    local home = os.getenv("HOME")
    for _, steam in ipairs({ path.join(home, ".local", "share", "Steam"), path.join(home, ".steam", "steam") }) do
        local roots = { steam }
        local vdf = path.join(steam, "steamapps", "libraryfolders.vdf")
        if os.isfile(vdf) then
            for root in io.readfile(vdf):gmatch("\"path\"%s+\"([^\"]+)\"") do
                table.insert(roots, root)
            end
        end
        for _, root in ipairs(roots) do
            local found = holds(path.join(root, "steamapps", "common", "Fallout 4"))
            if found then
                return found
            end
        end
    end
end

-- F4SE's script sources, which NEC.psc needs for Form.GetName:
-- --f4se_scripts, the game's own Data/Scripts/Source where F4SE was
-- installed into the game, or F4SE's mod beside deploy_dir in Mod Organizer
-- 2.
local function f4se_folder(a_game)
    local function holds(a_dir)
        return os.isfile(path.join(a_dir, "F4SE.psc")) and a_dir or nil
    end
    local set = get_config("f4se_scripts")
    if set and set ~= "" then
        return holds(set) or raise("--f4se_scripts=%s holds no F4SE.psc", set)
    end
    local found = holds(path.join(a_game, "Data", "Scripts", "Source"))
    local deploy = get_config("deploy_dir")
    if not found and deploy and deploy ~= "" then
        for _, name in ipairs({ "Fallout 4 Script Extender", "Fallout 4 Script Extender (F4SE)" }) do
            found = found or holds(path.join(path.directory(deploy), name, "Scripts", "Source"))
        end
    end
    return found or raise("F4SE's script sources are not in %s, so set --f4se_scripts to the folder that holds F4SE.psc",
        path.join(a_game, "Data", "Scripts", "Source"))
end

-- The game's own script sources, unpacked once from the Creation Kit's
-- Base.zip into the generated folder, since the game folder is never written.
local function base_folder(a_target, a_game)
    local zip = path.join(a_game, "Data", "Scripts", "Source", "Base", "Base.zip")
    if not os.isfile(zip) then
        raise("%s is missing, so the Papyrus compiler has no game scripts to read", zip)
    end
    local dir = path.join(folder(a_target), "base")
    local stamp = path.join(dir, ".unpacked")
    local from = string.format("%d %d", os.filesize(zip), os.mtime(zip))
    if os.isfile(stamp) and io.readfile(stamp) == from then
        return dir
    end
    os.tryrm(dir)
    archive.extract(zip, dir)
    if not os.isfile(path.join(dir, "Institute_Papyrus_Flags.flg")) then
        raise("%s holds no Institute_Papyrus_Flags.flg", zip)
    end
    io.writefile(stamp, from)
    return dir
end

-- A folder as the compiler reads it: as it is on Windows, under wine on
-- drive Z:.
local function compiler_path(a_dir)
    local full = path.absolute(a_dir)
    if is_host("windows") then
        return full
    end
    return "Z:" .. (full:gsub("/", "\\"))
end

-- The compiler's output. It returns 0 when a compile fails too, so the
-- output is what tells.
local function compile(a_game, a_imports, a_into, a_curdir)
    local exe = path.join(a_game, "Papyrus Compiler", "PapyrusCompiler.exe")
    local paths = {}
    for _, dir in ipairs(a_imports) do
        table.insert(paths, compiler_path(dir))
    end
    local args = { "NEC.psc", "-f=Institute_Papyrus_Flags.flg", "-i=" .. table.concat(paths, ";"), "-o=" .. compiler_path(a_into) }
    local program = exe
    if not is_host("windows") then
        program = find_program("wine")
        if not program then
            raise("wine is missing, so the Papyrus compiler cannot run on this machine")
        end
        table.insert(args, 1, compiler_path(exe))
    end
    local out, err = try {
        function ()
            return os.iorunv(program, args, { curdir = a_curdir, envs = { WINEDEBUG = "-all" } })
        end,
        catch {
            function (a_error)
                raise("the Papyrus compiler did not run on this machine: %s", tostring(a_error))
            end
        }
    }
    return (out or "") .. (err or "")
end

local function u16(a_data, a_at)
    local low, high = a_data:byte(a_at, a_at + 1)
    return low + high * 256
end

local function u16_text(a_text)
    return string.char(#a_text % 256, math.floor(#a_text / 256)) .. a_text
end

-- A .pex's 3 stamps and where the rest of it starts. The header is 16 bytes,
-- then the stamps, each a length and its bytes.
local function stamps_of(a_data)
    if a_data:sub(1, 4) ~= MAGIC then
        return nil
    end
    local at = 17
    local stamps = {}
    for _ = 1, 3 do
        local length = u16(a_data, at)
        table.insert(stamps, a_data:sub(at + 2, at + 1 + length))
        at = at + 2 + length
    end
    return stamps, at
end

-- Every text in a .pex's string table, which follows the stamps.
local function strings_of(a_data, a_at)
    local found = {}
    local count = u16(a_data, a_at)
    local at = a_at + 2
    for _ = 1, count do
        local length = u16(a_data, at)
        found[a_data:sub(at + 2, at + 1 + length)] = true
        at = at + 2 + length
    end
    return found
end

-- The name of every function and struct NEC.psc declares.
local function names_in(a_psc)
    local names = {}
    for line in io.readfile(a_psc):gmatch("[^\r\n]+") do
        if not line:match("^%s*;") then
            local name = line:match("^%s*[%w_%[%]]*%s*Function%s+([%w_]+)%s*%(") or line:match("^%s*Struct%s+([%w_]+)")
            if name then
                table.insert(names, name)
            end
        end
    end
    return names
end

-- Writes a_pex from a_compiled with the blank stamps, and checks it: the
-- stamps read back, the rest is the compiled file byte for byte, and every
-- function and struct of a_psc is in its string table.
local function restamp(a_compiled, a_pex, a_psc)
    local compiled = io.readfile(a_compiled, { encoding = "binary" })
    local _, rest = stamps_of(compiled)
    if not rest then
        raise("%s is no .pex", a_compiled)
    end
    local head = compiled:sub(1, 16)
    for _, stamp in ipairs(STAMPS) do
        head = head .. u16_text(stamp)
    end
    io.writefile(a_pex, head .. compiled:sub(rest), { encoding = "binary" })

    local written = io.readfile(a_pex, { encoding = "binary" })
    local stamps, at = stamps_of(written)
    if not stamps or stamps[1] ~= STAMPS[1] or stamps[2] ~= STAMPS[2] or stamps[3] ~= STAMPS[3] or written:sub(at) ~= compiled:sub(rest) then
        os.tryrm(a_pex)
        raise("%s did not keep its stamps or its body", a_pex)
    end
    local strings = strings_of(written, at)
    for _, name in ipairs(names_in(a_psc)) do
        if not strings[name] then
            os.tryrm(a_pex)
            raise("%s lacks %s, which NEC.psc declares", a_pex, name)
        end
    end
end

-- Compiles NEC.pex when NEC.psc changed since the last one, and returns its
-- path. nil, with a line saying so, when no Papyrus compiler is found, and
-- then no NEC.pex is left from before either.
function build(a_target)
    local dir = folder(a_target)
    local psc = path.join(dir, "NEC.psc")
    local pex = path.join(dir, SCRIPTS, "NEC.pex")
    local record = path.join(dir, RECORD)
    if not os.isfile(psc) then
        raise("%s is missing, which the build writes from NEC.psc.in", psc)
    end

    local game = game_folder()
    if not game then
        os.tryrm(pex)
        cprint("${yellow}NEC.pex skipped${clear}: no Papyrus compiler found, set --game_dir to the Fallout 4 folder")
        return nil
    end
    local from = hash.sha256(psc)
    if os.isfile(pex) and os.isfile(record) and io.readfile(record) == from then
        return pex
    end

    os.tryrm(pex)
    os.tryrm(record)
    local compiled = path.join(dir, COMPILED)
    os.tryrm(path.join(compiled, "NEC.pex"))
    os.mkdir(compiled)
    os.mkdir(path.join(dir, SCRIPTS))
    -- The first folder that holds a script wins, so NEC.psc's own comes
    -- first, then F4SE's, whose ScriptObject.psc adds to the game's own.
    local imports = { dir, f4se_folder(game), base_folder(a_target, game) }
    local out = compile(game, imports, compiled, dir)
    if not out:find("1 succeeded, 0 failed", 1, true) or not os.isfile(path.join(compiled, "NEC.pex")) then
        raise("the Papyrus compiler did not compile NEC.psc:\n%s", out)
    end
    restamp(path.join(compiled, "NEC.pex"), pex, psc)
    io.writefile(record, from)
    cprint("${bright green}compiled${clear} %s", path.relative(pex, os.projectdir()))
    return pex
end

-- NEC.pex and NEC.psc of the last build, for xmake release. It stops when
-- NEC.pex is missing or was built from another NEC.psc.
function built(a_target)
    local dir = folder(a_target)
    local psc = path.join(dir, "NEC.psc")
    local pex = path.join(dir, SCRIPTS, "NEC.pex")
    local record = path.join(dir, RECORD)
    if not os.isfile(pex) or not os.isfile(psc) or not os.isfile(record) or io.readfile(record) ~= hash.sha256(psc) then
        raise("%s is missing or older than NEC.psc.in, run xmake with the Papyrus compiler found first",
            path.relative(pex, os.projectdir()))
    end
    return pex, psc
end
