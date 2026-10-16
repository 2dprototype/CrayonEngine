#include "plugin_manager.hpp"
#include "../core/log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #if defined(__APPLE__)
        #include <mach-o/dyld.h>
    #endif
#endif

namespace crayon {

namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------------------------
// tiny platform layer
// ---------------------------------------------------------------------------------------------
std::string last_os_error() {
#if defined(_WIN32)
    const DWORD code = GetLastError();
    char buf[512] = {0};
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf) - 1, nullptr);
    std::string s(buf, n);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s.empty() ? ("error " + std::to_string(code)) : (s + " (error " + std::to_string(code) + ")");
#else
    const char* e = dlerror();
    return e ? e : "unknown error";
#endif
}

void* open_library(const fs::path& p) {
#if defined(_WIN32)
    // ALTERED_SEARCH_PATH: the plugin's own directory is searched for ITS dependencies first
    // (so plugins/foo.dll can ship next to a private foo_helper.dll).
    return reinterpret_cast<void*>(LoadLibraryExW(fs::absolute(p).wstring().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    return dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* find_symbol(void* lib, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(lib), name));
#else
    return dlsym(lib, name);
#endif
}

void close_library(void* lib) {
    if (!lib) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(lib));
#else
    dlclose(lib);
#endif
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool valid_plugin_name(const std::string& n) {
    if (n.empty() || n.size() > 64) return false;
    for (char c : n) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.')) return false;
    }
    return n.find("..") == std::string::npos;
}

#if defined(_WIN32)
constexpr const char* kLibExt = ".dll";
#elif defined(__APPLE__)
constexpr const char* kLibExt = ".dylib";
#else
constexpr const char* kLibExt = ".so";
#endif

void push_unique(std::vector<fs::path>& v, const fs::path& p) {
    if (p.empty()) return;
    std::error_code ec;
    fs::path n = fs::weakly_canonical(p, ec);
    if (ec) n = p.lexically_normal();
    for (const auto& e : v) if (e == n) return;
    v.push_back(n);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// lookup helpers
// ---------------------------------------------------------------------------------------------
fs::path PluginManager::executable_dir() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) return fs::path(buf).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) {
        fs::path p = fs::weakly_canonical(fs::path(buf), ec);
        if (!ec) return p.parent_path();
    }
#else
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && !p.empty()) return p.parent_path();
#endif
    return fs::current_path(ec);
}

std::vector<fs::path> PluginManager::search_dirs() {
    std::vector<fs::path> dirs;
    std::error_code ec;

    // 1. CRAYON_PLUGIN_PATH (';' separated on Windows, ':' elsewhere) - handy for development.
    if (const char* env = std::getenv("CRAYON_PLUGIN_PATH")) {
#if defined(_WIN32)
        const char sep = ';';
#else
        const char sep = ':';
#endif
        std::string all = env;
        size_t start = 0;
        while (start <= all.size()) {
            size_t end = all.find(sep, start);
            if (end == std::string::npos) end = all.size();
            if (end > start) push_unique(dirs, fs::path(all.substr(start, end - start)));
            start = end + 1;
        }
    }

    // 2. Next to the crayon binary (the documented place), and its plugins/ subfolder.
    const fs::path exe = executable_dir();
    push_unique(dirs, exe);
    push_unique(dirs, exe / "plugins");

    // 3. The project folder (the engine chdir()s there) and its plugins/ subfolder.
    const fs::path cwd = fs::current_path(ec);
    push_unique(dirs, cwd);
    push_unique(dirs, cwd / "plugins");
    return dirs;
}

std::vector<std::string> PluginManager::candidate_filenames(const std::string& name) {
    std::vector<std::string> out;
    auto add = [&](const std::string& s) {
        if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
    };
    const std::string lo = lower(name);
    for (const std::string& n : {name, lo}) {
        add(n + kLibExt);
        add("lib" + n + kLibExt);       // conventional unix naming / MinGW default naming
    }
    return out;
}

fs::path PluginManager::resolve(const PluginRequest& req, std::string& tried_out) {
    std::error_code ec;
    tried_out.clear();
    auto note = [&](const fs::path& p) { tried_out += "\n    " + p.string(); };

    // Explicit path from t.plugins.name = "some/file.dll"
    if (!req.path.empty()) {
        fs::path p(req.path);
        if (!p.has_extension()) p += kLibExt;
        std::vector<fs::path> options;
        if (p.is_absolute()) options.push_back(p);
        else {
            options.push_back(fs::current_path(ec) / p);
            options.push_back(executable_dir() / p);
        }
        for (const auto& o : options) {
            note(o);
            if (fs::is_regular_file(o, ec)) return o;
        }
        return {};
    }

    const auto files = candidate_filenames(req.name);
    for (const fs::path& dir : search_dirs()) {
        for (const std::string& f : files) {
            const fs::path p = dir / f;
            if (fs::is_regular_file(p, ec)) return p;
        }
        note(dir / files.front());
    }
    return {};
}

// ---------------------------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------------------------
PluginManager::PluginManager() = default;
PluginManager::~PluginManager() { unload(); }

void PluginManager::fail(const std::string& key, const std::string& reason) {
    CRAYON_LOG_ERROR("Plugin '{}' not loaded: {}", key, reason);
    m_failures.push_back({key, reason});
}

bool PluginManager::is_loaded(const std::string& name) const {
    const std::string lo = lower(name);
    for (const PluginInfo& i : m_infos) {
        if (lower(i.name) == lo || lower(i.key) == lo) return true;
    }
    return false;
}

void PluginManager::load(const std::vector<PluginRequest>& requests, const CrayonHostAPI* host) {
    m_host = host;

    for (const PluginRequest& req : requests) {
        if (!valid_plugin_name(req.name)) {
            fail(req.name, "invalid plugin name (use letters, digits, '_', '-' and '.')");
            continue;
        }
        if (is_loaded(req.name)) continue;   // t.plugins listed twice

        std::string tried;
        fs::path file = resolve(req, tried);
        void* lib = nullptr;
        if (!file.empty()) {
            lib = open_library(file);
            if (!lib) {
                fail(req.name, "could not load '" + file.string() + "': " + last_os_error());
                continue;
            }
        }
#if defined(__ANDROID__)
        // Android only lets the app dlopen libraries from its own lib dir, by bare name.
        if (!lib && req.path.empty()) {
            for (const std::string& f : candidate_filenames(req.name)) {
                lib = dlopen(f.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (lib) { file = f; break; }
            }
        }
#endif
        if (!lib) {
            if (!req.path.empty()) fail(req.name, "library not found. Looked at:" + tried);
            else fail(req.name, "library not found. Put '" + candidate_filenames(req.name).front() +
                                "' next to the crayon binary (or in a plugins/ folder). Looked in:" + tried);
            continue;
        }

        auto entry = reinterpret_cast<CrayonPluginEntryFn>(find_symbol(lib, CRAYON_PLUGIN_ENTRY_NAME));
        if (!entry) {
            close_library(lib);
            fail(req.name, "'" + file.string() + "' is not a Crayon plugin (missing export '" CRAYON_PLUGIN_ENTRY_NAME "')");
            continue;
        }

        const CrayonPlugin* desc = nullptr;
        try { desc = entry(); } catch (const std::exception& e) {
            close_library(lib);
            fail(req.name, std::string("entry point threw: ") + e.what());
            continue;
        } catch (...) {
            close_library(lib);
            fail(req.name, "entry point threw an unknown exception");
            continue;
        }

        if (!desc || desc->struct_size < offsetof(CrayonPlugin, on_unload) + sizeof(desc->on_unload) || !desc->name || !desc->name[0]) {
            close_library(lib);
            fail(req.name, "plugin descriptor is missing or malformed");
            continue;
        }
        // `desc` lives inside the library: copy what we need before any close_library() below.
        const std::string plugin_name = desc->name;
        const uint32_t    plugin_api  = desc->api_version;

        if (plugin_api != CRAYON_PLUGIN_API_VERSION) {
            close_library(lib);
            fail(req.name, "plugin API version " + std::to_string(plugin_api) +
                           " does not match the engine's version " + std::to_string(CRAYON_PLUGIN_API_VERSION) +
                           " - rebuild the plugin against this engine's plugins/include/crayon_plugin.h");
            continue;
        }
        if (!valid_plugin_name(plugin_name)) {
            close_library(lib);
            fail(req.name, "plugin reports an invalid name");
            continue;
        }
        if (lower(plugin_name) != lower(req.name)) {
            CRAYON_LOG_WARN("Plugin file for t.plugins.{} calls itself '{}'; Lua code will see crayon.{}",
                            req.name, plugin_name, plugin_name);
        }
        {
            bool dup = false;
            for (const PluginInfo& i : m_infos) if (lower(i.name) == lower(plugin_name)) dup = true;
            if (dup) {
                close_library(lib);
                CRAYON_LOG_WARN("Plugin '{}' is already loaded; ignoring '{}'", plugin_name, file.string());
                continue;
            }
        }

        Slot slot;
        slot.handle = lib;
        slot.desc = desc;

        if (desc->on_load) {
            int rc = 1;
            try { rc = desc->on_load(host); } catch (...) { rc = 1; }
            if (rc != 0) {
                close_library(lib);
                fail(req.name, "plugin on_load() reported failure (code " + std::to_string(rc) + ")");
                continue;
            }
        }
        slot.loaded_ok = true;

        PluginInfo info;
        info.name = plugin_name;
        info.key = req.name;
        info.version = desc->version ? desc->version : "";
        info.description = desc->description ? desc->description : "";
        info.path = file.string();

        CRAYON_LOG_INFO("Plugin loaded: {} {} ({})", info.name, info.version, info.path);
        m_slots.push_back(slot);
        m_infos.push_back(std::move(info));
    }
}

void PluginManager::register_lua(lua_State* L) {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        s.active = false;
        int rc = 0;
        if (s.desc->on_register) {
            try { rc = s.desc->on_register(L); } catch (...) { rc = -1; }
        }
        if (rc != 0) {
            fail(m_infos[i].key, "plugin on_register() reported failure (code " + std::to_string(rc) + ")");
            continue;
        }
        s.active = true;
    }
}

void PluginManager::update(lua_State* L, float dt) {
    for (Slot& s : m_slots) {
        if (!s.active || !s.desc->on_update) continue;
        try { s.desc->on_update(L, dt); }
        catch (const std::exception& e) { CRAYON_LOG_ERROR("Plugin '{}' on_update threw: {}", s.desc->name, e.what()); }
        catch (...) { CRAYON_LOG_ERROR("Plugin '{}' on_update threw an unknown exception", s.desc->name); }
    }
}

void PluginManager::unload() {
    for (size_t i = m_slots.size(); i-- > 0;) {
        Slot& s = m_slots[i];
        if (s.loaded_ok && s.desc && s.desc->on_unload) {
            try { s.desc->on_unload(); } catch (...) { CRAYON_LOG_ERROR("Plugin '{}' on_unload threw", s.desc->name); }
        }
        close_library(s.handle);
        s.handle = nullptr;
        s.desc = nullptr;
    }
    m_slots.clear();
    m_infos.clear();
    m_failures.clear();
}

} // namespace crayon
