#pragma once

#include "asset_types.hpp"
#include <lua.hpp>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace crayon::assets {

class BundleManager {
public:
    static BundleManager& instance();

    void reset();

    bool define_bundle(const std::string& name, const std::vector<AssetEntry>& entries);
    bool has_bundle(const std::string& name) const;
    bool is_bundle_loaded(const std::string& name) const;
    std::vector<std::string> get_loaded_bundles() const;

    bool queue_bundle_async(lua_State* L, const std::string& name, int callback_ref);
    bool load_bundle_sync(lua_State* L, const std::string& name);

    void update(lua_State* L, float dt);

    bool unload_bundle(lua_State* L, const std::string& name);
    void unload_all(lua_State* L);

    bool is_done() const { return m_queue.empty() && m_total_queued == m_loaded_count; }
    float get_progress() const;
    int   get_percent() const;
    const std::string& get_current_item() const { return m_current_item; }
    size_t get_total_count() const { return m_total_queued; }
    size_t get_loaded_count() const { return m_loaded_count; }

    void  set_budget_ms(float ms) { m_budget_ms = ms; }
    float get_budget_ms() const { return m_budget_ms; }

    // Pushes the loaded asset onto the Lua stack, or nil if not found
    int push_asset(lua_State* L, const std::string& id, AssetType expected_type);
    bool has_asset(const std::string& id) const;

private:
    BundleManager() = default;

    bool load_single_asset(lua_State* L, const QueueTask& task);
    void store_in_registry(lua_State* L, const std::string& bundle, const std::string& id, AssetType type);
    void remove_from_registry(lua_State* L, const std::string& bundle);

    std::unordered_map<std::string, AssetBundle> m_bundles;
    std::unordered_map<std::string, std::string> m_id_to_bundle; // asset id -> bundle name

    std::deque<QueueTask> m_queue;
    size_t                m_total_queued = 0;
    size_t                m_loaded_count = 0;
    std::string           m_current_item;
    float                 m_budget_ms = 8.0f; // max ms per frame for loading

    struct CallbackInfo {
        std::string bundle_name;
        int         lua_ref;
    };
    std::vector<CallbackInfo> m_pending_callbacks;
};

} // namespace crayon::assets
