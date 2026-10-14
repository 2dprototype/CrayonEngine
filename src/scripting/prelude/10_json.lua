-- crayon.json: small JSON decoder/encoder (used by crayon.tilemap for Tiled maps; handy for saves).
--   crayon.json.decode(str) -> value        (JSON null becomes crayon.json.null)
--   crayon.json.encode(value [, indent]) -> string
local json = { null = setmetatable({}, { __tostring = function() return "null" end }) }

local function decode_error(str, i, msg)
    local line, col = 1, 1
    for k = 1, math.min(i, #str) do
        if str:sub(k, k) == "\n" then line = line + 1; col = 1 else col = col + 1 end
    end
    error(string.format("json: %s at line %d, column %d", msg, line, col), 0)
end

local function skip_ws(str, i)
    local _, e = str:find("^[ \t\r\n]*", i)
    return e + 1
end

local escapes = { ['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t" }

local function utf8_encode(cp)
    if cp < 0x80 then return string.char(cp)
    elseif cp < 0x800 then return string.char(0xC0 + math.floor(cp / 64), 0x80 + cp % 64)
    elseif cp < 0x10000 then
        return string.char(0xE0 + math.floor(cp / 4096), 0x80 + math.floor(cp / 64) % 64, 0x80 + cp % 64)
    end
    return string.char(0xF0 + math.floor(cp / 262144), 0x80 + math.floor(cp / 4096) % 64,
                       0x80 + math.floor(cp / 64) % 64, 0x80 + cp % 64)
end

local parse_value

local function parse_string(str, i)
    local out, start = {}, i + 1
    local k = start
    while true do
        local c = str:sub(k, k)
        if c == "" then decode_error(str, k, "unterminated string") end
        if c == '"' then
            out[#out + 1] = str:sub(start, k - 1)
            return table.concat(out), k + 1
        elseif c == "\\" then
            out[#out + 1] = str:sub(start, k - 1)
            local e = str:sub(k + 1, k + 1)
            if e == "u" then
                local hex = str:sub(k + 2, k + 5)
                if not hex:match("^%x%x%x%x$") then decode_error(str, k, "bad \\u escape") end
                local cp = tonumber(hex, 16)
                k = k + 6
                if cp >= 0xD800 and cp <= 0xDBFF and str:sub(k, k + 1) == "\\u" then   -- surrogate pair
                    local lo = tonumber(str:sub(k + 2, k + 5), 16)
                    if lo and lo >= 0xDC00 and lo <= 0xDFFF then
                        cp = 0x10000 + (cp - 0xD800) * 1024 + (lo - 0xDC00)
                        k = k + 6
                    end
                end
                out[#out + 1] = utf8_encode(cp)
            else
                local r = escapes[e]
                if not r then decode_error(str, k, "bad escape '\\" .. e .. "'") end
                out[#out + 1] = r
                k = k + 2
            end
            start = k
        else
            k = k + 1
        end
    end
end

local function parse_number(str, i)
    local s = str:match("^-?%d+%.?%d*[eE]?[+-]?%d*", i)
    local n = s and tonumber(s)
    if not n then decode_error(str, i, "bad number") end
    return n, i + #s
end

local function parse_array(str, i)
    local arr, n = {}, 0
    i = skip_ws(str, i + 1)
    if str:sub(i, i) == "]" then return arr, i + 1 end
    while true do
        local v
        v, i = parse_value(str, i)
        n = n + 1
        arr[n] = v
        i = skip_ws(str, i)
        local c = str:sub(i, i)
        if c == "]" then return arr, i + 1 end
        if c ~= "," then decode_error(str, i, "expected ',' or ']'") end
        i = skip_ws(str, i + 1)
    end
end

local function parse_object(str, i)
    local obj = {}
    i = skip_ws(str, i + 1)
    if str:sub(i, i) == "}" then return obj, i + 1 end
    while true do
        if str:sub(i, i) ~= '"' then decode_error(str, i, "expected string key") end
        local key
        key, i = parse_string(str, i)
        i = skip_ws(str, i)
        if str:sub(i, i) ~= ":" then decode_error(str, i, "expected ':'") end
        i = skip_ws(str, i + 1)
        obj[key], i = parse_value(str, i)
        i = skip_ws(str, i)
        local c = str:sub(i, i)
        if c == "}" then return obj, i + 1 end
        if c ~= "," then decode_error(str, i, "expected ',' or '}'") end
        i = skip_ws(str, i + 1)
    end
end

parse_value = function(str, i)
    i = skip_ws(str, i)
    local c = str:sub(i, i)
    if c == "{" then return parse_object(str, i)
    elseif c == "[" then return parse_array(str, i)
    elseif c == '"' then return parse_string(str, i)
    elseif c == "-" or c:match("%d") then return parse_number(str, i)
    elseif str:sub(i, i + 3) == "true" then return true, i + 4
    elseif str:sub(i, i + 4) == "false" then return false, i + 5
    elseif str:sub(i, i + 3) == "null" then return json.null, i + 4
    end
    decode_error(str, i, "unexpected character '" .. c .. "'")
end

function json.decode(str)
    if type(str) ~= "string" then error("json.decode expects a string", 2) end
    if str:sub(1, 3) == "\239\187\191" then str = str:sub(4) end   -- UTF-8 BOM
    local v, i = parse_value(str, 1)
    i = skip_ws(str, i)
    if i <= #str then decode_error(str, i, "trailing characters") end
    return v
end

local function escape_string(s)
    return '"' .. s:gsub('[%c"\\]', function(c)
        local map = { ['"'] = '\\"', ["\\"] = "\\\\", ["\b"] = "\\b", ["\f"] = "\\f", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t" }
        return map[c] or string.format("\\u%04x", c:byte())
    end) .. '"'
end

local function is_array(t)
    local n = 0
    for k in pairs(t) do
        if type(k) ~= "number" or k < 1 or k % 1 ~= 0 then return false end
        n = n + 1
    end
    return n == #t
end

local function encode_value(v, indent, level, seen)
    local tv = type(v)
    if v == json.null or v == nil then return "null"
    elseif tv == "boolean" then return tostring(v)
    elseif tv == "number" then
        if v ~= v or v == math.huge or v == -math.huge then error("json.encode: cannot encode nan/inf", 0) end
        if v % 1 == 0 and math.abs(v) < 1e15 then return string.format("%d", v) end
        return string.format("%.14g", v)
    elseif tv == "string" then return escape_string(v)
    elseif tv == "table" then
        if seen[v] then error("json.encode: circular reference", 0) end
        seen[v] = true
        local parts, nl, pad, pad_in = {}, indent and "\n" or "", indent and string.rep(indent, level) or "", indent and string.rep(indent, level + 1) or ""
        local result
        if is_array(v) then
            if #v == 0 then result = "[]" else
                for i = 1, #v do parts[i] = pad_in .. encode_value(v[i], indent, level + 1, seen) end
                result = "[" .. nl .. table.concat(parts, "," .. nl) .. nl .. pad .. "]"
            end
        else
            local keys = {}
            for k in pairs(v) do
                if type(k) ~= "string" then error("json.encode: object keys must be strings", 0) end
                keys[#keys + 1] = k
            end
            table.sort(keys)
            for i, k in ipairs(keys) do
                parts[i] = pad_in .. escape_string(k) .. ":" .. (indent and " " or "") .. encode_value(v[k], indent, level + 1, seen)
            end
            result = "{" .. nl .. table.concat(parts, "," .. nl) .. nl .. pad .. "}"
        end
        seen[v] = nil
        return result
    end
    error("json.encode: cannot encode " .. tv, 0)
end

-- indent: nil for compact output, or a string such as "  " for pretty printing.
function json.encode(value, indent)
    return encode_value(value, indent, 0, {})
end

crayon.json = json
