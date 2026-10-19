-- The language files in publish/Translations, for the build and the release.
--
-- en.ini is the 1 source of English. Every build writes Core/Text/Keys.h from
-- it, 1 key per line and the English NEC.dll falls back to, and fails on a
-- broken en.ini. The other files are checked against it: a build warns about
-- each fault, so it passes while a new line waits for its translations, and
-- xmake release stops on any. src/Core/Text/Translations.cpp reads the files
-- in the game by the same rules, line by line.

-- The folder, from the project's root.
local FOLDER = "publish/Translations"

-- The 2 numbers [Layout] may set, and their bounds. Fit.cpp's own note says
-- why a grey row stops at 20 lines.
local LAYOUT = {
    linewidth = { name = "LineWidth", low = 10, high = 100 },
    rowlines = { name = "RowLines", low = 5, high = 20 },
}

-- Ends each English text in Keys.h, so no text may hold it.
local RAW_END = ")nec\""

local function trimmed(a_text)
    return (a_text:gsub("^[ \t]+", ""):gsub("[ \t]+$", ""))
end

-- The line of a_text that holds byte a_at.
local function line_at(a_text, a_at)
    local _, breaks = a_text:sub(1, a_at):gsub("\n", "")
    return breaks + 1
end

-- The byte where a_text stops being UTF-8, or nil when all of it is.
local function bad_utf8(a_text)
    local i = 1
    while i <= #a_text do
        local byte = a_text:byte(i)
        local size = byte < 0x80 and 1 or byte >= 0xC2 and byte <= 0xDF and 2 or byte >= 0xE0 and byte <= 0xEF and 3 or byte >= 0xF0 and byte <= 0xF4 and 4
        if not size then
            return i
        end
        for k = 1, size - 1 do
            local trail = a_text:byte(i + k)
            if not trail or trail < 0x80 or trail > 0xBF then
                return i
            end
        end
        -- No overlong forms, no UTF-16 halves, nothing past U+10FFFF.
        local second = a_text:byte(i + 1)
        if (byte == 0xE0 and second < 0xA0) or (byte == 0xED and second > 0x9F) or
            (byte == 0xF0 and second < 0x90) or (byte == 0xF4 and second > 0x8F) then
            return i
        end
        i = i + size
    end
end

-- The {n} slots of a_text as a set, or nil and why not. Only a bare {n}
-- counts, or {{ and }} for a brace, so std::vformat can always format it.
local function slots(a_text)
    local set = {}
    local i = 1
    while i <= #a_text do
        local char = a_text:sub(i, i)
        if char == "{" and a_text:sub(i + 1, i + 1) == "{" then
            i = i + 2
        elseif char == "{" then
            local number = a_text:match("^{(%d+)}", i)
            if number and (number == "0" or not number:find("^0")) then
                set[number] = true
                i = i + #number + 2
            else
                local spec = a_text:match("^{(%d+):[^}]*}", i)
                if spec then
                    return nil, string.format("holds %s, and only a bare {%s} works", a_text:match("^{%d+:[^}]*}", i), spec)
                end
                return nil, "holds a { that is no {0}, {1} or {2}"
            end
        elseif char == "}" and a_text:sub(i + 1, i + 1) == "}" then
            i = i + 2
        elseif char == "}" then
            return nil, "holds a } that closes no {0}, {1} or {2}"
        else
            i = i + 1
        end
    end
    return set
end

-- Why a_set differs from a_english, or nil when they hold the same slots.
local function slots_differ(a_set, a_english)
    for number in pairs(a_english) do
        if not a_set[number] then
            return string.format("lacks {%s}, which the English line has", number)
        end
    end
    for number in pairs(a_set) do
        if not a_english[number] then
            return string.format("holds {%s}, which the English line lacks", number)
        end
    end
end

-- A file as its KEY=text lines and its faults, each naming its line. A
-- [Layout] number is checked here and kept nowhere, since only the game reads
-- it.
local function parse(a_file)
    local name = FOLDER .. "/" .. path.filename(a_file)
    local text = io.readfile(a_file, { encoding = "binary" })
    local file = { name = name, lines = {}, faults = {}, refused = false }
    local function fault(a_number, a_why)
        table.insert(file.faults, a_number and string.format("%s line %d: %s", name, a_number, a_why) or string.format("%s %s", name, a_why))
    end
    if text:sub(1, 3) == "\239\187\191" or text:sub(1, 2) == "\255\254" or text:sub(1, 2) == "\254\255" then
        fault(nil, "starts with a byte order mark, and a shipped file is UTF-8 without one")
        file.refused = true
        return file
    end
    local bad = bad_utf8(text)
    if bad then
        fault(line_at(text, bad), "is no UTF-8")
        file.refused = true
        return file
    end
    local section = ""
    local number = 0
    for line in (text .. "\n"):gmatch("(.-)\r?\n") do
        number = number + 1
        line = trimmed(line)
        if line == "" or line:find("^[;#]") then
            -- A comment or a blank line.
        elseif line:find("^%[") then
            local heading = line:match("^%[([^%]]*)%]")
            if heading then
                section = trimmed(heading):lower()
            else
                fault(number, "is no KEY=text, [section] or ; comment")
            end
        else
            local key, value = line:match("^([^=]+)=(.*)$")
            if not key then
                fault(number, "is no KEY=text, [section] or ; comment")
            else
                key, value = trimmed(key), trimmed(value)
                local layout = section == "layout" and LAYOUT[key:lower()]
                if layout then
                    local count = tonumber(value:match("^%d+$") or "")
                    if not count or count < layout.low or count > layout.high then
                        fault(number, string.format("%s %s is no whole number from %d to %d", layout.name, value, layout.low, layout.high))
                    end
                else
                    table.insert(file.lines, { key = key, text = value, number = number })
                end
            end
        end
    end
    return file
end

-- A fault of a_line in a_file, naming its line and key.
local function line_fault(a_file, a_line, a_why)
    return string.format("%s line %d: %s %s", a_file.name, a_line.number, a_line.key, a_why)
end

-- The faults every file's lines share, a key given twice or no text, or nil.
-- Notes the first line of each key in a_seen.
local function shared_fault(a_line, a_seen)
    local before = a_seen[a_line.key]
    a_seen[a_line.key] = before or a_line.number
    if before then
        return string.format("comes again after line %d", before)
    elseif a_line.text == "" then
        return "has no text"
    end
end

-- en.ini as the list of its lines, each with its slots. Raises on anything
-- that would make a broken Keys.h or an English line the game cannot show.
local function english(a_folder)
    local file = parse(path.join(a_folder, "en.ini"))
    local seen = {}
    for _, line in ipairs(file.lines) do
        line.slots = {}
        local why
        if not line.key:match("^[A-Z][A-Z0-9_]*$") then
            why = "is no key, which is A to Z, 0 to 9 and _, starting with a letter"
        else
            why = shared_fault(line, seen)
        end
        if not why and line.text:find(RAW_END, 1, true) then
            why = "holds " .. RAW_END .. ", which ends a text in Keys.h"
        end
        if not why then
            local set
            set, why = slots(line.text)
            line.slots = set or line.slots
        end
        if why then
            table.insert(file.faults, line_fault(file, line, why))
        end
    end
    if #file.lines == 0 then
        table.insert(file.faults, file.name .. " has no lines")
    end
    if #file.faults > 0 then
        raise("en.ini is the English compiled into NEC.dll, so the build stops:\n  %s", table.concat(file.faults, "\n  "))
    end
    return file.lines
end

-- Every fault of the files beside en.ini, each against the English.
local function faults(a_folder, a_english)
    local out = {}
    local index = {}
    for _, line in ipairs(a_english) do
        index[line.key] = line
    end
    for _, file_path in ipairs(os.files(path.join(a_folder, "*.ini"))) do
        if path.filename(file_path) ~= "en.ini" then
            local file = parse(file_path)
            table.join2(out, file.faults)
            local seen = {}
            for _, line in ipairs(file.lines) do
                local own = index[line.key]
                local why
                if not own then
                    why = "is no key of en.ini"
                else
                    why = shared_fault(line, seen)
                end
                if not why then
                    local set
                    set, why = slots(line.text)
                    why = why or slots_differ(set, own.slots)
                end
                if why then
                    table.insert(out, line_fault(file, line, why))
                end
            end
            for _, line in ipairs(a_english) do
                if not seen[line.key] and not file.refused then
                    table.insert(out, string.format("%s lacks %s, line %d of en.ini", file.name, line.key, line.number))
                end
            end
        end
    end
    return out
end

-- The keys of en.ini that no file under src names, so a line nobody shows.
local function unused(a_english)
    local words = {}
    for _, file in ipairs(os.files(path.join(os.projectdir(), "src", "**"))) do
        for word in io.readfile(file):gmatch("[%w_]+") do
            words[word] = true
        end
    end
    local out = {}
    for _, line in ipairs(a_english) do
        if not words[line.key] then
            table.insert(out, string.format("%s/en.ini line %d: %s is named by no file under src", FOLDER, line.number, line.key))
        end
    end
    return out
end

-- Every warning is printed, where wprint shows only the first without -v.
local function warn(a_why)
    cprint("${color.warning}warning:${clear} %s", a_why)
end

-- Keys.h for a_english, the text only. Each key's comment is its English.
local function keys_header(a_english)
    local out = {
        "#pragma once",
        "",
        "// Written by xmake/texts.lua from publish/Translations/en.ini. Edit en.ini,",
        "// never this file. Included by Core/Text/Lines.h only.",
        "",
        "#include <cstddef>",
        "",
        "namespace Text",
        "{",
        string.format("\tinline constexpr std::size_t KEY_COUNT = %d;", #a_english),
    }
    for i, line in ipairs(a_english) do
        table.insert(out, "")
        -- A backslash at the end of a // comment would swallow the next line.
        table.insert(out, "\t// " .. (line.text:find("\\$") and line.text .. " (ends in a backslash)" or line.text))
        table.insert(out, string.format("\tinline constexpr Key %s{ %d };", line.key, i - 1))
    end
    table.insert(out, "")
    table.insert(out, "\tinline constexpr EnglishLine ENGLISH[KEY_COUNT]{")
    for _, line in ipairs(a_english) do
        table.insert(out, string.format("\t\t{ \"%s\", R\"nec(%s%s },", line.key, line.text, RAW_END))
    end
    table.insert(out, "\t};")
    table.insert(out, "}")
    return table.concat(out, "\n") .. "\n"
end

-- What every build runs first: Keys.h written into a_dir/Core/Text when en.ini
-- changed it, and a warning for each fault of the other files.
function build(a_dir)
    local folder = path.join(os.projectdir(), FOLDER)
    local lines = english(folder)
    local file = path.join(a_dir, "Core", "Text", "Keys.h")
    local text = keys_header(lines)
    -- Written only when it changes, so a build with no new text recompiles
    -- nothing.
    if not os.isfile(file) or io.readfile(file, { encoding = "binary" }) ~= text then
        io.writefile(file, text, { encoding = "binary" })
    end
    for _, why in ipairs(faults(folder, lines)) do
        warn(why)
    end
    for _, why in ipairs(unused(lines)) do
        warn(why)
    end
end

-- What xmake release runs first: it stops on any fault of a shipped file.
function check()
    local folder = path.join(os.projectdir(), FOLDER)
    local lines = english(folder)
    local found = faults(folder, lines)
    if #found > 0 then
        raise("the language files need fixing before a release:\n  %s", table.concat(found, "\n  "))
    end
    for _, why in ipairs(unused(lines)) do
        warn(why)
    end
end
