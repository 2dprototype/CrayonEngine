#include "lua_runtime.hpp"
#include "../core/log.hpp"
#include <string>
#include <unordered_set>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <iostream>

namespace crayon {

// ---------------------------------------------------------------------------
// LuaJIT (Lua 5.1 ABI) compat shims
//   5.2+ functions like lua_absindex / lua_isinteger / lua_rawlen don't exist
//   in LuaJIT, so we provide local replacements that compile cleanly.
// ---------------------------------------------------------------------------

static int pp_absindex(lua_State* L, int idx) {
    if (idx > 0 || idx <= LUA_REGISTRYINDEX) return idx;
    return lua_gettop(L) + idx + 1;
}

static bool pp_isinteger(lua_State* L, int idx) {
    if (!lua_isnumber(L, idx)) return false;
    lua_Number n = lua_tonumber(L, idx);
    return n == std::floor(n) &&
           n >= -9.2233720368547758e18 &&
           n <=  9.2233720368547758e18;
}

static size_t pp_rawlen(lua_State* L, int idx) {
    return lua_objlen(L, idx);
}

// ---------------------------------------------------------------------------
// ANSI colour codes
// ---------------------------------------------------------------------------
#define PP_RESET  "\033[0m"
#define PP_KEY    "\033[36m"   // cyan
#define PP_STR    "\033[32m"   // green
#define PP_NUM    "\033[33m"   // yellow
#define PP_BOOL   "\033[35m"   // magenta
#define PP_NIL    "\033[90m"   // bright black
#define PP_FUNC   "\033[34m"   // blue
#define PP_META   "\033[90m"   // dim

static void pp_value(lua_State* L, int idx, std::string& out, int indent,
                     std::unordered_set<const void*>& seen, bool color);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool pp_is_ident(const char* s, size_t len) {
    if (len == 0) return false;
    if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (size_t i = 1; i < len; ++i) {
        if (!(std::isalnum((unsigned char)s[i]) || s[i] == '_')) return false;
    }
    static const char* kws[] = {
        "and","break","do","else","elseif","end","false","for","function",
        "goto","if","in","local","nil","not","or","repeat","return","then",
        "true","until","while", nullptr
    };
    for (int i = 0; kws[i]; ++i) {
        if (len == std::strlen(kws[i]) && std::memcmp(s, kws[i], len) == 0)
            return false;
    }
    return true;
}

static void pp_escaped(std::string& out, const char* s, size_t len) {
    out += '"';
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\0': out += "\\0";  break;
            default:
                if (c < 32) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
}

static void pp_indent(std::string& out, int n) {
    out.append(static_cast<size_t>(n) * 2, ' ');
}

static void pp_key(lua_State* L, int kidx, std::string& out, int indent,
                   std::unordered_set<const void*>& seen, bool color) {
    kidx = pp_absindex(L, kidx);
    if (lua_type(L, kidx) == LUA_TSTRING) {
        size_t klen = 0;
        const char* ks = lua_tolstring(L, kidx, &klen);
        if (color) out += PP_KEY;
        if (pp_is_ident(ks, klen)) {
            out.append(ks, klen);
        } else {
            pp_escaped(out, ks, klen);
        }
        if (color) out += PP_RESET;
    } else {
        out += '[';
        pp_value(L, kidx, out, indent, seen, color);
        out += ']';
    }
}

// ---------------------------------------------------------------------------
// Value printer
// ---------------------------------------------------------------------------

static void pp_value(lua_State* L, int idx, std::string& out, int indent,
                     std::unordered_set<const void*>& seen, bool color) {
    idx = pp_absindex(L, idx);
    const int t = lua_type(L, idx);

    switch (t) {
        case LUA_TNIL:
            if (color) out += PP_NIL;
            out += "nil";
            if (color) out += PP_RESET;
            return;

        case LUA_TBOOLEAN:
            if (color) out += PP_BOOL;
            out += lua_toboolean(L, idx) ? "true" : "false";
            if (color) out += PP_RESET;
            return;

        case LUA_TNUMBER: {
            if (color) out += PP_NUM;
            char buf[64];
            if (pp_isinteger(L, idx)) {
                std::snprintf(buf, sizeof(buf), "%lld",
                              (long long)lua_tointeger(L, idx));
            } else {
                std::snprintf(buf, sizeof(buf), "%.14g", lua_tonumber(L, idx));
            }
            out += buf;
            if (color) out += PP_RESET;
            return;
        }

        case LUA_TSTRING: {
            if (color) out += PP_STR;
            size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);
            pp_escaped(out, s, len);
            if (color) out += PP_RESET;
            return;
        }

        case LUA_TTABLE: {
            const void* ptr = lua_topointer(L, idx);
            if (seen.count(ptr)) {
                if (color) out += PP_META;
                out += "<cycle>";
                if (color) out += PP_RESET;
                return;
            }
            seen.insert(ptr);

            const size_t array_len = pp_rawlen(L, idx);

            // First pass: count total entries, detect primitives.
            int total = 0;
            bool all_primitive = true;
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                ++total;
                int vt = lua_type(L, -1);
                if (!(vt == LUA_TNIL || vt == LUA_TBOOLEAN ||
                      vt == LUA_TNUMBER || vt == LUA_TSTRING)) {
                    all_primitive = false;
                }
                lua_pop(L, 1);
            }

            if (total == 0) {
                out += "{}";
                seen.erase(ptr);
                return;
            }

            const bool only_array = (static_cast<int>(array_len) == total);
            const bool compact    = all_primitive && total <= 4;

            if (compact) {
                out += "{ ";
                bool first = true;
                lua_pushnil(L);
                while (lua_next(L, idx) != 0) {
                    if (!first) out += ", ";
                    first = false;
                    if (only_array) {
                        pp_value(L, -1, out, indent, seen, color);
                    } else {
                        pp_key(L, -2, out, indent, seen, color);
                        out += " = ";
                        pp_value(L, -1, out, indent, seen, color);
                    }
                    lua_pop(L, 1);
                }
                out += " }";
            } else {
                out += "{\n";
                lua_pushnil(L);
                while (lua_next(L, idx) != 0) {
                    pp_indent(out, indent + 1);
                    if (only_array) {
                        pp_value(L, -1, out, indent + 1, seen, color);
                    } else {
                        pp_key(L, -2, out, indent + 1, seen, color);
                        out += " = ";
                        pp_value(L, -1, out, indent + 1, seen, color);
                    }
                    out += ",\n";
                    lua_pop(L, 1);
                }
                pp_indent(out, indent);
                out += "}";
            }

            seen.erase(ptr);
            return;
        }

        case LUA_TFUNCTION: {
            if (color) out += PP_FUNC;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "<function: %p",
                          lua_topointer(L, idx));
            out += buf;
            out += '>';
            if (color) out += PP_RESET;
            return;
        }

        case LUA_TUSERDATA:
        case LUA_TLIGHTUSERDATA: {
            if (color) out += PP_META;
            char buf[64];
            std::snprintf(buf, sizeof(buf), "<userdata: %p",
                          lua_topointer(L, idx));
            out += buf;
            out += '>';
            if (color) out += PP_RESET;
            return;
        }

        case LUA_TTHREAD:
            if (color) out += PP_META;
            out += "<thread>";
            if (color) out += PP_RESET;
            return;

        default:
            if (color) out += PP_META;
            out += '<';
            out += lua_typename(L, t);
            out += '>';
            if (color) out += PP_RESET;
            return;
    }
}

// ---------------------------------------------------------------------------
// crayon.print(...)
// ---------------------------------------------------------------------------

static int l_crayon_print(lua_State* L) {
    const int n = lua_gettop(L);
    std::string out;

    // Colours only when the console is enabled — matches log behaviour.
    // If you don't have is_console_enabled() in your log.hpp yet, just
    // delete these two lines and uncomment the fallback below.
#ifdef __ANDROID__
    const bool color = false;                          // logcat does not render ANSI colours
#else
    const bool color = crayon::is_console_enabled();   // optional
#endif
    // const bool color = true;                        // fallback

    for (int i = 1; i <= n; ++i) {
        if (i > 1) out += ' ';
        std::unordered_set<const void*> seen;
        pp_value(L, i, out, 0, seen, color);
    }

    // Write directly to stdout. No dependency on log_raw / LogLevel::Raw,
    // so this file builds standalone against the current log.hpp.
    if (crayon::is_console_enabled()) {                 // optional
#ifdef __ANDROID__
        crayon::log_message(crayon::LogLevel::Raw, out); // -> logcat (stdout is not visible on Android)
#else
        std::cout << out << '\n';
#endif
    }
    // If you removed is_console_enabled() above, replace the line above with:
    // std::cout << out << '\n';

    return 0;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_print_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

    lua_pushcfunction(L, l_crayon_print);
    lua_setfield(L, -2, "print");

    lua_pop(L, 1);
}

} // namespace crayon