-- The Papyrus API's document, for xmake apidoc and xmake release.
--
-- The comments of NEC.psc.in are its 1 source. It is written twice with the
-- same words in the same order: NEC_Papyrus_API.txt, which ships beside
-- NEC.ini, and the Nexus article, which adds only markup. xmake apidoc writes
-- both, and xmake release stops when a committed copy differs from what this
-- writes, so neither drifts from the script.
--
-- How the comments are read, each line from its "; ":
--
--   # Title         a section, the lines under it its text
--   ## Title        a group of functions, the lines right under it its text
--   (nothing)       a line of only ";" ends a paragraph
--   1. Text         a numbered list, a line indented deeper continues it
--     Code          2 more spaces, kept as written
--   Event NAME      an event, the next line its callback, the rest its text
--
-- The lines right above a Function or a Struct are its 1 line, and a
-- function with no line above it stops the document. A function with a body
-- is listed by its signature alone.

-- The files, from the project's root.
local SOURCE = "publish/Scripts/Source/User/NEC.psc.in"
local FEATURES = "src/Features.cpp"
local TEXT = "publish/NEC_Papyrus_API.txt"
local ARTICLE = "publish/nexusmods/article_api.bbcode"

local TITLE = "NEC Papyrus API"

-- The section whose text ends with the names IsFeatureOn takes.
local FEATURE_SECTION = "Feature names"

-- The plain text's width, and its indents.
local WIDTH = 100
local INDENT = "    "
local SIGNATURE_INDENT = "        "

-- A section title as the Nexus page draws its own.
local HEADING = "[size=3][b][font=Georgia][color=#1AFF80]%s[/color][/font][/b][/size]"
local MONO = "[font=Courier New]%s[/font]"

-- The lines of a_file without their ends, as Git on Windows may check them
-- out with CRLF.
local function lines_of(a_file)
    local lines = {}
    for line in (io.readfile(a_file, { encoding = "binary" }):gsub("\r", "") .. "\n"):gmatch("(.-)\n") do
        table.insert(lines, line)
    end
    return lines
end

local function fail(a_number, a_why, ...)
    raise("%s(%d): " .. a_why, SOURCE, a_number, ...)
end

-- The text of a comment line, "" for a line of only ";", nil for any other.
local function comment_of(a_line)
    if a_line == ";" then
        return ""
    end
    return a_line:match("^; (.*)$")
end

-- A section's lines as paragraphs, lists, code and events.
local function items_of(a_texts, a_numbers)
    local items = {}
    local open
    local blanks = 0
    for k, text in ipairs(a_texts) do
        local number = a_numbers[k]
        local list_item = text:match("^%d+%.%s+.*$")
        local event = text:match("^Event%s+(%S+)$")
        if text == "" then
            if open and open.kind == "code" then
                blanks = blanks + 1
            else
                open = nil
            end
        elseif open and open.kind == "event" and not open.signature then
            open.signature = text:match("^Function%s.*$") or fail(number, "the line under Event %s is not its callback", open.name)
        elseif event then
            open = { kind = "event", name = event, text = {} }
            table.insert(items, open)
        elseif open and open.kind == "event" then
            table.insert(open.text, text)
        elseif list_item then
            if not (open and open.kind == "list") then
                open = { kind = "list", entries = {} }
                table.insert(items, open)
            end
            table.insert(open.entries, { text })
        elseif open and open.kind == "list" and text:match("^%s") then
            table.insert(open.entries[#open.entries], (text:gsub("^%s+", "")))
        elseif text:match("^  ") then
            if open and open.kind == "code" then
                for _ = 1, blanks do
                    table.insert(open.lines, "")
                end
            else
                open = { kind = "code", lines = {} }
                table.insert(items, open)
            end
            blanks = 0
            table.insert(open.lines, text:sub(3))
        elseif text:match("^%s") then
            fail(number, "an indented line that is no code, 2 spaces deeper, and continues no list")
        else
            if not (open and open.kind == "para") then
                open = { kind = "para", words = {} }
                table.insert(items, open)
            end
            table.insert(open.words, text)
        end
        if text ~= "" then
            blanks = 0
        end
    end
    for _, item in ipairs(items) do
        if item.kind == "event" and not item.signature then
            raise("%s: the event %s has no callback line", SOURCE, item.name)
        end
    end
    return items
end

local function stripped(a_signature)
    local before
    repeat
        before = a_signature
        a_signature = a_signature:gsub("%s+native%s*$", ""):gsub("%s+global%s*$", "")
    until a_signature == before
    return (a_signature:gsub("^%s+", ""))
end

-- NEC.psc.in as its lead line and its sections and groups in order.
local function parse(a_file)
    local lines = lines_of(a_file)
    local doc = { blocks = {} }
    local current
    -- The comment lines read since the last code line, with their numbers.
    local texts, numbers = {}, {}

    local function place(a_number)
        if #texts == 0 then
            return
        end
        if not current then
            fail(numbers[1], "a comment before the first # section")
        end
        if current.kind == "group" then
            fail(numbers[1], "a comment in the group %s that is above no function", current.title)
        end
        for k, text in ipairs(texts) do
            table.insert(current.texts, text)
            table.insert(current.numbers, numbers[k])
        end
        texts, numbers = {}, {}
    end

    local function entry_line(a_number)
        if #texts == 0 then
            fail(a_number, "this has no comment line above it")
        end
        for k, text in ipairs(texts) do
            if text == "" or text:match("^%s") then
                fail(numbers[k], "a function's comment is 1 line of text")
            end
        end
        local line = table.concat(texts, " ")
        texts, numbers = {}, {}
        return line
    end

    local i = 1
    while i <= #lines do
        local line = lines[i]
        local text = comment_of(line)
        if text then
            local section = text:match("^# (.+)$")
            local group = text:match("^## (.+)$")
            if section or group then
                place(i)
                current = { kind = section and "section" or "group", title = section or group, texts = {}, numbers = {}, entries = {} }
                table.insert(doc.blocks, current)
                -- A group's text is the comment right under its title.
                if group then
                    i = i + 1
                    while lines[i] and comment_of(lines[i]) and comment_of(lines[i]) ~= "" do
                        table.insert(current.texts, comment_of(lines[i]))
                        table.insert(current.numbers, i)
                        i = i + 1
                    end
                    i = i - 1
                end
            else
                table.insert(texts, text)
                table.insert(numbers, i)
            end
        elseif line:match("^%s*$") then
            -- A blank line ends a comment unless a function follows it.
            if current and current.kind == "section" then
                place(i)
            elseif #texts > 0 then
                fail(numbers[1], "a comment in the group %s that is above no function", current and current.title or "")
            end
        elseif line:match("^Scriptname%s") then
            -- The script's own line, which the document leaves out.
        elseif line:match("^{.*}$") and not doc.lead then
            doc.lead = line:sub(2, -2)
        elseif line:match("^[%w_%[%]]*%s*Function%s+[%w_]+%s*%(") then
            if not current or current.kind ~= "group" then
                fail(i, "a function outside any ## group")
            end
            table.insert(current.entries, { signature = { stripped(line) }, line = entry_line(i) })
            if not line:match("%snative%s*$") then
                repeat
                    i = i + 1
                until not lines[i] or lines[i]:match("^EndFunction%s*$")
                if not lines[i] then
                    fail(i - 1, "a function with no EndFunction")
                end
            end
        elseif line:match("^Struct%s+[%w_]+%s*$") then
            if not current or current.kind ~= "group" then
                fail(i, "a struct outside any ## group")
            end
            local entry = { signature = { line }, line = entry_line(i), struct = true }
            repeat
                i = i + 1
                if not lines[i] then
                    fail(i - 1, "a struct with no EndStruct")
                end
                table.insert(entry.signature, lines[i])
            until lines[i]:match("^EndStruct%s*$")
            table.insert(current.entries, entry)
        else
            fail(i, "a line the document does not know how to show")
        end
        i = i + 1
    end
    place(#lines)
    if not doc.lead then
        raise("%s has no {...} line under its Scriptname", SOURCE)
    end

    for _, block in ipairs(doc.blocks) do
        block.items = items_of(block.texts, block.numbers)
        if block.kind == "group" then
            for _, item in ipairs(block.items) do
                if item.kind ~= "para" then
                    raise("%s: the group %s has a text that is no paragraph", SOURCE, block.title)
                end
            end
        end
    end
    return doc
end

-- The names IsFeatureOn takes: the rows of Features.cpp with a part, which
-- another DLL can take over. Added as the last paragraph of the Feature
-- names section.
local function add_features(a_doc, a_file)
    local names = {}
    for _, line in ipairs(lines_of(a_file)) do
        local name = line:match("%.name%s*=%s*\"([%w_]+)\"")
        if name and line:find("%.part%s*=") then
            table.insert(names, name)
        end
    end
    if #names < 2 then
        raise("%s holds no feature rows with a .part, so the feature names are missing", FEATURES)
    end
    local listed = table.concat(names, ", ", 1, #names - 1) .. " and " .. names[#names] .. "."
    for _, block in ipairs(a_doc.blocks) do
        if block.kind == "section" and block.title == FEATURE_SECTION then
            table.insert(block.items, { kind = "para", words = { "The names: " .. listed } })
            return
        end
    end
    raise("%s has no # %s section for the feature names", SOURCE, FEATURE_SECTION)
end

-- a_parts as lines of at most WIDTH, the first after a_first and the rest
-- after a_rest, each part kept whole.
local function wrapped(a_parts, a_first, a_rest)
    local lines = {}
    local line = a_first
    local fresh = true
    for _, part in ipairs(a_parts) do
        if not fresh and #line + 1 + #part > WIDTH then
            table.insert(lines, line)
            line = a_rest .. part
        else
            line = fresh and line .. part or line .. " " .. part
        end
        fresh = false
    end
    table.insert(lines, line)
    return lines
end

local function words_of(a_text)
    local words = {}
    for word in a_text:gmatch("%S+") do
        table.insert(words, word)
    end
    return words
end

-- A signature breaks only after a comma.
local function signature_parts(a_signature)
    local parts = {}
    for part in (a_signature .. ", "):gmatch("(.-), ") do
        table.insert(parts, part .. ",")
    end
    parts[#parts] = parts[#parts]:sub(1, -2)
    return parts
end

local function append(a_out, a_lines)
    for _, line in ipairs(a_lines) do
        table.insert(a_out, line)
    end
end

-- 1 item of a section as plain text lines.
local function item_text(a_item)
    if a_item.kind == "para" then
        return wrapped(words_of(table.concat(a_item.words, " ")), "", "")
    elseif a_item.kind == "list" then
        local lines = {}
        for _, entry in ipairs(a_item.entries) do
            append(lines, wrapped(words_of(table.concat(entry, " ")), "", "   "))
        end
        return lines
    elseif a_item.kind == "code" then
        local lines = {}
        for _, line in ipairs(a_item.lines) do
            table.insert(lines, line == "" and "" or INDENT .. line)
        end
        return lines
    end
    local lines = { a_item.name }
    append(lines, wrapped(signature_parts(a_item.signature), INDENT, SIGNATURE_INDENT))
    append(lines, wrapped(words_of(table.concat(a_item.text, " ")), INDENT, INDENT))
    return lines
end

local function as_text(a_doc)
    local out = { TITLE, ("="):rep(#TITLE), "" }
    append(out, wrapped(words_of(a_doc.lead), "", ""))
    for _, block in ipairs(a_doc.blocks) do
        table.insert(out, "")
        table.insert(out, "")
        table.insert(out, block.title)
        table.insert(out, ((block.kind == "section" and "=" or "-"):rep(#block.title)))
        for _, item in ipairs(block.items) do
            table.insert(out, "")
            append(out, item_text(item))
        end
        for _, entry in ipairs(block.entries) do
            table.insert(out, "")
            if entry.struct then
                append(out, entry.signature)
            else
                append(out, wrapped(signature_parts(entry.signature[1]), "", SIGNATURE_INDENT))
            end
            append(out, wrapped(words_of(entry.line), INDENT, INDENT))
        end
    end
    return table.concat(out, "\n") .. "\n"
end

-- 1 item of a section as Nexus markup.
local function item_article(a_item)
    if a_item.kind == "para" then
        return table.concat(a_item.words, " ")
    elseif a_item.kind == "list" then
        local lines = { "[list=1]" }
        for _, entry in ipairs(a_item.entries) do
            -- The list numbers each entry itself.
            table.insert(lines, "[*]" .. table.concat(entry, " "):gsub("^%d+%.%s+", ""))
        end
        table.insert(lines, "[/list]")
        return table.concat(lines, "\n")
    elseif a_item.kind == "code" then
        return "[code]" .. table.concat(a_item.lines, "\n") .. "[/code]"
    end
    return string.format("[*][b]%s[/b]\n%s\n%s", a_item.name, MONO:format(a_item.signature), table.concat(a_item.text, " "))
end

local function as_article(a_doc)
    local out = { a_doc.lead }
    for _, block in ipairs(a_doc.blocks) do
        table.insert(out, "")
        table.insert(out, block.kind == "section" and HEADING:format(block.title) or "[b]" .. block.title .. "[/b]")
        -- Events that follow each other share 1 list.
        local previous
        for _, item in ipairs(block.items) do
            if previous == "event" and item.kind ~= "event" then
                table.insert(out, "[/list]")
            end
            if item.kind ~= "event" or previous ~= "event" then
                table.insert(out, "")
                if item.kind == "event" then
                    table.insert(out, "[list]")
                end
            end
            table.insert(out, item_article(item))
            previous = item.kind
        end
        if previous == "event" then
            table.insert(out, "[/list]")
        end
        if #block.entries > 0 then
            table.insert(out, "")
            table.insert(out, "[list]")
            for _, entry in ipairs(block.entries) do
                local signature = entry.struct and "[code]" .. table.concat(entry.signature, "\n") .. "[/code]" or MONO:format(entry.signature[1])
                table.insert(out, "[*]" .. signature .. "\n" .. entry.line)
            end
            table.insert(out, "[/list]")
        end
    end
    return table.concat(out, "\n") .. "\n"
end

-- Both files as this writes them, each with its path from the project's
-- root.
local function documents()
    local root = os.projectdir()
    local doc = parse(path.join(root, SOURCE))
    add_features(doc, path.join(root, FEATURES))
    return { { TEXT, as_text(doc) }, { ARTICLE, as_article(doc) } }
end

-- What xmake apidoc runs: both files written when they changed.
function write()
    for _, document in ipairs(documents()) do
        local file = path.join(os.projectdir(), document[1])
        if not os.isfile(file) or io.readfile(file, { encoding = "binary" }):gsub("\r", "") ~= document[2] then
            io.writefile(file, document[2], { encoding = "binary" })
            cprint("${bright green}wrote${clear} %s", document[1])
        else
            cprint("%s is as NEC.psc.in writes it", document[1])
        end
    end
end

-- What xmake release runs first: it stops when a committed copy differs from
-- what NEC.psc.in writes, naming its first line that does.
function check()
    local differ = {}
    for _, document in ipairs(documents()) do
        local file = path.join(os.projectdir(), document[1])
        local committed = os.isfile(file) and io.readfile(file, { encoding = "binary" }):gsub("\r", "") or nil
        if committed ~= document[2] then
            local number = 1
            if committed then
                local a = (committed .. "\n"):gmatch("(.-)\n")
                local b = (document[2] .. "\n"):gmatch("(.-)\n")
                while a() == b() do
                    number = number + 1
                end
            end
            table.insert(differ, committed and string.format("%s from line %d", document[1], number) or document[1] .. " is missing")
        end
    end
    if #differ > 0 then
        raise("the API document differs from NEC.psc.in, so run xmake apidoc and commit what it writes:\n  %s", table.concat(differ, "\n  "))
    end
end
