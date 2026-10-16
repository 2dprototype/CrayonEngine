// activation - Crayon Engine plugin: offline game activation (license keys), trials, feature flags.
//
//   function crayon.config(t) t.plugins.activation = true end
//   function crayon.init()
//       crayon.activation.configure{ product = "mygame", features = {"dlc1", "editor"}, trialDays = 7 }
//       if not crayon.activation.isAllowed() then ... show the key entry screen ... end
//   end
//
// See plugins/activation/README.md.

#include <lua.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "activation_core.hpp"
#include "crayon_plugin.h"

#ifndef CRAYON_ACTIVATION_SECRET
#define CRAYON_ACTIVATION_SECRET "CHANGE-ME-set-CRAYON_ACTIVATION_SECRET-in-cmake"
#endif

namespace fs = std::filesystem;
using namespace crayon::activation;

namespace {

const CrayonHostAPI* g_host = nullptr;

void hlog(int level, const std::string& msg) {
    if (CRAYON_HOST_HAS(g_host, log)) g_host->log(level, ("[activation] " + msg).c_str());
}

int64_t now_unix() { return static_cast<int64_t>(std::time(nullptr)); }

// ---------------------------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------------------------
struct State {
    bool configured = false;
    std::string product;
    std::vector<std::string> features;
    int trial_days = 0;

    // persisted (license.dat)
    std::string key;
    int64_t activated_at = 0;
    int64_t trial_start  = 0;
    int64_t last_seen    = 0;

    bool tampered = false;            // license.dat was modified / clock was turned back
    std::string machine_raw;
    Bytes       machine_hash_;
    fs::path    file;
    std::string last_status;
    float       timer = 0.0f;
};

State S;

std::string secret() { return CRAYON_ACTIVATION_SECRET; }

// ---------------------------------------------------------------------------------------------
// license.dat  (plain text + HMAC bound to this machine, so copying it elsewhere or editing it fails)
// ---------------------------------------------------------------------------------------------
fs::path data_root() {
#if defined(_WIN32)
    if (const char* a = std::getenv("APPDATA")) return fs::path(a);
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) return fs::path(h) / "Library" / "Application Support";
#else
    if (const char* x = std::getenv("XDG_DATA_HOME")) if (*x) return fs::path(x);
    if (const char* h = std::getenv("HOME")) return fs::path(h) / ".local" / "share";
#endif
    std::error_code ec;
    return fs::current_path(ec);
}

std::string state_mac(const std::string& body) {
    const std::string k = "store:" + S.machine_raw;
    const Bytes store_key_bytes = hmac_sha256(secret(), reinterpret_cast<const uint8_t*>(k.data()), k.size());
    const std::string store_key(store_key_bytes.begin(), store_key_bytes.end());
    const Bytes mac = hmac_sha256(store_key, reinterpret_cast<const uint8_t*>(body.data()), body.size());
    return to_hex(mac.data(), mac.size());
}

std::string state_body() {
    std::ostringstream o;
    o << "product=" << S.product << "\n"
      << "key=" << S.key << "\n"
      << "activated_at=" << S.activated_at << "\n"
      << "trial_start=" << S.trial_start << "\n"
      << "last_seen=" << S.last_seen << "\n";
    return o.str();
}

bool save_state() {
    std::error_code ec;
    fs::create_directories(S.file.parent_path(), ec);
    const std::string body = state_body();
    std::ofstream f(S.file, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << body << "mac=" << state_mac(body) << "\n";
    return bool(f);
}

void load_state() {
    S.key.clear(); S.activated_at = S.trial_start = S.last_seen = 0;
    std::ifstream f(S.file, std::ios::binary);
    if (!f) return;   // first run

    std::map<std::string, std::string> kv;
    std::string line, mac;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "mac") mac = v; else kv[k] = v;
    }
    State tmp;
    tmp.product = S.product; tmp.machine_raw = S.machine_raw;
    auto num = [&](const char* k) -> int64_t { auto it = kv.find(k); return it == kv.end() ? 0 : std::strtoll(it->second.c_str(), nullptr, 10); };
    auto str = [&](const char* k) -> std::string { auto it = kv.find(k); return it == kv.end() ? std::string() : it->second; };

    std::ostringstream o;
    o << "product=" << str("product") << "\n" << "key=" << str("key") << "\n"
      << "activated_at=" << num("activated_at") << "\n" << "trial_start=" << num("trial_start") << "\n"
      << "last_seen=" << num("last_seen") << "\n";
    if (mac.empty() || mac != state_mac(o.str()) || str("product") != S.product) {
        S.tampered = true;
        hlog(CRAYON_LOG_LEVEL_WARN, "license.dat failed its integrity check");
        return;
    }
    S.key = str("key");
    S.activated_at = num("activated_at");
    S.trial_start = num("trial_start");
    S.last_seen = num("last_seen");
}

// ---------------------------------------------------------------------------------------------
// evaluation
// ---------------------------------------------------------------------------------------------
struct Eval {
    std::string status = "unlicensed";   // activated | trial | expired | trial_expired | unlicensed | tampered
    bool has_license = false;
    License lic;
    int trial_days_left = 0;
};

// Moves "now" forward only; a clock turned back by more than a day is reported as tampering.
int64_t effective_now() {
    const int64_t n = now_unix();
    if (S.last_seen > 0 && n < S.last_seen - 86400) S.tampered = true;
    return std::max(n, S.last_seen);
}

Eval evaluate() {
    Eval e;
    const int64_t now = effective_now();
    if (S.tampered) { e.status = "tampered"; return e; }

    if (!S.key.empty()) {
        License lic;
        if (parse_key(secret(), S.product, S.key, lic) == KeyStatus::Ok &&
            (lic.machine == 0 || lic.machine == machine_tag(S.machine_hash_))) {
            e.has_license = true;
            e.lic = lic;
            e.status = (lic.expiry_day != 0 && day_of(now) > lic.expiry_day) ? "expired" : "activated";
            return e;
        }
        S.tampered = true;   // a stored key that no longer verifies can only come from editing
        e.status = "tampered";
        return e;
    }
    if (S.trial_days > 0 && S.trial_start > 0) {
        const double used = double(now - S.trial_start) / 86400.0;
        e.trial_days_left = std::max(0, int(std::ceil(double(S.trial_days) - used)));
        e.status = used < S.trial_days ? "trial" : "trial_expired";
    }
    return e;
}

void touch_last_seen(bool force = false) {
    if (!S.configured || S.tampered) return;
    const int64_t n = now_unix();
    if (force || n > S.last_seen + 3600) { S.last_seen = std::max(S.last_seen, n); save_state(); }
}

// ---------------------------------------------------------------------------------------------
// Lua helpers
// ---------------------------------------------------------------------------------------------
void require_configured(lua_State* L) {
    if (!S.configured) luaL_error(L, "crayon.activation.configure{product=\"...\"} must be called first");
}

void push_info(lua_State* L, const Eval& e) {
    lua_newtable(L);
    lua_pushstring(L, e.status.c_str());       lua_setfield(L, -2, "status");
    lua_pushinteger(L, e.lic.edition);         lua_setfield(L, -2, "edition");
    lua_pushnumber(L, e.lic.serial);           lua_setfield(L, -2, "serial");
    lua_pushboolean(L, e.lic.machine != 0);    lua_setfield(L, -2, "machineBound");
    lua_pushnumber(L, double(S.activated_at)); lua_setfield(L, -2, "activatedAt");
    if (e.lic.expiry_day != 0) {
        const int64_t end = kEpoch2020 + (int64_t(e.lic.expiry_day) + 1) * 86400;   // end of the last valid day
        lua_pushnumber(L, double(end));        lua_setfield(L, -2, "expires");
        lua_pushinteger(L, int(std::max<int64_t>(0, e.lic.expiry_day - day_of(effective_now()))));
        lua_setfield(L, -2, "daysLeft");
    }
    lua_newtable(L);   // features: { dlc1 = true, ... }
    for (size_t i = 0; i < S.features.size() && i < 32; ++i) {
        if (e.lic.features & (1u << i)) { lua_pushboolean(L, 1); lua_setfield(L, -2, S.features[i].c_str()); }
    }
    lua_setfield(L, -2, "features");
}

int fail3(lua_State* L, const char* code, const char* message) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, code);
    lua_pushstring(L, message);
    return 3;
}

// ---------------------------------------------------------------------------------------------
// crayon.activation.*
// ---------------------------------------------------------------------------------------------

// configure{ product = "mygame", features = {"dlc1", ...}, trialDays = 0 } -> true
int l_configure(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    State fresh;
    lua_getfield(L, 1, "product");
    if (lua_type(L, -1) != LUA_TSTRING || lua_objlen(L, -1) == 0) luaL_error(L, "configure: 'product' (string) is required");
    fresh.product = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "features");
    if (lua_istable(L, -1)) {
        const int n = int(lua_objlen(L, -1));
        if (n > 32) luaL_error(L, "configure: at most 32 features are supported");
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "configure: features must be strings");
            fresh.features.push_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "trialDays");
    if (lua_isnumber(L, -1)) fresh.trial_days = std::max(0, int(lua_tonumber(L, -1)));
    lua_pop(L, 1);

    fresh.configured = true;
    fresh.machine_raw = raw_machine_id();
    fresh.machine_hash_ = machine_hash(fresh.machine_raw);
    fresh.file = data_root() / "CrayonActivation" / fresh.product / "license.dat";
    S = fresh;

    load_state();
    // The trial starts the first time a game with trialDays > 0 runs without a license.
    if (!S.tampered && S.key.empty() && S.trial_days > 0 && S.trial_start == 0) {
        S.trial_start = now_unix();
        S.last_seen = S.trial_start;
        if (!save_state()) hlog(CRAYON_LOG_LEVEL_WARN, "could not write " + S.file.string());
    }
    touch_last_seen();
    S.last_status = evaluate().status;
    hlog(CRAYON_LOG_LEVEL_INFO, "product '" + S.product + "': " + S.last_status);
    lua_pushboolean(L, 1);
    return 1;
}

// getMachineId() -> "A1B2-C3D4-E5F6-0718"   (send this to the developer for machine-bound keys)
int l_get_machine_id(lua_State* L) {
    const Bytes h = machine_hash(S.configured ? S.machine_raw : raw_machine_id());
    lua_pushstring(L, machine_display_id(h).c_str());
    return 1;
}

// activate(key) -> true, info   |   false, code, message
int l_activate(lua_State* L) {
    require_configured(L);
    const std::string key = luaL_checkstring(L, 1);
    if (S.tampered) return fail3(L, "tampered", "License data was tampered with. Delete license.dat to reset.");

    License lic;
    switch (parse_key(secret(), S.product, key, lic)) {
        case KeyStatus::Ok: break;
        case KeyStatus::Malformed:     return fail3(L, "malformed", "That is not a valid key format.");
        case KeyStatus::BadSignature:  return fail3(L, "bad_signature", "Invalid key.");
        case KeyStatus::WrongProduct:  return fail3(L, "wrong_product", "This key is for a different product.");
    }
    if (lic.machine != 0 && lic.machine != machine_tag(S.machine_hash_))
        return fail3(L, "wrong_machine", "This key is bound to a different computer.");
    const int64_t now = effective_now();
    if (lic.expiry_day != 0 && day_of(now) > lic.expiry_day) return fail3(L, "expired", "This key has expired.");

    const std::string old_key = S.key; const int64_t old_at = S.activated_at;
    S.key = normalize_key(key);
    S.activated_at = now;
    S.last_seen = std::max(S.last_seen, now);
    if (!save_state()) {
        S.key = old_key; S.activated_at = old_at;
        return fail3(L, "io", ("Could not write " + S.file.string()).c_str());
    }
    const Eval e = evaluate();
    S.last_status = e.status;
    lua_pushboolean(L, 1);
    push_info(L, e);
    return 2;
}

// deactivate() -> true   (forgets the key on this machine; the trial is NOT restarted)
int l_deactivate(lua_State* L) {
    require_configured(L);
    S.key.clear(); S.activated_at = 0;
    S.last_seen = std::max(S.last_seen, now_unix());
    const bool ok = save_state();
    S.last_status = evaluate().status;
    lua_pushboolean(L, ok);
    return 1;
}

int l_get_status(lua_State* L) { require_configured(L); lua_pushstring(L, evaluate().status.c_str()); return 1; }
int l_is_activated(lua_State* L) { require_configured(L); lua_pushboolean(L, evaluate().status == "activated"); return 1; }
int l_is_trial(lua_State* L) { require_configured(L); lua_pushboolean(L, evaluate().status == "trial"); return 1; }

// isAllowed() -> true when the game may run: activated, or inside the trial
int l_is_allowed(lua_State* L) {
    require_configured(L);
    const std::string s = evaluate().status;
    lua_pushboolean(L, s == "activated" || s == "trial");
    return 1;
}

int l_get_trial_days_left(lua_State* L) { require_configured(L); lua_pushinteger(L, evaluate().trial_days_left); return 1; }

// getInfo() -> table | nil   (nil when no valid license is stored)
int l_get_info(lua_State* L) {
    require_configured(L);
    const Eval e = evaluate();
    if (!e.has_license) { lua_pushnil(L); return 1; }
    push_info(L, e);
    return 1;
}

// hasFeature(name) -> bool   (only an activated license unlocks features)
int l_has_feature(lua_State* L) {
    require_configured(L);
    const std::string name = luaL_checkstring(L, 1);
    const Eval e = evaluate();
    bool has = false;
    if (e.status == "activated") {
        for (size_t i = 0; i < S.features.size() && i < 32; ++i)
            if (S.features[i] == name && (e.lic.features & (1u << i))) has = true;
    }
    lua_pushboolean(L, has);
    return 1;
}

// getEdition() -> integer (0 when not activated)
int l_get_edition(lua_State* L) {
    require_configured(L);
    const Eval e = evaluate();
    lua_pushinteger(L, e.status == "activated" ? e.lic.edition : 0);
    return 1;
}

// getLicensePath() -> string
int l_get_path(lua_State* L) { require_configured(L); lua_pushstring(L, S.file.string().c_str()); return 1; }

// ---------------------------------------------------------------------------------------------
// plugin callbacks
// ---------------------------------------------------------------------------------------------
int p_on_load(const CrayonHostAPI* host) {
    g_host = host;
    if (secret().rfind("CHANGE-ME", 0) == 0)
        hlog(CRAYON_LOG_LEVEL_WARN, "built with the default secret - anyone can generate keys. Rebuild with -DCRAYON_ACTIVATION_SECRET=...");
    return 0;
}

int p_on_register(lua_State* L) {
    S = State{};   // fresh Lua state -> game must call configure() again

    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return 1; }
    lua_newtable(L);
    struct Entry { const char* name; lua_CFunction fn; };
    static const Entry entries[] = {
        {"configure", l_configure}, {"getMachineId", l_get_machine_id}, {"activate", l_activate},
        {"deactivate", l_deactivate}, {"getStatus", l_get_status}, {"isActivated", l_is_activated},
        {"isTrial", l_is_trial}, {"isAllowed", l_is_allowed}, {"getTrialDaysLeft", l_get_trial_days_left},
        {"getInfo", l_get_info}, {"hasFeature", l_has_feature}, {"getEdition", l_get_edition},
        {"getLicensePath", l_get_path},
    };
    for (const Entry& e : entries) { lua_pushcfunction(L, e.fn); lua_setfield(L, -2, e.name); }
    lua_setfield(L, -2, "activation");
    lua_pop(L, 1);
    return 0;
}

// Every 30 s: re-check expiry / trial end while the game runs and fire
// crayon.onActivationChanged(newStatus, oldStatus) when the status changes.
void p_on_update(lua_State* L, float dt) {
    if (!S.configured) return;
    S.timer += dt;
    if (S.timer < 30.0f) return;
    S.timer = 0.0f;

    touch_last_seen();
    const std::string now_status = evaluate().status;
    if (now_status == S.last_status) return;
    const std::string old = S.last_status;
    S.last_status = now_status;

    lua_getglobal(L, "crayon");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "onActivationChanged");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, now_status.c_str());
            lua_pushstring(L, old.c_str());
            if (lua_pcall(L, 2, 0, 0) != 0) {
                const char* err = lua_tostring(L, -1);
                hlog(CRAYON_LOG_LEVEL_ERROR, std::string("Error in crayon.onActivationChanged(): ") + (err ? err : "?"));
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

void p_on_unload(void) {
    if (S.configured && !S.tampered) touch_last_seen(true);
    S = State{};
    g_host = nullptr;
}

const CrayonPlugin k_plugin = {
    sizeof(CrayonPlugin), CRAYON_PLUGIN_API_VERSION,
    "activation", "1.0.0",
    "Offline game activation: signed license keys, machine binding, expiry, trial, feature flags",
    p_on_load, p_on_register, p_on_update, p_on_unload,
};

} // namespace

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) { return &k_plugin; }
