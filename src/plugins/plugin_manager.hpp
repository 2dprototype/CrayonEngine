#pragma once

// PluginManager: finds, loads, drives and unloads native plugins (.dll / .so / .dylib).
// See plugins/include/crayon_plugin.h for the plugin-facing ABI and plugins/README.md for the guide.
//
// This class deliberately has no dependency on Engine or on the Lua C API (lua_State is only passed
// through), so it can be unit-tested on its own (plugins/tests/).

#include <filesystem>
#include <string>
#include <vector>

#include "../core/engine_config.hpp"
#include "crayon_plugin.h"

namespace crayon {

struct PluginInfo {
    std::string name;         // descriptor name (what the plugin calls itself, e.g. "physics4D")
    std::string key;          // the t.plugins key that requested it
    std::string version;
    std::string description;
    std::string path;         // file it was loaded from
};

struct PluginFailure {
    std::string name;         // t.plugins key
    std::string reason;
};

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // Load every requested plugin.  Failures are logged and remembered (failures()), never fatal.
    void load(const std::vector<PluginRequest>& requests, const CrayonHostAPI* host);

    // Let every loaded plugin register its Lua API.  Call after the engine's own modules.
    void register_lua(lua_State* L);

    // Per-frame hook (before crayon.update).
    void update(lua_State* L, float dt);

    // on_unload() every plugin (reverse load order) and release the libraries.
    // Call AFTER the Lua state has been closed.
    void unload();

    bool is_loaded(const std::string& name) const;            // case-insensitive
    const std::vector<PluginInfo>& loaded() const { return m_infos; }
    const std::vector<PluginFailure>& failures() const { return m_failures; }

    // ---- file lookup helpers (public so they can be tested) ----
    static std::filesystem::path executable_dir();
    static std::vector<std::filesystem::path> search_dirs();                 // in priority order
    static std::vector<std::string> candidate_filenames(const std::string& name);
    static std::filesystem::path resolve(const PluginRequest& req, std::string& tried_out);   // empty = not found

private:
    struct Slot {
        void*               handle = nullptr;
        const CrayonPlugin* desc   = nullptr;
        bool                loaded_ok = false;   // on_load succeeded -> on_unload owed
        bool                active    = false;   // on_register succeeded -> receives on_update
    };

    void fail(const std::string& key, const std::string& reason);

    std::vector<Slot>          m_slots;
    std::vector<PluginInfo>    m_infos;      // parallel to m_slots
    std::vector<PluginFailure> m_failures;
    const CrayonHostAPI*       m_host = nullptr;
};

} // namespace crayon
