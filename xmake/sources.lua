-- What the DLL was built from, for xmake release to tell whether the last
-- build is still current.
--
-- Every build writes NEC.sources beside NEC.dll: the commit it was built at,
-- the DLL's own hash, and the SHA-256 of each file the DLL is made from. The
-- release then stops only when a file's content differs, so a checkout or a
-- rebase that gives the same files new times leaves the build current. A DLL
-- with no record of its own, as from a build before this file, falls back to
-- file times.

-- The files the DLL is made from, from the project's root. en.ini counts,
-- since its English is compiled in.
local SOURCES = { "xmake.lua", "xmake/*.lua", "publish/Translations/en.ini", "src/**", "lib/commonlibf4/xmake.lua",
    "lib/commonlibf4/include/**", "lib/commonlibf4/src/**", "lib/commonlibf4/lib/**" }

-- The same files as paths for git, which takes no patterns.
local GIT_PATHS = { "xmake.lua", "xmake", "publish/Translations/en.ini", "src", "lib/commonlibf4" }

-- Each source as its path from the project's root with forward slashes, to
-- its SHA-256.
local function hashes()
    local root = os.projectdir()
    local found = {}
    for _, pattern in ipairs(SOURCES) do
        for _, file in ipairs(os.files(path.join(root, pattern))) do
            found[path.unix(path.relative(file, root))] = hash.sha256(file)
        end
    end
    return found
end

local function sorted_keys(a_map)
    local keys = table.keys(a_map)
    table.sort(keys)
    return keys
end

-- The commit HEAD is at, with "plus changes" when a source differs from it,
-- or "an unknown commit" where git is missing.
local function commit()
    return try {
        function ()
            local root = os.projectdir()
            local head = os.iorunv("git", { "-C", root, "rev-parse", "--short", "HEAD" }):trim()
            local args = { "-C", root, "status", "--porcelain", "--" }
            table.join2(args, GIT_PATHS)
            return os.iorunv("git", args):trim() == "" and head or head .. " plus changes"
        end
    } or "an unknown commit"
end

local function record_of(a_dll)
    return path.join(path.directory(a_dll), path.basename(a_dll) .. ".sources")
end

-- What every build runs last: the record of a_dll.
function write(a_dll)
    local found = hashes()
    local lines = { "commit " .. commit(), "dll " .. hash.sha256(a_dll) }
    for _, file in ipairs(sorted_keys(found)) do
        table.insert(lines, found[file] .. " " .. file)
    end
    io.writefile(record_of(a_dll), table.concat(lines, "\n") .. "\n")
end

-- What xmake release asks: nil when a_dll has no record of its own, else the
-- commit it was built at and each source that differs now, as "src/A.cpp
-- changed", "src/B.h new" or "src/C.h gone", in path order.
function compare(a_dll)
    local file = record_of(a_dll)
    if not os.isfile(file) then
        return nil
    end
    local built = { files = {} }
    for line in io.readfile(file):gmatch("[^\r\n]+") do
        local key, value = line:match("^(%S+) (.+)$")
        if key == "commit" or key == "dll" then
            built[key] = value
        elseif key then
            built.files[value] = key
        end
    end
    -- A record left by another DLL, as when the build that made this one
    -- failed to write its own, tells nothing about it.
    if built.dll ~= hash.sha256(a_dll) then
        return nil
    end

    local now = hashes()
    local differ = {}
    for _, name in ipairs(sorted_keys(now)) do
        if not built.files[name] then
            table.insert(differ, name .. " new")
        elseif built.files[name] ~= now[name] then
            table.insert(differ, name .. " changed")
        end
    end
    for _, name in ipairs(sorted_keys(built.files)) do
        if not now[name] then
            table.insert(differ, name .. " gone")
        end
    end
    table.sort(differ)
    return { commit = built.commit, differ = differ }
end

-- The first source whose file time is a_time or later, for a DLL with no
-- record.
function newer(a_time)
    local root = os.projectdir()
    for _, pattern in ipairs(SOURCES) do
        for _, file in ipairs(os.files(path.join(root, pattern))) do
            if os.mtime(file) >= a_time then
                return path.relative(file, root)
            end
        end
    end
end
