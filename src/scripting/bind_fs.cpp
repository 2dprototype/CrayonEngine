#include "lua_runtime.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <SDL3/SDL.h>

namespace crayon {

static int l_fs_exists(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool exists = std::filesystem::exists(path);
    lua_pushboolean(L, exists);
    return 1;
}

static int l_fs_is_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool is_f = std::filesystem::is_regular_file(path);
    lua_pushboolean(L, is_f);
    return 1;
}

static int l_fs_is_directory(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool is_d = std::filesystem::is_directory(path);
    lua_pushboolean(L, is_d);
    return 1;
}

static int l_fs_read_text(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::ifstream f(path);
    if (!f.is_open()) {
        lua_pushnil(L);
        lua_pushstring(L, "could not open file for reading");
        return 2;
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string str = buffer.str();
    lua_pushlstring(L, str.data(), str.size());
    return 1;
}

static int l_fs_write_text(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* content = luaL_checklstring(L, 2, &len);

    // Create parent directory if needed
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream f(path);
    if (!f.is_open()) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "could not open file for writing");
        return 2;
    }
    f.write(content, len);
    lua_pushboolean(L, true);
    return 1;
}

static int l_fs_append_text(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* content = luaL_checklstring(L, 2, &len);

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream f(path, std::ios::app);
    if (!f.is_open()) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "could not open file for appending");
        return 2;
    }
    f.write(content, len);
    lua_pushboolean(L, true);
    return 1;
}

static int l_fs_list_dir(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_newtable(L);
    int idx = 1;

    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        lua_pushstring(L, entry.path().filename().string().c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static int l_fs_get_save_dir(lua_State* L) {
    const char* org = luaL_optstring(L, 1, "CrayonEngine");
    const char* app = luaL_optstring(L, 2, "Game");
    char* pref = SDL_GetPrefPath(org, app);
    if (pref) {
        lua_pushstring(L, pref);
        SDL_free(pref);
    } else {
        lua_pushstring(L, "./save/");
    }
    return 1;
}

void register_fs_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_fs_exists);
    lua_setfield(L, -2, "exists");

    lua_pushcfunction(L, l_fs_is_file);
    lua_setfield(L, -2, "isFile");

    lua_pushcfunction(L, l_fs_is_directory);
    lua_setfield(L, -2, "isDirectory");

    lua_pushcfunction(L, l_fs_read_text);
    lua_setfield(L, -2, "readText");

    lua_pushcfunction(L, l_fs_write_text);
    lua_setfield(L, -2, "writeText");

    lua_pushcfunction(L, l_fs_append_text);
    lua_setfield(L, -2, "appendText");

    lua_pushcfunction(L, l_fs_list_dir);
    lua_setfield(L, -2, "listDir");

    lua_pushcfunction(L, l_fs_get_save_dir);
    lua_setfield(L, -2, "getSaveDir");

    lua_setfield(L, -2, "fs");
    lua_pop(L, 1);
}

} // namespace crayon
