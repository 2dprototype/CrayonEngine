#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../core/audio_system.hpp"

namespace crayon {

static int l_audio_load_sound(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t id = Engine::get().get_audio().load_sound(path);
    lua_pushinteger(L, id);
    return 1;
}

static int l_audio_unload_sound(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool res = Engine::get().get_audio().unload_sound(id);
    lua_pushboolean(L, res);
    return 1;
}

static int l_audio_play_sound(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool loop = false;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "volume");
        if (!lua_isnil(L, -1)) volume = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 2, "pitch");
        if (!lua_isnil(L, -1)) pitch = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 2, "pan");
        if (!lua_isnil(L, -1)) pan = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 2, "loop");
        if (!lua_isnil(L, -1)) loop = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    } else {
        if (lua_isnumber(L, 2)) volume = static_cast<float>(lua_tonumber(L, 2));
        if (lua_isnumber(L, 3)) pitch = static_cast<float>(lua_tonumber(L, 3));
        if (lua_isnumber(L, 4)) pan = static_cast<float>(lua_tonumber(L, 4));
        if (lua_isboolean(L, 5)) loop = lua_toboolean(L, 5) != 0;
    }

    uint32_t voice_id = Engine::get().get_audio().play_sound(id, volume, pitch, pan, loop);
    lua_pushinteger(L, voice_id);
    return 1;
}

static int l_audio_play_sound_3d(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));

    float volume = 1.0f;
    float pitch = 1.0f;
    float min_dist = 1.0f;
    float max_dist = 25.0f;

    if (lua_istable(L, 5)) {
        lua_getfield(L, 5, "volume");
        if (!lua_isnil(L, -1)) volume = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 5, "pitch");
        if (!lua_isnil(L, -1)) pitch = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 5, "minDist");
        if (!lua_isnil(L, -1)) min_dist = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 5, "maxDist");
        if (!lua_isnil(L, -1)) max_dist = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }

    uint32_t voice_id = Engine::get().get_audio().play_sound_3d(id, glm::vec3(x, y, z), volume, pitch, min_dist, max_dist);
    lua_pushinteger(L, voice_id);
    return 1;
}

static int l_audio_stop_sound(lua_State* L) {
    uint32_t voice_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    Engine::get().get_audio().stop_sound(voice_id);
    return 0;
}

static int l_audio_stop_all_sounds(lua_State* L) {
    (void)L;
    Engine::get().get_audio().stop_all_sounds();
    return 0;
}

static int l_audio_is_sound_playing(lua_State* L) {
    uint32_t voice_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, Engine::get().get_audio().is_sound_playing(voice_id));
    return 1;
}

static int l_audio_play_music(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool loop = true;
    float fade_in = 0.0f;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "loop");
        if (!lua_isnil(L, -1)) loop = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);

        lua_getfield(L, 2, "fadeIn");
        if (!lua_isnil(L, -1)) fade_in = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    } else {
        if (lua_isboolean(L, 2)) loop = lua_toboolean(L, 2) != 0;
        if (lua_isnumber(L, 3)) fade_in = static_cast<float>(lua_tonumber(L, 3));
    }

    bool res = Engine::get().get_audio().play_music(path, loop, fade_in);
    lua_pushboolean(L, res);
    return 1;
}

static int l_audio_stop_music(lua_State* L) {
    float fade_out = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    Engine::get().get_audio().stop_music(fade_out);
    return 0;
}

static int l_audio_pause_music(lua_State* L) {
    (void)L;
    Engine::get().get_audio().pause_music();
    return 0;
}

static int l_audio_resume_music(lua_State* L) {
    (void)L;
    Engine::get().get_audio().resume_music();
    return 0;
}

static int l_audio_is_music_playing(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_audio().is_music_playing());
    return 1;
}

static int l_audio_set_master_volume(lua_State* L) {
    float v = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_audio().set_master_volume(v);
    return 0;
}

static int l_audio_get_master_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_master_volume());
    return 1;
}

static int l_audio_set_music_volume(lua_State* L) {
    float v = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_audio().set_music_volume(v);
    return 0;
}

static int l_audio_get_music_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_music_volume());
    return 1;
}

static int l_audio_set_sfx_volume(lua_State* L) {
    float v = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_audio().set_sfx_volume(v);
    return 0;
}

static int l_audio_get_sfx_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_sfx_volume());
    return 1;
}

static int l_audio_set_listener_position(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    Engine::get().get_audio().set_listener_position(glm::vec3(x, y, z));
    return 0;
}

static int l_audio_set_listener_orientation(lua_State* L) {
    float fx = static_cast<float>(luaL_checknumber(L, 1));
    float fy = static_cast<float>(luaL_checknumber(L, 2));
    float fz = static_cast<float>(luaL_checknumber(L, 3));
    float ux = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float uy = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float uz = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    Engine::get().get_audio().set_listener_orientation(glm::vec3(fx, fy, fz), glm::vec3(ux, uy, uz));
    return 0;
}

void register_audio_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_audio_load_sound);
    lua_setfield(L, -2, "loadSound");

    lua_pushcfunction(L, l_audio_unload_sound);
    lua_setfield(L, -2, "unloadSound");

    lua_pushcfunction(L, l_audio_play_sound);
    lua_setfield(L, -2, "playSound");

    lua_pushcfunction(L, l_audio_play_sound_3d);
    lua_setfield(L, -2, "playSound3D");

    lua_pushcfunction(L, l_audio_stop_sound);
    lua_setfield(L, -2, "stopSound");

    lua_pushcfunction(L, l_audio_stop_all_sounds);
    lua_setfield(L, -2, "stopAllSounds");

    lua_pushcfunction(L, l_audio_is_sound_playing);
    lua_setfield(L, -2, "isSoundPlaying");

    lua_pushcfunction(L, l_audio_play_music);
    lua_setfield(L, -2, "playMusic");

    lua_pushcfunction(L, l_audio_stop_music);
    lua_setfield(L, -2, "stopMusic");

    lua_pushcfunction(L, l_audio_pause_music);
    lua_setfield(L, -2, "pauseMusic");

    lua_pushcfunction(L, l_audio_resume_music);
    lua_setfield(L, -2, "resumeMusic");

    lua_pushcfunction(L, l_audio_is_music_playing);
    lua_setfield(L, -2, "isMusicPlaying");

    lua_pushcfunction(L, l_audio_set_master_volume);
    lua_setfield(L, -2, "setMasterVolume");

    lua_pushcfunction(L, l_audio_get_master_volume);
    lua_setfield(L, -2, "getMasterVolume");

    lua_pushcfunction(L, l_audio_set_music_volume);
    lua_setfield(L, -2, "setMusicVolume");

    lua_pushcfunction(L, l_audio_get_music_volume);
    lua_setfield(L, -2, "getMusicVolume");

    lua_pushcfunction(L, l_audio_set_sfx_volume);
    lua_setfield(L, -2, "setSfxVolume");

    lua_pushcfunction(L, l_audio_get_sfx_volume);
    lua_setfield(L, -2, "getSfxVolume");

    lua_pushcfunction(L, l_audio_set_listener_position);
    lua_setfield(L, -2, "setListenerPosition");

    lua_pushcfunction(L, l_audio_set_listener_orientation);
    lua_setfield(L, -2, "setListenerOrientation");

    lua_setfield(L, -2, "audio");
    lua_pop(L, 1); // pop crayon
}

} // namespace crayon
