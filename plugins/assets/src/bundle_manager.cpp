#include "bundle_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace crayon::assets {

BundleManager& BundleManager::instance() {
    static BundleManager s_mgr;
    return s_mgr;
}

void BundleManager::reset() {
    m_bundles.clear();
    m_id_to_bundle.clear();
    m_queue.clear();
    m_total_queued = 0;
    m_loaded_count = 0;
    m_current_item.clear();
    m_pending_callbacks.clear();
}

bool BundleManager::define_bundle(const std::string& name, const std::vector<AssetEntry>& entries) {
    if (name.empty()) return false;
    AssetBundle bundle;
    bundle.name = name;
    bundle.items = entries;
    bundle.loaded = false;
    m_bundles[name] = std::move(bundle);
    return true;
}

bool BundleManager::has_bundle(const std::string& name) const {
    return m_bundles.find(name) != m_bundles.end();
}

bool BundleManager::is_bundle_loaded(const std::string& name) const {
    auto it = m_bundles.find(name);
    return it != m_bundles.end() && it->second.loaded;
}

std::vector<std::string> BundleManager::get_loaded_bundles() const {
    std::vector<std::string> result;
    for (const auto& [name, b] : m_bundles) {
        if (b.loaded) result.push_back(name);
    }
    return result;
}

float BundleManager::get_progress() const {
    if (m_total_queued == 0) return 1.0f;
    float p = static_cast<float>(m_loaded_count) / static_cast<float>(m_total_queued);
    if (p > 1.0f) p = 1.0f;
    return p;
}

int BundleManager::get_percent() const {
    return static_cast<int>(get_progress() * 100.0f);
}

bool BundleManager::queue_bundle_async(lua_State* L, const std::string& name, int callback_ref) {
    auto it = m_bundles.find(name);
    if (it == m_bundles.end()) return false;
    if (it->second.loaded) {
        if (callback_ref != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);
            if (lua_isfunction(L, -1)) {
                lua_pushstring(L, name.c_str());
                lua_pcall(L, 1, 0, 0);
            } else {
                lua_pop(L, 1);
            }
            luaL_unref(L, LUA_REGISTRYINDEX, callback_ref);
        }
        return true;
    }

    // Reset progress tracking if queue was previously empty
    if (m_queue.empty()) {
        m_total_queued = 0;
        m_loaded_count = 0;
    }

    for (const auto& entry : it->second.items) {
        m_queue.push_back(QueueTask{ name, entry });
        m_total_queued++;
    }

    if (callback_ref != LUA_NOREF) {
        m_pending_callbacks.push_back(CallbackInfo{ name, callback_ref });
    }
    return true;
}

bool BundleManager::load_bundle_sync(lua_State* L, const std::string& name) {
    auto it = m_bundles.find(name);
    if (it == m_bundles.end()) return false;
    if (it->second.loaded) return true;

    for (const auto& entry : it->second.items) {
        QueueTask task{ name, entry };
        m_current_item = entry.path;
        load_single_asset(L, task);
    }

    it->second.loaded = true;
    m_current_item.clear();
    return true;
}

void BundleManager::update(lua_State* L, float /*dt*/) {
    if (m_queue.empty()) return;

    auto start_time = std::chrono::high_resolution_clock::now();

    while (!m_queue.empty()) {
        QueueTask task = m_queue.front();
        m_queue.pop_front();

        m_current_item = task.entry.path;
        load_single_asset(L, task);
        m_loaded_count++;

        // Check if the bundle this item belongs to has finished loading
        bool bundle_has_more = false;
        for (const auto& q : m_queue) {
            if (q.bundle_name == task.bundle_name) {
                bundle_has_more = true;
                break;
            }
        }
        if (!bundle_has_more) {
            auto it = m_bundles.find(task.bundle_name);
            if (it != m_bundles.end()) {
                it->second.loaded = true;
            }
        }

        // Budget check: do not exceed frame time budget (e.g. 8ms)
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed_ms = std::chrono::duration<float, std::milli>(now - start_time).count();
        if (elapsed_ms >= m_budget_ms) {
            break;
        }
    }

    if (m_queue.empty()) {
        m_current_item.clear();

        // Fire any pending completion callbacks
        for (const auto& cb : m_pending_callbacks) {
            if (cb.lua_ref != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, cb.lua_ref);
                if (lua_isfunction(L, -1)) {
                    lua_pushstring(L, cb.bundle_name.c_str());
                    lua_pcall(L, 1, 0, 0);
                } else {
                    lua_pop(L, 1);
                }
                luaL_unref(L, LUA_REGISTRYINDEX, cb.lua_ref);
            }
        }
        m_pending_callbacks.clear();
    }
}

void BundleManager::store_in_registry(lua_State* L, const std::string& bundle, const std::string& id, AssetType type) {
    // Expects the loaded asset value at the top of the Lua stack (-1).
    // Stores in crayon.assets._registry[bundle][id] = value
    m_id_to_bundle[id] = bundle;

    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }

    lua_getfield(L, -1, "assets");
    if (!lua_istable(L, -1)) { lua_pop(L, 3); return; }

    lua_getfield(L, -1, "_registry");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "_registry");
    }

    lua_getfield(L, -1, bundle.c_str());
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, bundle.c_str());
    }

    // Set _registry[bundle][id] = asset_value (which is at -5)
    lua_pushvalue(L, -5);
    lua_setfield(L, -2, id.c_str());

    // If it's a sound, keep track in __sounds array for clean unloading
    if (type == AssetType::Sound && lua_isnumber(L, -5)) {
        lua_getfield(L, -1, "__sounds");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, -3, "__sounds");
        }
        int len = static_cast<int>(lua_objlen(L, -1));
        lua_pushvalue(L, -6); // the sound ID
        lua_rawseti(L, -2, len + 1);
        lua_pop(L, 1); // pop __sounds
    }

    lua_pop(L, 4); // pop bundle_table, _registry, assets, crayon
    lua_pop(L, 1); // pop original asset value
}

bool BundleManager::load_single_asset(lua_State* L, const QueueTask& task) {
    switch (task.entry.type) {
        case AssetType::Texture: {
            lua_getglobal(L, "crayon");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "graphics");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "loadTexture");
                    if (lua_isfunction(L, -1)) {
                        lua_pushstring(L, task.entry.path.c_str());
                        int nargs = 1;
                        if (!task.entry.filter.empty() || !task.entry.wrap.empty()) {
                            lua_newtable(L);
                            if (!task.entry.filter.empty()) {
                                lua_pushstring(L, task.entry.filter.c_str());
                                lua_setfield(L, -2, "filter");
                            }
                            if (!task.entry.wrap.empty()) {
                                lua_pushstring(L, task.entry.wrap.c_str());
                                lua_setfield(L, -2, "wrap");
                            }
                            nargs = 2;
                        }
                        if (lua_pcall(L, nargs, 1, 0) == 0) {
                            store_in_registry(L, task.bundle_name, task.entry.id, task.entry.type);
                            lua_pop(L, 2); // pop graphics, crayon
                            return true;
                        }
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            return false;
        }

        case AssetType::Model: {
            lua_getglobal(L, "crayon");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "graphics");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "loadModel");
                    if (lua_isfunction(L, -1)) {
                        lua_pushstring(L, task.entry.path.c_str());
                        if (lua_pcall(L, 1, 1, 0) == 0) {
                            store_in_registry(L, task.bundle_name, task.entry.id, task.entry.type);
                            lua_pop(L, 2);
                            return true;
                        }
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            return false;
        }

        case AssetType::Sound: {
            lua_getglobal(L, "crayon");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "audio");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "loadSound");
                    if (lua_isfunction(L, -1)) {
                        lua_pushstring(L, task.entry.path.c_str());
                        if (lua_pcall(L, 1, 1, 0) == 0) {
                            store_in_registry(L, task.bundle_name, task.entry.id, task.entry.type);
                            lua_pop(L, 2);
                            return true;
                        }
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            return false;
        }

        case AssetType::Music: {
            // Music is streamed by path; store path string
            lua_pushstring(L, task.entry.path.c_str());
            store_in_registry(L, task.bundle_name, task.entry.id, task.entry.type);
            return true;
        }

        case AssetType::Data: {
            std::ifstream file(task.entry.path, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                std::string content = ss.str();
                lua_pushlstring(L, content.data(), content.size());
            } else {
                lua_pushnil(L);
            }
            store_in_registry(L, task.bundle_name, task.entry.id, task.entry.type);
            return true;
        }

        default:
            return false;
    }
}

int BundleManager::push_asset(lua_State* L, const std::string& id, AssetType /*expected_type*/) {
    auto it = m_id_to_bundle.find(id);
    if (it == m_id_to_bundle.end()) {
        lua_pushnil(L);
        return 1;
    }

    const std::string& bundle = it->second;

    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_pushnil(L); return 1; }

    lua_getfield(L, -1, "assets");
    if (!lua_istable(L, -1)) { lua_pop(L, 2); lua_pushnil(L); return 1; }

    lua_getfield(L, -1, "_registry");
    if (!lua_istable(L, -1)) { lua_pop(L, 3); lua_pushnil(L); return 1; }

    lua_getfield(L, -1, bundle.c_str());
    if (!lua_istable(L, -1)) { lua_pop(L, 4); lua_pushnil(L); return 1; }

    lua_getfield(L, -1, id.c_str());
    // Move the asset value to replace crayon
    lua_insert(L, -5);
    lua_pop(L, 4); // pop bundle, _registry, assets, crayon
    return 1;
}

bool BundleManager::has_asset(const std::string& id) const {
    return m_id_to_bundle.find(id) != m_id_to_bundle.end();
}

bool BundleManager::unload_bundle(lua_State* L, const std::string& name) {
    auto it = m_bundles.find(name);
    if (it == m_bundles.end() || !it->second.loaded) return false;

    // 1. Unload sound buffers in audio engine if any
    lua_getglobal(L, "crayon");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "assets");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "_registry");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, name.c_str());
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "__sounds");
                    if (lua_istable(L, -1)) {
                        lua_getfield(L, -5, "audio");
                        if (lua_istable(L, -1)) {
                            lua_getfield(L, -1, "unloadSound");
                            if (lua_isfunction(L, -1)) {
                                int count = static_cast<int>(lua_objlen(L, -3));
                                for (int i = 1; i <= count; ++i) {
                                    lua_pushvalue(L, -1); // unloadSound
                                    lua_rawgeti(L, -4, i); // sound ID
                                    lua_pcall(L, 1, 0, 0);
                                }
                            }
                            lua_pop(L, 1);
                        }
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1);
                }
                // Set _registry[name] = nil
                lua_pushnil(L);
                lua_setfield(L, -2, name.c_str());
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    // 2. Remove entries from m_id_to_bundle
    for (const auto& entry : it->second.items) {
        m_id_to_bundle.erase(entry.id);
    }
    it->second.loaded = false;

    // 3. Trigger Lua garbage collection step to reclaim memory immediately
    lua_gc(L, LUA_GCCOLLECT, 0);
    return true;
}

void BundleManager::unload_all(lua_State* L) {
    for (auto& [name, b] : m_bundles) {
        if (b.loaded) {
            unload_bundle(L, name);
        }
    }
}

} // namespace crayon::assets
