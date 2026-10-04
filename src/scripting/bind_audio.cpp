#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../core/audio_system.hpp"

namespace crayon {

// ==================================================================
//  Existing bindings (unchanged behavior)
// ==================================================================

static int l_audio_load_sound(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t id = Engine::get().get_audio().load_sound(path);
    lua_pushinteger(L, id);
    return 1;
}

static int l_audio_unload_sound(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, Engine::get().get_audio().unload_sound(id));
    return 1;
}

static int l_audio_play_sound(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float volume = 1.0f, pitch = 1.0f, pan = 0.0f;
    bool loop = false;
    int priority = 0;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "volume");   if (!lua_isnil(L, -1)) volume = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 2, "pitch");    if (!lua_isnil(L, -1)) pitch  = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 2, "pan");      if (!lua_isnil(L, -1)) pan    = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 2, "loop");     if (!lua_isnil(L, -1)) loop   = lua_toboolean(L, -1) != 0;   lua_pop(L, 1);
        lua_getfield(L, 2, "priority"); if (!lua_isnil(L, -1)) priority = (int)lua_tointeger(L, -1);  lua_pop(L, 1);
    } else {
        if (lua_isnumber(L, 2))  volume = (float)lua_tonumber(L, 2);
        if (lua_isnumber(L, 3))  pitch  = (float)lua_tonumber(L, 3);
        if (lua_isnumber(L, 4))  pan    = (float)lua_tonumber(L, 4);
        if (lua_isboolean(L, 5)) loop   = lua_toboolean(L, 5) != 0;
    }

    uint32_t vid = Engine::get().get_audio().play_sound(id, volume, pitch, pan, loop);
    if (vid != 0 && priority != 0) {
        Engine::get().get_audio().set_voice_priority(vid, priority);
    }
    lua_pushinteger(L, vid);
    return 1;
}

static int l_audio_play_sound_3d(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_checknumber(L, 4);

    float volume = 1.0f, pitch = 1.0f, min_dist = 1.0f, max_dist = 25.0f;
    if (lua_istable(L, 5)) {
        lua_getfield(L, 5, "volume");  if (!lua_isnil(L, -1)) volume   = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 5, "pitch");   if (!lua_isnil(L, -1)) pitch    = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 5, "minDist"); if (!lua_isnil(L, -1)) min_dist = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, 5, "maxDist"); if (!lua_isnil(L, -1)) max_dist = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    }

    uint32_t vid = Engine::get().get_audio().play_sound_3d(
        id, glm::vec3(x, y, z), volume, pitch, min_dist, max_dist);
    lua_pushinteger(L, vid);
    return 1;
}

static int l_audio_stop_sound(lua_State* L) {
    Engine::get().get_audio().stop_sound((uint32_t)luaL_checkinteger(L, 1));
    return 0;
}

static int l_audio_stop_all_sounds(lua_State* L) {
    (void)L;
    Engine::get().get_audio().stop_all_sounds();
    return 0;
}

static int l_audio_is_sound_playing(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_audio().is_sound_playing((uint32_t)luaL_checkinteger(L, 1)));
    return 1;
}

static int l_audio_play_music(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    bool loop = true;
    float fade_in = 0.0f;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "loop");   if (!lua_isnil(L, -1)) loop    = lua_toboolean(L, -1) != 0;   lua_pop(L, 1);
        lua_getfield(L, 2, "fadeIn"); if (!lua_isnil(L, -1)) fade_in = (float)lua_tonumber(L, -1);  lua_pop(L, 1);
    } else {
        if (lua_isboolean(L, 2)) loop = lua_toboolean(L, 2) != 0;
        if (lua_isnumber(L, 3))  fade_in = (float)lua_tonumber(L, 3);
    }

    lua_pushboolean(L, Engine::get().get_audio().play_music(path, loop, fade_in));
    return 1;
}

static int l_audio_stop_music(lua_State* L) {
    float fade_out = (float)luaL_optnumber(L, 1, 0.0);
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
    Engine::get().get_audio().set_master_volume((float)luaL_checknumber(L, 1));
    return 0;
}
static int l_audio_get_master_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_master_volume()); return 1;
}
static int l_audio_set_music_volume(lua_State* L) {
    Engine::get().get_audio().set_music_volume((float)luaL_checknumber(L, 1));
    return 0;
}
static int l_audio_get_music_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_music_volume()); return 1;
}
static int l_audio_set_sfx_volume(lua_State* L) {
    Engine::get().get_audio().set_sfx_volume((float)luaL_checknumber(L, 1));
    return 0;
}
static int l_audio_get_sfx_volume(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_audio().get_sfx_volume()); return 1;
}

static int l_audio_set_listener_position(lua_State* L) {
    Engine::get().get_audio().set_listener_position(glm::vec3(
        (float)luaL_checknumber(L, 1),
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3)));
    return 0;
}

static int l_audio_set_listener_orientation(lua_State* L) {
    Engine::get().get_audio().set_listener_orientation(
        glm::vec3((float)luaL_checknumber(L, 1),
                  (float)luaL_checknumber(L, 2),
                  (float)luaL_checknumber(L, 3)),
        glm::vec3((float)luaL_optnumber(L, 4, 0.0),
                  (float)luaL_optnumber(L, 5, 1.0),
                  (float)luaL_optnumber(L, 6, 0.0)));
    return 0;
}

// ==================================================================
//  NEW: procedural sound creation
// ==================================================================

static WaveType parse_wave_type(const char* name) {
    if (!name) return WaveType::Sine;
    std::string s(name);
    if (s == "sample")   return WaveType::Sample;
    if (s == "sine")     return WaveType::Sine;
    if (s == "square")   return WaveType::Square;
    if (s == "saw")      return WaveType::Saw;
    if (s == "triangle") return WaveType::Triangle;
    if (s == "noise")    return WaveType::Noise;
    return WaveType::Sine;
}

static int l_audio_create_sound(lua_State* L) {
    if (!lua_istable(L, 1)) return luaL_error(L, "createSound expects a table");

    SoundTemplate tmpl;

    lua_getfield(L, 1, "wave");
    if (lua_isstring(L, -1)) tmpl.wave = parse_wave_type(lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "freq");     if (lua_isnumber(L, -1)) tmpl.freq = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    lua_getfield(L, 1, "duration"); if (lua_isnumber(L, -1)) tmpl.duration = (float)lua_tonumber(L, -1); lua_pop(L, 1);

    lua_getfield(L, 1, "envelope");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "attack");  if (lua_isnumber(L, -1)) tmpl.adsr.attack  = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "decay");   if (lua_isnumber(L, -1)) tmpl.adsr.decay   = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "sustain"); if (lua_isnumber(L, -1)) tmpl.adsr.sustain = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "release"); if (lua_isnumber(L, -1)) tmpl.adsr.release = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "filter");
    if (lua_istable(L, -1)) {
        tmpl.filter_enabled = true;
        lua_getfield(L, -1, "cutoff");   if (lua_isnumber(L, -1)) tmpl.filter_cutoff   = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "highpass"); if (lua_isboolean(L, -1)) tmpl.filter_highpass = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
        lua_getfield(L, -1, "type");
        if (lua_isstring(L, -1) && std::string(lua_tostring(L, -1)) == "highpass")
            tmpl.filter_highpass = true;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "pitchSweep");
    if (lua_istable(L, -1)) {
        tmpl.pitch_sweep = true;
        lua_getfield(L, -1, "from"); if (lua_isnumber(L, -1)) tmpl.sweep_from = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "to");   if (lua_isnumber(L, -1)) tmpl.sweep_to   = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, -1, "time"); if (lua_isnumber(L, -1)) tmpl.sweep_time = (float)lua_tonumber(L, -1); lua_pop(L, 1);
    }
    lua_pop(L, 1);

    uint32_t id = Engine::get().get_audio().create_sound(tmpl);
    lua_pushinteger(L, id);
    return 1;
}

// --- Baked procedural buffer ---
struct BufferGenCtx {
    lua_State* L;
    int func_ref;
};

static int l_audio_create_buffer(lua_State* L) {
    int frames = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Store function in registry
    lua_pushvalue(L, 2);
    int func_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    BufferGenCtx ctx{ L, func_ref };

    uint32_t id = Engine::get().get_audio().create_buffer(
        frames,
        [&ctx](float t, int i) -> float {
            lua_rawgeti(ctx.L, LUA_REGISTRYINDEX, ctx.func_ref);  // +1
            lua_pushnumber(ctx.L, t);                             // +1
            lua_pushinteger(ctx.L, i);                            // +1
            if (lua_pcall(ctx.L, 2, 1, 0) != 0) {                 // pops 3, pushes 1
                lua_pop(ctx.L, 1);                                // -1  ✓
                return 0.0f;
            }
            float v = (float)lua_tonumber(ctx.L, -1);
            lua_pop(ctx.L, 1);                                    // -1  ✓
            return v;
        });
        
    luaL_unref(L, LUA_REGISTRYINDEX, func_ref);
    lua_pushinteger(L, id);
    return 1;
}

// ==================================================================
//  NEW: live voice control
// ==================================================================

static int l_audio_set_voice_gain(lua_State* L) {
    Engine::get().get_audio().set_voice_gain(
        (uint32_t)luaL_checkinteger(L, 1),
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3));
    return 0;
}
static int l_audio_set_voice_volume(lua_State* L) {
    Engine::get().get_audio().set_voice_volume(
        (uint32_t)luaL_checkinteger(L, 1),
        (float)luaL_checknumber(L, 2));
    return 0;
}
static int l_audio_set_voice_pitch(lua_State* L) {
    Engine::get().get_audio().set_voice_pitch(
        (uint32_t)luaL_checkinteger(L, 1),
        (float)luaL_checknumber(L, 2));
    return 0;
}
static int l_audio_set_voice_pan(lua_State* L) {
    Engine::get().get_audio().set_voice_pan(
        (uint32_t)luaL_checkinteger(L, 1),
        (float)luaL_checknumber(L, 2));
    return 0;
}
static int l_audio_set_voice_position(lua_State* L) {
    Engine::get().get_audio().set_voice_position(
        (uint32_t)luaL_checkinteger(L, 1),
        glm::vec3((float)luaL_checknumber(L, 2),
                  (float)luaL_checknumber(L, 3),
                  (float)luaL_checknumber(L, 4)));
    return 0;
}
static int l_audio_set_voice_priority(lua_State* L) {
    Engine::get().get_audio().set_voice_priority(
        (uint32_t)luaL_checkinteger(L, 1),
        (int)luaL_checkinteger(L, 2));
    return 0;
}
static int l_audio_set_voice_loop(lua_State* L) {
    Engine::get().get_audio().set_voice_loop(
        (uint32_t)luaL_checkinteger(L, 1),
        lua_toboolean(L, 2) != 0);
    return 0;
}

// ==================================================================
//  NEW: custom Lua block generator
// ==================================================================

static int l_audio_set_block_generator(lua_State* L) {
    uint32_t voice_id  = (uint32_t)luaL_checkinteger(L, 1);
    int      blockSize = (int)luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    Engine::get().get_audio().set_custom_gen(voice_id, ref, blockSize);
    lua_pushinteger(L, ref);
    return 1;
}

static int l_audio_clear_block_generator(lua_State* L) {
    uint32_t voice_id = (uint32_t)luaL_checkinteger(L, 1);

    Engine::get().get_audio().clear_custom_gen(voice_id);

    // If the user supplied a ref to unref, do it
    if (lua_isnumber(L, 2)) {
        int ref = (int)lua_tointeger(L, 2);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    return 0;
}

// ==================================================================
//  NEW: stats
// ==================================================================

static int l_audio_get_active_voice_count(lua_State* L) {
    lua_pushinteger(L, Engine::get().get_audio().get_active_voice_count());
    return 1;
}
static int l_audio_get_max_voices(lua_State* L) {
    lua_pushinteger(L, Engine::get().get_audio().get_max_voices());
    return 1;
}

// ==================================================================
//  Registration
// ==================================================================

void register_audio_bindings(lua_State* L) {
    // --- Set the custom generator callback so mix() can call into Lua ---
Engine::get().get_audio().set_custom_gen_callback(
    [L](int lua_ref, float* out, int frames, double t_start, int sr) {

        if (lua_ref < 0) {
            for (int i = 0; i < frames; ++i) out[i] = 0.0f;
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);       // +1
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);                                // -1  ✓
            for (int i = 0; i < frames; ++i) out[i] = 0.0f;
            return;
        }

        lua_pushnumber(L, t_start);                       // +1
        lua_pushinteger(L, sr);                           // +1
        lua_pushinteger(L, frames);                       // +1

        if (lua_pcall(L, 3, 1, 0) != 0) {                 // pops 4, pushes 1
            lua_pop(L, 1);                                // -1  ✓  (errmsg)
            for (int i = 0; i < frames; ++i) out[i] = 0.0f;
            return;
        }

        if (!lua_istable(L, -1)) {                        // stack: [table]
            lua_pop(L, 1);                                // -1  ✓
            for (int i = 0; i < frames; ++i) out[i] = 0.0f;
            return;
        }

        for (int i = 0; i < frames; ++i) {
            lua_rawgeti(L, -1, i + 1);                    // +1
            out[i] = (float)lua_tonumber(L, -1);
            lua_pop(L, 1);                                // -1
        }
        lua_pop(L, 1);   // was lua_pop(L, 2) — only the table is on the stack
    });
    
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    // --- existing bindings ---
    lua_pushcfunction(L, l_audio_load_sound);        lua_setfield(L, -2, "loadSound");
    lua_pushcfunction(L, l_audio_unload_sound);      lua_setfield(L, -2, "unloadSound");
    lua_pushcfunction(L, l_audio_play_sound);        lua_setfield(L, -2, "playSound");
    lua_pushcfunction(L, l_audio_play_sound_3d);     lua_setfield(L, -2, "playSound3D");
    lua_pushcfunction(L, l_audio_stop_sound);        lua_setfield(L, -2, "stopSound");
    lua_pushcfunction(L, l_audio_stop_all_sounds);   lua_setfield(L, -2, "stopAllSounds");
    lua_pushcfunction(L, l_audio_is_sound_playing);  lua_setfield(L, -2, "isSoundPlaying");
    lua_pushcfunction(L, l_audio_play_music);        lua_setfield(L, -2, "playMusic");
    lua_pushcfunction(L, l_audio_stop_music);        lua_setfield(L, -2, "stopMusic");
    lua_pushcfunction(L, l_audio_pause_music);       lua_setfield(L, -2, "pauseMusic");
    lua_pushcfunction(L, l_audio_resume_music);      lua_setfield(L, -2, "resumeMusic");
    lua_pushcfunction(L, l_audio_is_music_playing);  lua_setfield(L, -2, "isMusicPlaying");
    lua_pushcfunction(L, l_audio_set_master_volume); lua_setfield(L, -2, "setMasterVolume");
    lua_pushcfunction(L, l_audio_get_master_volume); lua_setfield(L, -2, "getMasterVolume");
    lua_pushcfunction(L, l_audio_set_music_volume);  lua_setfield(L, -2, "setMusicVolume");
    lua_pushcfunction(L, l_audio_get_music_volume);  lua_setfield(L, -2, "getMusicVolume");
    lua_pushcfunction(L, l_audio_set_sfx_volume);    lua_setfield(L, -2, "setSfxVolume");
    lua_pushcfunction(L, l_audio_get_sfx_volume);    lua_setfield(L, -2, "getSfxVolume");
    lua_pushcfunction(L, l_audio_set_listener_position);    lua_setfield(L, -2, "setListenerPosition");
    lua_pushcfunction(L, l_audio_set_listener_orientation); lua_setfield(L, -2, "setListenerOrientation");

    // --- new: procedural creation ---
    lua_pushcfunction(L, l_audio_create_sound);   lua_setfield(L, -2, "createSound");
    lua_pushcfunction(L, l_audio_create_buffer);  lua_setfield(L, -2, "createBuffer");

    // --- new: live voice control ---
    lua_pushcfunction(L, l_audio_set_voice_gain);     lua_setfield(L, -2, "setVoiceGain");
    lua_pushcfunction(L, l_audio_set_voice_volume);   lua_setfield(L, -2, "setVoiceVolume");
    lua_pushcfunction(L, l_audio_set_voice_pitch);    lua_setfield(L, -2, "setVoicePitch");
    lua_pushcfunction(L, l_audio_set_voice_pan);      lua_setfield(L, -2, "setVoicePan");
    lua_pushcfunction(L, l_audio_set_voice_position); lua_setfield(L, -2, "setVoicePosition");
    lua_pushcfunction(L, l_audio_set_voice_priority); lua_setfield(L, -2, "setVoicePriority");
    lua_pushcfunction(L, l_audio_set_voice_loop);     lua_setfield(L, -2, "setVoiceLoop");

    // --- new: block generator ---
    lua_pushcfunction(L, l_audio_set_block_generator);   lua_setfield(L, -2, "setBlockGenerator");
    lua_pushcfunction(L, l_audio_clear_block_generator); lua_setfield(L, -2, "clearBlockGenerator");

    // --- new: stats ---
    lua_pushcfunction(L, l_audio_get_active_voice_count); lua_setfield(L, -2, "getActiveVoiceCount");
    lua_pushcfunction(L, l_audio_get_max_voices);         lua_setfield(L, -2, "getMaxVoices");

    lua_setfield(L, -2, "audio");
    lua_pop(L, 1); // pop crayon
}

} // namespace crayon