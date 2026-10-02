#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../graphics/particles.hpp"
#include <new>

namespace crayon {

struct LuaParticleEmitter {
    std::shared_ptr<ParticleEmitter> emitter;
};

[[maybe_unused]] static void* test_particle_udata(lua_State* L, int idx, const char* tname) {
    if (!lua_isuserdata(L, idx)) return nullptr;
    if (lua_getmetatable(L, idx)) {
        luaL_getmetatable(L, tname);
        bool match = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (match) return lua_touserdata(L, idx);
    }
    return nullptr;
}

static std::shared_ptr<ParticleEmitter> check_emitter(lua_State* L, int idx) {
    auto* e = static_cast<LuaParticleEmitter*>(luaL_checkudata(L, idx, "Particles.Emitter"));
    if (!e || !e->emitter) {
        luaL_error(L, "attempt to use invalid Particles.Emitter");
        return nullptr;
    }
    return e->emitter;
}

static void push_emitter_userdata(lua_State* L, std::shared_ptr<ParticleEmitter> emitter) {
    void* mem = lua_newuserdata(L, sizeof(LuaParticleEmitter));
    new (mem) LuaParticleEmitter{ std::move(emitter) };
    luaL_getmetatable(L, "Particles.Emitter");
    lua_setmetatable(L, -2);
}

static int l_emitter_emit(lua_State* L) {
    auto em = check_emitter(L, 1);
    int count = static_cast<int>(luaL_optinteger(L, 2, 1));
    em->emit(count);
    return 0;
}

static int l_emitter_burst(lua_State* L) {
    auto em = check_emitter(L, 1);
    int count = static_cast<int>(luaL_checkinteger(L, 2));
    em->burst(count);
    return 0;
}

static int l_emitter_update(lua_State* L) {
    auto em = check_emitter(L, 1);
    float dt = static_cast<float>(luaL_checknumber(L, 2));
    em->update(dt);
    return 0;
}

static int l_emitter_draw(lua_State* L) {
    auto em = check_emitter(L, 1);
    em->draw(Engine::get().get_batch2d());
    return 0;
}

static int l_emitter_draw_3d(lua_State* L) {
    auto em = check_emitter(L, 1);
    em->draw_3d(Engine::get().get_mesh_renderer());
    return 0;
}

static int l_emitter_reset(lua_State* L) {
    auto em = check_emitter(L, 1);
    em->reset();
    return 0;
}

static int l_emitter_set_position(lua_State* L) {
    auto em = check_emitter(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    em->get_config_ref().position = glm::vec3(x, y, z);
    return 0;
}

static int l_emitter_set_emission_rate(lua_State* L) {
    auto em = check_emitter(L, 1);
    float rate = static_cast<float>(luaL_checknumber(L, 2));
    em->get_config_ref().emission_rate = rate;
    return 0;
}

static int l_emitter_set_active(lua_State* L) {
    auto em = check_emitter(L, 1);
    bool act = lua_toboolean(L, 2);
    em->set_active(act);
    return 0;
}

static int l_emitter_get_alive_count(lua_State* L) {
    auto em = check_emitter(L, 1);
    lua_pushinteger(L, em->get_alive_count());
    return 1;
}

static int l_emitter_gc(lua_State* L) {
    auto* e = static_cast<LuaParticleEmitter*>(luaL_checkudata(L, 1, "Particles.Emitter"));
    if (e) {
        e->~LuaParticleEmitter();
    }
    return 0;
}

static void register_emitter_metatable(lua_State* L) {
    luaL_newmetatable(L, "Particles.Emitter");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_emitter_emit);
    lua_setfield(L, -2, "emit");
    lua_pushcfunction(L, l_emitter_burst);
    lua_setfield(L, -2, "burst");
    lua_pushcfunction(L, l_emitter_update);
    lua_setfield(L, -2, "update");
    lua_pushcfunction(L, l_emitter_draw);
    lua_setfield(L, -2, "draw");
    lua_pushcfunction(L, l_emitter_draw_3d);
    lua_setfield(L, -2, "draw3d");
    lua_pushcfunction(L, l_emitter_reset);
    lua_setfield(L, -2, "reset");
    lua_pushcfunction(L, l_emitter_set_position);
    lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_emitter_set_emission_rate);
    lua_setfield(L, -2, "setEmissionRate");
    lua_pushcfunction(L, l_emitter_set_active);
    lua_setfield(L, -2, "setActive");
    lua_pushcfunction(L, l_emitter_get_alive_count);
    lua_setfield(L, -2, "getAliveCount");

    lua_pushcfunction(L, l_emitter_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

static int l_particles_create_emitter(lua_State* L) {
    ParticleConfig cfg{};
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "maxParticles");
        if (lua_isnumber(L, -1)) cfg.max_particles = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "emissionRate");
        if (lua_isnumber(L, -1)) cfg.emission_rate = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "lifetimeMin");
        if (lua_isnumber(L, -1)) cfg.lifetime_min = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "lifetimeMax");
        if (lua_isnumber(L, -1)) cfg.lifetime_max = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "sizeStart");
        if (lua_isnumber(L, -1)) {
            cfg.size_start_min = static_cast<float>(lua_tonumber(L, -1));
            cfg.size_start_max = cfg.size_start_min;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "sizeEnd");
        if (lua_isnumber(L, -1)) cfg.size_end = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "gravity");
        if (lua_isnumber(L, -1)) cfg.acceleration.y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "blendMode");
        if (lua_isstring(L, -1)) {
            std::string bm = lua_tostring(L, -1);
            if (bm == "additive") cfg.blend_mode = BlendMode::Additive;
            else if (bm == "multiply") cfg.blend_mode = BlendMode::Multiply;
            else cfg.blend_mode = BlendMode::Alpha;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "is3d");
        if (lua_isboolean(L, -1)) cfg.is_3d = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    auto emitter = std::make_shared<ParticleEmitter>(cfg);
    push_emitter_userdata(L, emitter);
    return 1;
}

void register_particle_bindings(lua_State* L) {
    register_emitter_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_particles_create_emitter);
    lua_setfield(L, -2, "createEmitter");

    lua_setfield(L, -2, "particles");
    lua_pop(L, 1);
}

} // namespace crayon
