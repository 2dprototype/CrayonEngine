// assets plugin: Asynchronous mission-based asset loading and memory management.
//
//   function crayon.config(t) t.plugins.assets = true end
//

#include <lua.hpp>
#include "crayon_plugin.h"
#include "bundle_manager.hpp"

namespace {

using namespace crayon::assets;

const CrayonHostAPI* s_host = nullptr;

static int l_assets_define(lua_State* L) {
    const char* bundle_name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    std::vector<AssetEntry> entries;

    auto parse_category = [&](const char* cat_name, AssetType type) {
        lua_getfield(L, 2, cat_name);
        if (lua_istable(L, -1)) {
            int len = static_cast<int>(lua_objlen(L, -1));
            for (int i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                if (lua_istable(L, -1)) {
                    AssetEntry e;
                    e.type = type;

                    lua_getfield(L, -1, "id");
                    if (lua_isstring(L, -1)) e.id = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "path");
                    if (lua_isstring(L, -1)) e.path = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "filter");
                    if (lua_isstring(L, -1)) e.filter = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "wrap");
                    if (lua_isstring(L, -1)) e.wrap = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    if (!e.id.empty() && !e.path.empty()) {
                        entries.push_back(std::move(e));
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    };

    parse_category("textures", AssetType::Texture);
    parse_category("models",   AssetType::Model);
    parse_category("sounds",   AssetType::Sound);
    parse_category("music",    AssetType::Music);
    parse_category("data",     AssetType::Data);

    bool ok = BundleManager::instance().define_bundle(bundle_name, entries);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_assets_load_async(lua_State* L) {
    const char* bundle_name = luaL_checkstring(L, 1);
    int callback_ref = LUA_NOREF;
    if (lua_isfunction(L, 2)) {
        lua_pushvalue(L, 2);
        callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    bool ok = BundleManager::instance().queue_bundle_async(L, bundle_name, callback_ref);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_assets_load_sync(lua_State* L) {
    const char* bundle_name = luaL_checkstring(L, 1);
    bool ok = BundleManager::instance().load_bundle_sync(L, bundle_name);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_assets_is_done(lua_State* L) {
    lua_pushboolean(L, BundleManager::instance().is_done());
    return 1;
}

static int l_assets_get_progress(lua_State* L) {
    lua_pushnumber(L, BundleManager::instance().get_progress());
    return 1;
}

static int l_assets_get_percent(lua_State* L) {
    lua_pushinteger(L, BundleManager::instance().get_percent());
    return 1;
}

static int l_assets_get_current_item(lua_State* L) {
    lua_pushstring(L, BundleManager::instance().get_current_item().c_str());
    return 1;
}

static int l_assets_get_count(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(BundleManager::instance().get_loaded_count()));
    lua_pushinteger(L, static_cast<lua_Integer>(BundleManager::instance().get_total_count()));
    return 2;
}

static int l_assets_set_budget(lua_State* L) {
    float ms = static_cast<float>(luaL_checknumber(L, 1));
    if (ms < 0.5f) ms = 0.5f;
    BundleManager::instance().set_budget_ms(ms);
    return 0;
}

static int l_assets_get_budget(lua_State* L) {
    lua_pushnumber(L, BundleManager::instance().get_budget_ms());
    return 1;
}

static int l_assets_get_texture(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Texture);
}

static int l_assets_get_model(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Model);
}

static int l_assets_get_sound(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Sound);
}

static int l_assets_get_music(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Music);
}

static int l_assets_get_data(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Data);
}

static int l_assets_get(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    return BundleManager::instance().push_asset(L, id, AssetType::Unknown);
}

static int l_assets_has(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    lua_pushboolean(L, BundleManager::instance().has_asset(id));
    return 1;
}

static int l_assets_unload(lua_State* L) {
    const char* bundle_name = luaL_checkstring(L, 1);
    bool ok = BundleManager::instance().unload_bundle(L, bundle_name);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_assets_unload_all(lua_State* L) {
    BundleManager::instance().unload_all(L);
    return 0;
}

static int l_assets_is_loaded(lua_State* L) {
    const char* bundle_name = luaL_checkstring(L, 1);
    lua_pushboolean(L, BundleManager::instance().is_bundle_loaded(bundle_name));
    return 1;
}

static int l_assets_get_loaded_bundles(lua_State* L) {
    auto list = BundleManager::instance().get_loaded_bundles();
    lua_newtable(L);
    for (size_t i = 0; i < list.size(); ++i) {
        lua_pushstring(L, list[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

// ---- plugin callbacks ----------------------------------------------------

int p_on_load(const CrayonHostAPI* host) {
    s_host = host;
    if (CRAYON_HOST_HAS(host, log)) {
        host->log(CRAYON_LOG_LEVEL_INFO, "[assets] Crayon Asset Loader plugin loaded");
    }
    return 0;
}

int p_on_register(lua_State* L) {
    BundleManager::instance().reset();

    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    lua_newtable(L);

    // Internal registry for loaded assets
    lua_newtable(L);
    lua_setfield(L, -2, "_registry");

    struct Entry { const char* name; lua_CFunction fn; };
    static const Entry entries[] = {
        {"define",           l_assets_define},
        {"defineBundle",     l_assets_define},
        {"loadAsync",        l_assets_load_async},
        {"loadSync",         l_assets_load_sync},
        {"isDone",           l_assets_is_done},
        {"getProgress",      l_assets_get_progress},
        {"getPercent",       l_assets_get_percent},
        {"getCurrentItem",   l_assets_get_current_item},
        {"getCount",         l_assets_get_count},
        {"setBudget",        l_assets_set_budget},
        {"getBudget",        l_assets_get_budget},
        {"getTexture",       l_assets_get_texture},
        {"getModel",         l_assets_get_model},
        {"getSound",         l_assets_get_sound},
        {"getMusic",         l_assets_get_music},
        {"getData",          l_assets_get_data},
        {"get",              l_assets_get},
        {"has",              l_assets_has},
        {"unload",           l_assets_unload},
        {"unloadAll",        l_assets_unload_all},
        {"isLoaded",         l_assets_is_loaded},
        {"getLoadedBundles", l_assets_get_loaded_bundles},
    };

    for (const Entry& e : entries) {
        lua_pushcfunction(L, e.fn);
        lua_setfield(L, -2, e.name);
    }

    lua_setfield(L, -2, "assets");
    lua_pop(L, 1); // pop crayon
    return 0;
}

void p_on_update(lua_State* L, float dt) {
    BundleManager::instance().update(L, dt);
}

void p_on_unload(void) {
    BundleManager::instance().reset();
    s_host = nullptr;
}

const CrayonPlugin k_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "assets",
    "1.0.0",
    "Asynchronous mission-based asset loading and memory management",
    p_on_load,
    p_on_register,
    p_on_update,
    p_on_unload,
};

} // namespace

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &k_plugin;
}
