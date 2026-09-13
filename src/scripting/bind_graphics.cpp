#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../graphics/model3d.hpp"
#include <vector>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <new>

namespace crayon {

struct LuaTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

static void push_texture_userdata(lua_State* L, GLuint id, int width, int height) {
    auto* udata = static_cast<LuaTexture*>(lua_newuserdata(L, sizeof(LuaTexture)));
    udata->id = id;
    udata->width = width;
    udata->height = height;
    luaL_getmetatable(L, "Graphics.Texture");
    lua_setmetatable(L, -2);
}

static void push_model_userdata(lua_State* L, std::shared_ptr<Model3D> model) {
    void* mem = lua_newuserdata(L, sizeof(LuaModel));
    new (mem) LuaModel{ std::move(model) };
    luaL_getmetatable(L, "Graphics.Model");
    lua_setmetatable(L, -2);
}

static GLuint check_texture(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, idx, "Graphics.Texture"));
        return tex ? tex->id : 0;
    }
    return static_cast<GLuint>(luaL_checkinteger(L, idx));
}

static GLuint opt_texture(lua_State* L, int idx, GLuint fallback = 0) {
    if (lua_isnoneornil(L, idx)) return fallback;
    if (lua_isuserdata(L, idx)) {
        auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, idx, "Graphics.Texture"));
        return tex ? tex->id : fallback;
    }
    if (lua_isnumber(L, idx)) {
        return static_cast<GLuint>(lua_tointeger(L, idx));
    }
    return fallback;
}

static std::shared_ptr<Model3D> check_model(lua_State* L, int idx) {
    auto* model = static_cast<LuaModel*>(luaL_checkudata(L, idx, "Graphics.Model"));
    if (!model || !model->model) {
        luaL_error(L, "attempt to use invalid Graphics.Model");
        return nullptr;
    }
    return model->model;
}

// ---------------- Metatables ----------------

static int l_texture_get_size(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    lua_pushinteger(L, tex->width);
    lua_pushinteger(L, tex->height);
    return 2;
}

static int l_texture_get_width(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    lua_pushinteger(L, tex->width);
    return 1;
}

static int l_texture_get_height(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    lua_pushinteger(L, tex->height);
    return 1;
}

static int l_texture_get_id(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    lua_pushinteger(L, tex->id);
    return 1;
}

static int l_texture_is_valid(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    lua_pushboolean(L, tex && tex->id != 0);
    return 1;
}

static int l_texture_tostring(lua_State* L) {
    auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Graphics.Texture(ID: %u, %dx%d)", tex ? tex->id : 0, tex ? tex->width : 0, tex ? tex->height : 0);
    lua_pushstring(L, buf);
    return 1;
}

static int l_texture_gc(lua_State* /*L*/) {
    return 0;
}

static void register_texture_metatable(lua_State* L) {
    luaL_newmetatable(L, "Graphics.Texture");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // camelCase
    lua_pushcfunction(L, l_texture_get_size);
    lua_setfield(L, -2, "getSize");
    lua_pushcfunction(L, l_texture_get_width);
    lua_setfield(L, -2, "getWidth");
    lua_pushcfunction(L, l_texture_get_height);
    lua_setfield(L, -2, "getHeight");
    lua_pushcfunction(L, l_texture_get_id);
    lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_texture_is_valid);
    lua_setfield(L, -2, "isValid");

    lua_pushcfunction(L, l_texture_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_texture_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

static int l_model_is_valid(lua_State* L) {
    auto* model = static_cast<LuaModel*>(luaL_checkudata(L, 1, "Graphics.Model"));
    lua_pushboolean(L, model && model->model != nullptr && model->model->is_valid());
    return 1;
}

static int l_model_get_node_count(lua_State* L) {
    auto model = check_model(L, 1);
    lua_pushinteger(L, model ? static_cast<lua_Integer>(model->get_node_count()) : 0);
    return 1;
}

static void push_node_table(lua_State* L, const ModelNode& node, int idx) {
    lua_newtable(L);
    lua_pushstring(L, node.name.c_str());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, idx);
    lua_setfield(L, -2, "index");
    lua_pushinteger(L, node.parent_index);
    lua_setfield(L, -2, "parent");

    lua_pushnumber(L, node.translation.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, node.translation.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, node.translation.z);
    lua_setfield(L, -2, "z");

    lua_pushnumber(L, node.rotation.x);
    lua_setfield(L, -2, "rx");
    lua_pushnumber(L, node.rotation.y);
    lua_setfield(L, -2, "ry");
    lua_pushnumber(L, node.rotation.z);
    lua_setfield(L, -2, "rz");
    lua_pushnumber(L, node.rotation.w);
    lua_setfield(L, -2, "rw");

    lua_pushnumber(L, node.scale.x);
    lua_setfield(L, -2, "sx");
    lua_pushnumber(L, node.scale.y);
    lua_setfield(L, -2, "sy");
    lua_pushnumber(L, node.scale.z);
    lua_setfield(L, -2, "sz");
}

static int l_model_get_node(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    if (lua_isnumber(L, 2)) {
        size_t idx = static_cast<size_t>(lua_tointeger(L, 2));
        const auto* node = model->get_node(idx);
        if (node) {
            push_node_table(L, *node, static_cast<int>(idx));
            return 1;
        }
    } else if (lua_isstring(L, 2)) {
        std::string name = lua_tostring(L, 2);
        int idx = model->find_node_index(name);
        if (idx >= 0) {
            push_node_table(L, *model->get_node(static_cast<size_t>(idx)), idx);
            return 1;
        }
    }
    return 0;
}

static int l_model_get_nodes(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    const auto& nodes = model->get_nodes();
    lua_createtable(L, static_cast<int>(nodes.size()), 0);
    for (size_t i = 0; i < nodes.size(); ++i) {
        push_node_table(L, nodes[i], static_cast<int>(i));
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

static int l_model_get_part_count(lua_State* L) {
    auto model = check_model(L, 1);
    lua_pushinteger(L, model ? static_cast<lua_Integer>(model->get_part_count()) : 0);
    return 1;
}

static int l_model_get_part_name(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    size_t idx = static_cast<size_t>(luaL_checkinteger(L, 2));
    const auto* part = model->get_part(idx);
    if (part) {
        lua_pushstring(L, part->name.c_str());
        lua_pushstring(L, part->material_name.c_str());
        return 2;
    }
    return 0;
}

static int l_model_get_part_texture(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    size_t idx = static_cast<size_t>(luaL_checkinteger(L, 2));
    const auto* part = model->get_part(idx);
    lua_pushinteger(L, part ? part->texture_id : 0);
    return 1;
}

static int l_model_set_part_texture(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    size_t idx = static_cast<size_t>(luaL_checkinteger(L, 2));
    GLuint tex = opt_texture(L, 3, 0);
    model->set_part_texture(idx, tex);
    return 0;
}

static int l_model_set_part_color(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    size_t idx = static_cast<size_t>(luaL_checkinteger(L, 2));
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    model->set_part_color(idx, glm::vec4(r, g, b, a));
    return 0;
}

static int l_model_get_bounds(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    glm::vec3 bmin(0.0f), bmax(0.0f);
    model->get_bounds(bmin, bmax);
    lua_pushnumber(L, bmin.x);
    lua_pushnumber(L, bmin.y);
    lua_pushnumber(L, bmin.z);
    lua_pushnumber(L, bmax.x);
    lua_pushnumber(L, bmax.y);
    lua_pushnumber(L, bmax.z);
    return 6;
}

static int l_model_get_center(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    glm::vec3 c = model->get_center();
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    return 3;
}

static int l_model_get_size(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    glm::vec3 s = model->get_size();
    lua_pushnumber(L, s.x);
    lua_pushnumber(L, s.y);
    lua_pushnumber(L, s.z);
    return 3;
}

static int l_model_get_triangles(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;
    std::vector<glm::vec3> verts;
    std::vector<uint32_t> idxs;
    model->get_collision_data(verts, idxs, true);

    lua_createtable(L, static_cast<int>(idxs.size() / 3), 0);
    int tri_count = 0;
    for (size_t i = 0; i + 2 < idxs.size(); i += 3) {
        lua_createtable(L, 3, 0);

        for (int k = 0; k < 3; ++k) {
            const auto& p = verts[idxs[i + k]];
            lua_createtable(L, 3, 0);
            lua_pushnumber(L, p.x); lua_rawseti(L, -2, 1);
            lua_pushnumber(L, p.y); lua_rawseti(L, -2, 2);
            lua_pushnumber(L, p.z); lua_rawseti(L, -2, 3);
            lua_rawseti(L, -2, k + 1);
        }

        lua_rawseti(L, -2, ++tri_count);
    }
    return 1;
}

static int l_model_tostring(lua_State* L) {
    auto* model = static_cast<LuaModel*>(luaL_checkudata(L, 1, "Graphics.Model"));
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Graphics.Model(%p, parts: %zu, nodes: %zu)",
                  model ? model->model.get() : nullptr,
                  (model && model->model) ? model->model->get_part_count() : 0,
                  (model && model->model) ? model->model->get_node_count() : 0);
    lua_pushstring(L, buf);
    return 1;
}

static int l_model_gc(lua_State* L) {
    auto* model = static_cast<LuaModel*>(luaL_checkudata(L, 1, "Graphics.Model"));
    if (model) {
        model->~LuaModel();
    }
    return 0;
}

static void register_model_metatable(lua_State* L) {
    luaL_newmetatable(L, "Graphics.Model");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_model_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_model_is_valid);
    lua_setfield(L, -2, "is_valid");

    lua_pushcfunction(L, l_model_get_node_count);
    lua_setfield(L, -2, "getNodeCount");
    lua_pushcfunction(L, l_model_get_node_count);
    lua_setfield(L, -2, "get_node_count");

    lua_pushcfunction(L, l_model_get_node);
    lua_setfield(L, -2, "getNode");
    lua_pushcfunction(L, l_model_get_node);
    lua_setfield(L, -2, "get_node");
    lua_pushcfunction(L, l_model_get_node);
    lua_setfield(L, -2, "findNode");
    lua_pushcfunction(L, l_model_get_node);
    lua_setfield(L, -2, "find_node");

    lua_pushcfunction(L, l_model_get_nodes);
    lua_setfield(L, -2, "getNodes");
    lua_pushcfunction(L, l_model_get_nodes);
    lua_setfield(L, -2, "get_nodes");

    lua_pushcfunction(L, l_model_get_part_count);
    lua_setfield(L, -2, "getPartCount");
    lua_pushcfunction(L, l_model_get_part_count);
    lua_setfield(L, -2, "get_part_count");

    lua_pushcfunction(L, l_model_get_part_name);
    lua_setfield(L, -2, "getPartName");
    lua_pushcfunction(L, l_model_get_part_name);
    lua_setfield(L, -2, "get_part_name");

    lua_pushcfunction(L, l_model_get_part_texture);
    lua_setfield(L, -2, "getPartTexture");
    lua_pushcfunction(L, l_model_get_part_texture);
    lua_setfield(L, -2, "get_part_texture");

    lua_pushcfunction(L, l_model_set_part_texture);
    lua_setfield(L, -2, "setPartTexture");
    lua_pushcfunction(L, l_model_set_part_texture);
    lua_setfield(L, -2, "set_part_texture");

    lua_pushcfunction(L, l_model_set_part_color);
    lua_setfield(L, -2, "setPartColor");
    lua_pushcfunction(L, l_model_set_part_color);
    lua_setfield(L, -2, "set_part_color");

    lua_pushcfunction(L, l_model_get_bounds);
    lua_setfield(L, -2, "getBounds");
    lua_pushcfunction(L, l_model_get_bounds);
    lua_setfield(L, -2, "get_bounds");

    lua_pushcfunction(L, l_model_get_center);
    lua_setfield(L, -2, "getCenter");
    lua_pushcfunction(L, l_model_get_center);
    lua_setfield(L, -2, "get_center");

    lua_pushcfunction(L, l_model_get_size);
    lua_setfield(L, -2, "getSize");
    lua_pushcfunction(L, l_model_get_size);
    lua_setfield(L, -2, "get_size");

    lua_pushcfunction(L, l_model_get_triangles);
    lua_setfield(L, -2, "getTriangles");
    lua_pushcfunction(L, l_model_get_triangles);
    lua_setfield(L, -2, "get_triangles");

    lua_pushcfunction(L, l_model_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_model_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

static int l_graphics_clear(lua_State* L) {
    float r = static_cast<float>(luaL_checknumber(L, 1));
    float g = static_cast<float>(luaL_checknumber(L, 2));
    float b = static_cast<float>(luaL_checknumber(L, 3));
    float a = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    Engine::get().set_clear_color(r, g, b, a);
    return 0;
}

static int l_graphics_set_color(lua_State* L) {
    float r = static_cast<float>(luaL_checknumber(L, 1));
    float g = static_cast<float>(luaL_checknumber(L, 2));
    float b = static_cast<float>(luaL_checknumber(L, 3));
    float a = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    Engine::get().set_active_color(r, g, b, a);
    return 0;
}

static int l_graphics_set_retro_effects(lua_State* L) {
    if (!lua_istable(L, 1)) return 0;

    auto& effects = Engine::get().get_mesh_renderer().get_retro_effects();

    // opts.jitter_resolution
    lua_getfield(L, 1, "jitterResolution");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1);
        float jx = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        lua_rawgeti(L, -1, 2);
        float jy = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        effects.jitter_enabled = true;
        effects.jitter_resolution = glm::vec2(jx, jy);
    } else if (lua_isnil(L, -1) || (lua_isboolean(L, -1) && !lua_toboolean(L, -1))) {
        effects.jitter_enabled = false;
    }
    lua_pop(L, 1);

    // opts.affine
    lua_getfield(L, 1, "affine");
    if (lua_isnumber(L, -1)) {
        effects.affine_blend = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // opts.dither
    lua_getfield(L, 1, "dither");
    if (lua_isboolean(L, -1)) {
        effects.dither_enabled = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    // opts.color_depth or opts.dither_levels
    lua_getfield(L, 1, "colorDepth");
    if (lua_isnumber(L, -1)) {
        int depth = static_cast<int>(lua_tointeger(L, -1));
        effects.dither_levels = (depth == 8) ? 8.0f : 32.0f;
    }
    lua_pop(L, 1);
    lua_getfield(L, 1, "ditherLevels");
    if (lua_isnumber(L, -1)) {
        effects.dither_levels = static_cast<float>(lua_tonumber(L, -1));
    }
    lua_pop(L, 1);

    // opts.fog = { start = 10, end = 50, color = {0.1, 0.1, 0.2} }
    lua_getfield(L, 1, "fog");
    if (lua_istable(L, -1)) {
        effects.fog_enabled = true;
        lua_getfield(L, -1, "startDist");
        if (lua_isnumber(L, -1)) effects.fog_start = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "endDist");
        if (lua_isnumber(L, -1)) effects.fog_end = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "color");
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            float fr = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
            lua_rawgeti(L, -1, 2);
            float fg = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
            lua_rawgeti(L, -1, 3);
            float fb = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
            effects.fog_color = glm::vec3(fr, fg, fb);
        }
        lua_pop(L, 1);
    } else if (lua_isnil(L, -1) || (lua_isboolean(L, -1) && !lua_toboolean(L, -1))) {
        effects.fog_enabled = false;
    }
    lua_pop(L, 1);

    // opts.crt = true or { scanlines = 0.3, curvature = 0.05, vignette = 0.25 }
    lua_getfield(L, 1, "crt");
    if (lua_isboolean(L, -1)) {
        bool b = lua_toboolean(L, -1);
        effects.crt_scanlines = b;
        effects.crt_curvature = b;
        effects.vignette = b;
    } else if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "scanlines");
        if (lua_isnumber(L, -1)) { effects.crt_scanlines = true; effects.scanline_strength = static_cast<float>(lua_tonumber(L, -1)); }
        else if (lua_isboolean(L, -1)) { effects.crt_scanlines = lua_toboolean(L, -1); }
        lua_pop(L, 1);

        lua_getfield(L, -1, "curvature");
        if (lua_isnumber(L, -1)) { effects.crt_curvature = true; effects.curvature_distort = static_cast<float>(lua_tonumber(L, -1)); }
        else if (lua_isboolean(L, -1)) { effects.crt_curvature = lua_toboolean(L, -1); }
        lua_pop(L, 1);

        lua_getfield(L, -1, "vignette");
        if (lua_isnumber(L, -1)) { effects.vignette = true; effects.vignette_strength = static_cast<float>(lua_tonumber(L, -1)); }
        else if (lua_isboolean(L, -1)) { effects.vignette = lua_toboolean(L, -1); }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "scanlines");
    if (lua_isboolean(L, -1)) effects.crt_scanlines = lua_toboolean(L, -1);
    else if (lua_isnumber(L, -1)) { effects.crt_scanlines = true; effects.scanline_strength = static_cast<float>(lua_tonumber(L, -1)); }
    lua_pop(L, 1);

    lua_getfield(L, 1, "curvature");
    if (lua_isboolean(L, -1)) effects.crt_curvature = lua_toboolean(L, -1);
    else if (lua_isnumber(L, -1)) { effects.crt_curvature = true; effects.curvature_distort = static_cast<float>(lua_tonumber(L, -1)); }
    lua_pop(L, 1);

    lua_getfield(L, 1, "vignette");
    if (lua_isboolean(L, -1)) effects.vignette = lua_toboolean(L, -1);
    else if (lua_isnumber(L, -1)) { effects.vignette = true; effects.vignette_strength = static_cast<float>(lua_tonumber(L, -1)); }
    lua_pop(L, 1);

    return 0;
}

static int l_graphics_set_camera3d(lua_State* L) {
    auto& cam = Engine::get().get_camera();

    if (!lua_istable(L, 1)) {
        if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
            float x = static_cast<float>(lua_tonumber(L, 1));
            float y = static_cast<float>(lua_tonumber(L, 2));
            float z = static_cast<float>(lua_tonumber(L, 3));
            cam.set_position(glm::vec3(x, y, z));

            int top = lua_gettop(L);
            if (top >= 5) {
                float yaw = static_cast<float>(lua_tonumber(L, 4));
                float pitch = static_cast<float>(lua_tonumber(L, 5));
                float rad_yaw = glm::radians(yaw);
                float rad_pitch = glm::radians(pitch);
                glm::vec3 fwd(
                    std::cos(rad_pitch) * std::sin(rad_yaw),
                    std::sin(rad_pitch),
                    -std::cos(rad_pitch) * std::cos(rad_yaw)
                );
                cam.set_target(glm::vec3(x, y, z) + fwd);
                cam.set_up(glm::vec3(0.0f, 1.0f, 0.0f));
                if (top >= 6 && lua_isnumber(L, 6)) {
                    cam.set_fov(static_cast<float>(lua_tonumber(L, 6)));
                }
            }
        }
        return 0;
    }

    // cam.position = {x, y, z}
    lua_getfield(L, 1, "position");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        cam.set_position(glm::vec3(x, y, z));
    }
    lua_pop(L, 1);

    // cam.target = {x, y, z}
    lua_getfield(L, 1, "target");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        cam.set_target(glm::vec3(x, y, z));
    }
    lua_pop(L, 1);

    // cam.up = {x, y, z}
    lua_getfield(L, 1, "up");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        cam.set_up(glm::vec3(x, y, z));
    }
    lua_pop(L, 1);

    // cam.fov
    lua_getfield(L, 1, "fov");
    if (lua_isnumber(L, -1)) {
        cam.set_fov(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    // cam.near
    lua_getfield(L, 1, "near");
    if (lua_isnumber(L, -1)) {
        cam.set_near(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    // cam.far
    lua_getfield(L, 1, "far");
    if (lua_isnumber(L, -1)) {
        cam.set_far(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    // cam.ortho
    lua_getfield(L, 1, "ortho");
    if (lua_isboolean(L, -1)) {
        cam.set_orthographic(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    // cam.ortho_size
    lua_getfield(L, 1, "orthoSize");
    if (lua_isnumber(L, -1)) {
        cam.set_ortho_size(static_cast<float>(lua_tonumber(L, -1)));
    }
    lua_pop(L, 1);

    return 0;
}

static int l_graphics_get_camera_ray(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));

    int virt_w = Engine::get().get_window().get_virtual_width();
    int virt_h = Engine::get().get_window().get_virtual_height();
    if (virt_w <= 0 || virt_h <= 0) return 0;

    float norm_x = (sx / static_cast<float>(virt_w)) * 2.0f - 1.0f;
    float norm_y = 1.0f - (sy / static_cast<float>(virt_h)) * 2.0f;
    float aspect = static_cast<float>(virt_w) / static_cast<float>(virt_h);

    glm::vec3 origin, dir;
    Engine::get().get_camera().get_ray(norm_x, norm_y, aspect, origin, dir);

    lua_newtable(L);

    lua_newtable(L);
    lua_pushnumber(L, origin.x); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, origin.y); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, origin.z); lua_rawseti(L, -2, 3);
    lua_setfield(L, -2, "origin");

    lua_newtable(L);
    lua_pushnumber(L, dir.x); lua_rawseti(L, -2, 1);
    lua_pushnumber(L, dir.y); lua_rawseti(L, -2, 2);
    lua_pushnumber(L, dir.z); lua_rawseti(L, -2, 3);
    lua_setfield(L, -2, "direction");

    return 1;
}

static int l_graphics_load_texture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    GLuint id = Engine::get().load_texture(path);
    int w = 0, h = 0;
    Engine::get().get_texture_size(id, w, h);
    push_texture_userdata(L, id, w, h);
    return 1;
}

static int l_graphics_get_texture_size(lua_State* L) {
    if (lua_isuserdata(L, 1)) {
        auto* tex = static_cast<LuaTexture*>(luaL_checkudata(L, 1, "Graphics.Texture"));
        lua_pushinteger(L, tex->width);
        lua_pushinteger(L, tex->height);
        return 2;
    }
    GLuint tex_id = static_cast<GLuint>(luaL_checkinteger(L, 1));
    int w = 0, h = 0;
    Engine::get().get_texture_size(tex_id, w, h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

static int l_graphics_load_model(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string p(path);

    std::shared_ptr<Model3D> model;
    if (p == "cube") {
        model = Model3D::create_from_mesh(Mesh3D::create_cube(1.0f), "cube");
    } else if (p == "plane") {
        model = Model3D::create_from_mesh(Mesh3D::create_plane(10.0f, 10.0f, 10), "plane");
    } else if (p == "sphere") {
        model = Model3D::create_from_mesh(Mesh3D::create_sphere(0.5f, 16, 16), "sphere");
    } else if (p == "cylinder") {
        model = Model3D::create_from_mesh(Mesh3D::create_cylinder(0.5f, 1.0f, 16), "cylinder");
    } else if (p == "cone") {
        model = Model3D::create_from_mesh(Mesh3D::create_cone(0.5f, 1.0f, 16), "cone");
    } else if (p == "pyramid") {
        model = Model3D::create_from_mesh(Mesh3D::create_pyramid(1.0f, 1.0f), "pyramid");
    } else if (p == "torus") {
        model = Model3D::create_from_mesh(Mesh3D::create_torus(0.8f, 0.25f, 16, 12), "torus");
    } else if (p == "capsule") {
        model = Model3D::create_from_mesh(Mesh3D::create_capsule(0.4f, 0.8f, 8, 12), "capsule");
    } else if (p == "grid") {
        model = Model3D::create_from_mesh(Mesh3D::create_grid(20.0f, 20), "grid");
    } else {
        model = Engine::get().load_model3d(p);
    }

    push_model_userdata(L, model);
    return 1;
}

static int l_graphics_create_mesh(lua_State* L) {
    if (!lua_istable(L, 1)) return 0;

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    lua_getfield(L, 1, "vertices");
    if (lua_istable(L, -1)) {
        int v_len = static_cast<int>(lua_objlen(L, -1));
        vertices.reserve(v_len);
        for (int i = 1; i <= v_len; ++i) {
            lua_rawgeti(L, -1, i);
            if (lua_istable(L, -1)) {
                Vertex3D v{};
                v.normal = glm::vec3(0, 1, 0);
                v.uv = glm::vec2(0, 0);
                v.color = glm::vec4(1, 1, 1, 1);

                lua_getfield(L, -1, "pos");
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); v.position.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); v.position.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); v.position.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "norm");
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); v.normal.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); v.normal.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); v.normal.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "uv");
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); v.uv.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); v.uv.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "color");
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); v.color.r = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); v.color.g = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); v.color.b = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 4); v.color.a = static_cast<float>(luaL_optnumber(L, -1, 1.0)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                vertices.push_back(v);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "indices");
    if (lua_istable(L, -1)) {
        int idx_len = static_cast<int>(lua_objlen(L, -1));
        indices.reserve(idx_len);
        for (int i = 1; i <= idx_len; ++i) {
            lua_rawgeti(L, -1, i);
            int idx = static_cast<int>(lua_tointeger(L, -1));
            indices.push_back(static_cast<GLuint>(idx > 0 ? idx - 1 : 0));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    auto mesh = std::make_shared<Mesh3D>();
    mesh->create_from_data(vertices, indices);
    auto model = Model3D::create_from_mesh(mesh, "custom_mesh");
    push_model_userdata(L, model);
    return 1;
}

// ---------------- 3D Primitives Direct Draw ----------------

static int l_graphics_draw_model(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;

    float x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float z = static_cast<float>(luaL_optnumber(L, 4, 0.0));

    float rx = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 7, 0.0));

    float sx = static_cast<float>(luaL_optnumber(L, 8, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    float sz = static_cast<float>(luaL_optnumber(L, 10, 1.0));

    GLuint tex_id = opt_texture(L, 11, 0);

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(x, y, z));
    if (rz != 0.0f) m = glm::rotate(m, rz, glm::vec3(0, 0, 1));
    if (ry != 0.0f) m = glm::rotate(m, ry, glm::vec3(0, 1, 0));
    if (rx != 0.0f) m = glm::rotate(m, rx, glm::vec3(1, 0, 0));
    m = glm::scale(m, glm::vec3(sx, sy, sz));

    model->draw(Engine::get().get_mesh_renderer(), m, tex_id);
    return 0;
}

static int l_graphics_draw_model_node(lua_State* L) {
    auto model = check_model(L, 1);
    if (!model) return 0;

    float x = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float z = static_cast<float>(luaL_optnumber(L, 5, 0.0));

    float rx = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 8, 0.0));

    float sx = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 10, 1.0));
    float sz = static_cast<float>(luaL_optnumber(L, 11, 1.0));

    GLuint tex_id = opt_texture(L, 12, 0);

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(x, y, z));
    if (rz != 0.0f) m = glm::rotate(m, rz, glm::vec3(0, 0, 1));
    if (ry != 0.0f) m = glm::rotate(m, ry, glm::vec3(0, 1, 0));
    if (rx != 0.0f) m = glm::rotate(m, rx, glm::vec3(1, 0, 0));
    m = glm::scale(m, glm::vec3(sx, sy, sz));

    if (lua_isnumber(L, 2)) {
        int node_idx = static_cast<int>(lua_tointeger(L, 2));
        model->draw_node(Engine::get().get_mesh_renderer(), node_idx, m, tex_id);
    } else if (lua_isstring(L, 2)) {
        std::string name = lua_tostring(L, 2);
        model->draw_node(Engine::get().get_mesh_renderer(), name, m, tex_id);
    }
    return 0;
}

static int l_graphics_draw_cube(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float sx = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 5, sx));
    float sz = static_cast<float>(luaL_optnumber(L, 6, sx));
    GLuint tex = opt_texture(L, 7, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 10, 0.0));

    Engine::get().get_mesh_renderer().draw_cube({x, y, z}, {sx, sy, sz}, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_plane(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_optnumber(L, 4, 10.0));
    float d = static_cast<float>(luaL_optnumber(L, 5, w));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_plane({x, y, z}, w, d, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_sphere(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    GLuint tex = opt_texture(L, 5, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 8, 0.0));

    Engine::get().get_mesh_renderer().draw_sphere({x, y, z}, radius, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_cylinder(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    float height = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_cylinder({x, y, z}, radius, height, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_cone(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    float height = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_cone({x, y, z}, radius, height, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_pyramid(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float base_size = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float height = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_pyramid({x, y, z}, base_size, height, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_torus(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 0.8));
    float tube = static_cast<float>(luaL_optnumber(L, 5, 0.25));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_torus({x, y, z}, radius, tube, tex, {rx, ry, rz});
    return 0;
}

static int l_graphics_draw_capsule(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 0.4));
    float height = static_cast<float>(luaL_optnumber(L, 5, 0.8));
    GLuint tex = opt_texture(L, 6, 0);
    float rx = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    Engine::get().get_mesh_renderer().draw_capsule({x, y, z}, radius, height, tex, {rx, ry, rz});
    return 0;
}

// ---------------- 3D Billboards & Lines ----------------

static int l_graphics_draw_billboard(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float h = static_cast<float>(luaL_optnumber(L, 5, w));
    GLuint tex_id = opt_texture(L, 6, 0);

    BillboardMode mode = BillboardMode::Spherical;
    if (lua_isboolean(L, 7)) {
        mode = lua_toboolean(L, 7) ? BillboardMode::Cylindrical : BillboardMode::Spherical;
    } else if (lua_isstring(L, 7)) {
        const char* mode_str = lua_tostring(L, 7);
        if (mode_str && std::string(mode_str) == "cylindrical") {
            mode = BillboardMode::Cylindrical;
        }
    }

    float u0 = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float v0 = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float u1 = static_cast<float>(luaL_optnumber(L, 10, 1.0));
    float v1 = static_cast<float>(luaL_optnumber(L, 11, 1.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_billboard(tex_id, {x, y, z}, {w, h}, mode, col, u0, v0, u1, v1);
    return 0;
}

static int l_graphics_draw_line_3d(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float z1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float z2 = static_cast<float>(luaL_checknumber(L, 6));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_line_3d({x1, y1, z1}, {x2, y2, z2}, col);
    return 0;
}

static int l_graphics_draw_lines_3d(lua_State* L) {
    if (!lua_istable(L, 1)) return 0;
    int len = static_cast<int>(lua_objlen(L, 1));
    std::vector<glm::vec3> pts;
    pts.reserve(len);

    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            pts.push_back({x, y, z});
        }
        lua_pop(L, 1);
    }

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_lines_3d(pts, col);
    return 0;
}

static int l_graphics_draw_grid_3d(lua_State* L) {
    float size = static_cast<float>(luaL_optnumber(L, 1, 20.0));
    int divs = static_cast<int>(luaL_optinteger(L, 2, 20));
    float y = static_cast<float>(luaL_optnumber(L, 3, 0.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_grid_3d(size, divs, y, col);
    return 0;
}

static int l_graphics_draw_triangle_3d(lua_State* L) {
    // p1 = {x,y,z}, p2 = {x,y,z}, p3 = {x,y,z}
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_istable(L, 3)) return 0;

    auto get_vec3 = [&](int idx) {
        lua_rawgeti(L, idx, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        return glm::vec3(x, y, z);
    };

    glm::vec3 p1 = get_vec3(1);
    glm::vec3 p2 = get_vec3(2);
    glm::vec3 p3 = get_vec3(3);
    GLuint tex = opt_texture(L, 4, 0);

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_triangle_3d(p1, p2, p3, col, tex);
    return 0;
}

static int l_graphics_draw_quad_3d(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_istable(L, 3) || !lua_istable(L, 4)) return 0;

    auto get_vec3 = [&](int idx) {
        lua_rawgeti(L, idx, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 3); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        return glm::vec3(x, y, z);
    };

    glm::vec3 p1 = get_vec3(1);
    glm::vec3 p2 = get_vec3(2);
    glm::vec3 p3 = get_vec3(3);
    glm::vec3 p4 = get_vec3(4);
    GLuint tex = opt_texture(L, 5, 0);

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_mesh_renderer().draw_quad_3d(p1, p2, p3, p4, col, tex);
    return 0;
}

static int l_graphics_set_shading_mode(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    std::string m(mode_str);
    if (m == "flat") {
        Engine::get().get_mesh_renderer().set_shading_mode(ShadingMode::Flat);
    } else if (m == "unlit") {
        Engine::get().get_mesh_renderer().set_shading_mode(ShadingMode::Unlit);
    } else {
        Engine::get().get_mesh_renderer().set_shading_mode(ShadingMode::Gouraud);
    }
    return 0;
}

static int l_graphics_set_point_light(lua_State* L) {
    int raw_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int idx = (raw_idx >= 1) ? (raw_idx - 1) : raw_idx; // Supports both 1-based (1..4) and 0-based (0..3)
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float radius = static_cast<float>(luaL_optnumber(L, 8, 10.0));
    float intensity = static_cast<float>(luaL_optnumber(L, 9, 1.0));

    Engine::get().get_mesh_renderer().set_point_light(idx, {x, y, z}, {r, g, b}, radius, intensity);
    return 0;
}

static int l_graphics_set_point_light_enabled(lua_State* L) {
    int raw_idx = static_cast<int>(luaL_checkinteger(L, 1));
    int idx = (raw_idx >= 1) ? (raw_idx - 1) : raw_idx;
    bool enabled = lua_toboolean(L, 2);
    Engine::get().get_mesh_renderer().set_point_light_enabled(idx, enabled);
    return 0;
}

// ---------------- 2D Primitives ----------------

static int l_graphics_draw_sprite(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_optnumber(L, 4, 32.0));
    float h = static_cast<float>(luaL_optnumber(L, 5, 32.0));
    float rot = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float ox = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 8, 0.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_sprite(tex_id, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, col, rot, ox, oy);
    return 0;
}

static int l_graphics_draw_sprite_part(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float u0 = static_cast<float>(luaL_checknumber(L, 4));
    float v0 = static_cast<float>(luaL_checknumber(L, 5));
    float u1 = static_cast<float>(luaL_checknumber(L, 6));
    float v1 = static_cast<float>(luaL_checknumber(L, 7));
    float w = static_cast<float>(luaL_optnumber(L, 8, 32.0));
    float h = static_cast<float>(luaL_optnumber(L, 9, 32.0));
    float rot = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    float ox = static_cast<float>(luaL_optnumber(L, 11, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 12, 0.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_sprite(tex_id, x, y, w, h, u0, v0, u1, v1, col, rot, ox, oy);
    return 0;
}

static int l_graphics_draw_sprite_tiled(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float tile_w = static_cast<float>(luaL_optnumber(L, 6, 32.0));
    float tile_h = static_cast<float>(luaL_optnumber(L, 7, tile_w));
    float ox = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 9, 0.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_sprite_tiled(tex_id, x, y, w, h, tile_w, tile_h, col, ox, oy);
    return 0;
}

static int l_graphics_draw_sprite_9slice(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float left = static_cast<float>(luaL_checknumber(L, 6));
    float top = static_cast<float>(luaL_checknumber(L, 7));
    float right = static_cast<float>(luaL_checknumber(L, 8));
    float bottom = static_cast<float>(luaL_checknumber(L, 9));

    int tw = 0, th = 0;
    if (lua_isnumber(L, 10) && lua_isnumber(L, 11)) {
        tw = static_cast<int>(lua_tointeger(L, 10));
        th = static_cast<int>(lua_tointeger(L, 11));
    } else {
        Engine::get().get_texture_size(tex_id, tw, th);
    }
    if (tw <= 0) tw = 32;
    if (th <= 0) th = 32;

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_sprite_9slice(tex_id, x, y, w, h, left, top, right, bottom, tw, th, col);
    return 0;
}

static int l_graphics_draw_point(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float size = static_cast<float>(luaL_optnumber(L, 3, 1.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_point(x, y, size, col);
    return 0;
}

static int l_graphics_draw_line(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float thick = static_cast<float>(luaL_optnumber(L, 5, 1.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_line(x1, y1, x2, y2, col, thick);
    return 0;
}

static int l_graphics_draw_rect(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float thick = static_cast<float>(luaL_optnumber(L, 6, 1.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_rect(x, y, w, h, col, filled, thick);
    return 0;
}

static int l_graphics_draw_rounded_rect(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float radius = static_cast<float>(luaL_checknumber(L, 6));
    int segs = static_cast<int>(luaL_optinteger(L, 7, 8));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_rounded_rect(x, y, w, h, radius, col, filled, segs);
    return 0;
}

static int l_graphics_draw_triangle(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float x1 = static_cast<float>(luaL_checknumber(L, 2));
    float y1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float x3 = static_cast<float>(luaL_checknumber(L, 6));
    float y3 = static_cast<float>(luaL_checknumber(L, 7));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_triangle(x1, y1, x2, y2, x3, y3, col, filled);
    return 0;
}

static int l_graphics_draw_quad(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float x1 = static_cast<float>(luaL_checknumber(L, 2));
    float y1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float x3 = static_cast<float>(luaL_checknumber(L, 6));
    float y3 = static_cast<float>(luaL_checknumber(L, 7));
    float x4 = static_cast<float>(luaL_checknumber(L, 8));
    float y4 = static_cast<float>(luaL_checknumber(L, 9));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_quad(x1, y1, x2, y2, x3, y3, x4, y4, col, filled);
    return 0;
}

static int l_graphics_draw_polygon(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    if (!lua_istable(L, 2)) return 0;

    int len = static_cast<int>(lua_objlen(L, 2));
    if (len < 3) return 0;

    std::vector<glm::vec2> pts;

    // Check if element 1 is a subtable or a flat number
    lua_rawgeti(L, 2, 1);
    bool is_subtable = lua_istable(L, -1);
    lua_pop(L, 1);

    if (is_subtable) {
        pts.reserve(len);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 2, i);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                pts.push_back({x, y});
            }
            lua_pop(L, 1);
        }
    } else {
        pts.reserve(len / 2);
        for (int i = 1; i + 1 <= len; i += 2) {
            lua_rawgeti(L, 2, i);     float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, 2, i + 1); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            pts.push_back({x, y});
        }
    }

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_polygon(pts, col, filled);
    return 0;
}

static int l_graphics_draw_circle(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    int segs = static_cast<int>(luaL_optinteger(L, 5, 24));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_circle(cx, cy, radius, col, filled, segs);
    return 0;
}

static int l_graphics_draw_ellipse(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float rx = static_cast<float>(luaL_checknumber(L, 4));
    float ry = static_cast<float>(luaL_checknumber(L, 5));
    int segs = static_cast<int>(luaL_optinteger(L, 6, 24));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_ellipse(cx, cy, rx, ry, col, filled, segs);
    return 0;
}

static int l_graphics_draw_arc(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float a0 = static_cast<float>(luaL_checknumber(L, 5));
    float a1 = static_cast<float>(luaL_checknumber(L, 6));
    int segs = static_cast<int>(luaL_optinteger(L, 7, 16));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_arc(cx, cy, radius, a0, a1, col, filled, segs);
    return 0;
}

static int l_graphics_draw_ring(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float in_r = static_cast<float>(luaL_checknumber(L, 4));
    float out_r = static_cast<float>(luaL_checknumber(L, 5));
    int segs = static_cast<int>(luaL_optinteger(L, 6, 24));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_ring(cx, cy, in_r, out_r, col, filled, segs);
    return 0;
}

static int l_graphics_draw_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float scale = static_cast<float>(luaL_optnumber(L, 4, 1.0));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_text(text, x, y, scale, col);
    return 0;
}

static int l_graphics_get_text_width(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float scale = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float w = Engine::get().get_batch2d().get_text_width(text, scale);
    lua_pushnumber(L, w);
    return 1;
}

static int l_graphics_get_text_height(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float scale = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float h = Engine::get().get_batch2d().get_text_height(text, scale);
    lua_pushnumber(L, h);
    return 1;
}

static int l_graphics_set_blend_mode(lua_State* L) {
    const char* mode = luaL_checkstring(L, 1);
    std::string m(mode);
    if (m == "additive") {
        Engine::get().get_batch2d().set_blend_mode(BlendMode::Additive);
    } else if (m == "multiply") {
        Engine::get().get_batch2d().set_blend_mode(BlendMode::Multiply);
    } else if (m == "none") {
        Engine::get().get_batch2d().set_blend_mode(BlendMode::None);
    } else {
        Engine::get().get_batch2d().set_blend_mode(BlendMode::Alpha);
    }
    return 0;
}

static int l_graphics_set_scissor(lua_State* L) {
    int x = static_cast<int>(luaL_checkinteger(L, 1));
    int y = static_cast<int>(luaL_checkinteger(L, 2));
    int w = static_cast<int>(luaL_checkinteger(L, 3));
    int h = static_cast<int>(luaL_checkinteger(L, 4));
    Engine::get().get_batch2d().set_scissor(x, y, w, h);
    return 0;
}

static int l_graphics_reset_scissor(lua_State* L) {
    (void)L;
    Engine::get().get_batch2d().reset_scissor();
    return 0;
}

static int l_graphics_set_camera2d(lua_State* L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        Engine::get().get_batch2d().reset_camera2d();
        return 0;
    }
    if (!lua_istable(L, 1)) return 0;

    float x = 0.0f, y = 0.0f, zoom = 1.0f, angle = 0.0f, ox = 0.0f, oy = 0.0f;

    lua_getfield(L, 1, "x"); if (lua_isnumber(L, -1)) x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 1, "y"); if (lua_isnumber(L, -1)) y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 1, "zoom"); if (lua_isnumber(L, -1)) zoom = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 1, "angle"); if (lua_isnumber(L, -1)) angle = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 1, "originX"); if (lua_isnumber(L, -1)) ox = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    lua_getfield(L, 1, "originY"); if (lua_isnumber(L, -1)) oy = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);

    Engine::get().get_batch2d().set_camera2d(x, y, zoom, angle, ox, oy);
    return 0;
}

static int l_graphics_reset_camera2d(lua_State* L) {
    (void)L;
    Engine::get().get_batch2d().reset_camera2d();
    return 0;
}

static int l_graphics_push_matrix(lua_State* L) {
    (void)L;
    Engine::get().get_mesh_renderer().push_matrix();
    return 0;
}

static int l_graphics_pop_matrix(lua_State* L) {
    (void)L;
    Engine::get().get_mesh_renderer().pop_matrix();
    return 0;
}

static int l_graphics_translate(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    Engine::get().get_mesh_renderer().translate(glm::vec3(x, y, z));
    return 0;
}

static int l_graphics_rotate(lua_State* L) {
    float angle = static_cast<float>(luaL_checknumber(L, 1));
    float ax = static_cast<float>(luaL_checknumber(L, 2));
    float ay = static_cast<float>(luaL_checknumber(L, 3));
    float az = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_mesh_renderer().rotate(angle, glm::vec3(ax, ay, az));
    return 0;
}

static int l_graphics_scale(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float sz = static_cast<float>(luaL_checknumber(L, 3));
    Engine::get().get_mesh_renderer().scale(glm::vec3(sx, sy, sz));
    return 0;
}

static int l_graphics_set_light(lua_State* L) {
    float dx = static_cast<float>(luaL_checknumber(L, 1));
    float dy = static_cast<float>(luaL_checknumber(L, 2));
    float dz = static_cast<float>(luaL_checknumber(L, 3));
    float lr = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float lg = static_cast<float>(luaL_optnumber(L, 5, 0.95));
    float lb = static_cast<float>(luaL_optnumber(L, 6, 0.9));
    float ar = static_cast<float>(luaL_optnumber(L, 7, 0.25));
    float ag = static_cast<float>(luaL_optnumber(L, 8, 0.25));
    float ab = static_cast<float>(luaL_optnumber(L, 9, 0.3));

    Engine::get().get_mesh_renderer().set_directional_light(
        glm::vec3(dx, dy, dz),
        glm::vec3(lr, lg, lb),
        glm::vec3(ar, ag, ab)
    );
    return 0;
}

static glm::vec4 parse_lua_color(lua_State* L, int idx, const glm::vec4& fallback) {
    if (lua_istable(L, idx)) {
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
        lua_rawgeti(L, idx, 1); if (lua_isnumber(L, -1)) r = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 2); if (lua_isnumber(L, -1)) g = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 3); if (lua_isnumber(L, -1)) b = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, idx, 4); if (lua_isnumber(L, -1)) a = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        return glm::vec4(r, g, b, a);
    }
    return fallback;
}

static int l_graphics_push_scissor(lua_State* L) {
    int x = static_cast<int>(luaL_checkinteger(L, 1));
    int y = static_cast<int>(luaL_checkinteger(L, 2));
    int w = static_cast<int>(luaL_checkinteger(L, 3));
    int h = static_cast<int>(luaL_checkinteger(L, 4));
    Engine::get().get_batch2d().push_scissor(x, y, w, h);
    return 0;
}

static int l_graphics_pop_scissor(lua_State* L) {
    (void)L;
    Engine::get().get_batch2d().pop_scissor();
    return 0;
}

static int l_graphics_push_matrix_2d(lua_State* L) {
    (void)L;
    Engine::get().get_batch2d().push_matrix_2d();
    return 0;
}

static int l_graphics_pop_matrix_2d(lua_State* L) {
    (void)L;
    Engine::get().get_batch2d().pop_matrix_2d();
    return 0;
}

static int l_graphics_translate_2d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_batch2d().translate_2d(x, y);
    return 0;
}

static int l_graphics_rotate_2d(lua_State* L) {
    float angle = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_batch2d().rotate_2d(angle);
    return 0;
}

static int l_graphics_scale_2d(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_batch2d().scale_2d(sx, sy);
    return 0;
}

static int l_graphics_draw_gradient_rect(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    glm::vec4 c_tl = parse_lua_color(L, 5, glm::vec4(1.0f));
    glm::vec4 c_tr = parse_lua_color(L, 6, c_tl);
    glm::vec4 c_br = parse_lua_color(L, 7, c_tr);
    glm::vec4 c_bl = parse_lua_color(L, 8, c_tl);
    Engine::get().get_batch2d().draw_gradient_rect(x, y, w, h, c_tl, c_tr, c_br, c_bl);
    return 0;
}

static int l_graphics_draw_gradient_h(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    glm::vec4 c_left = parse_lua_color(L, 5, glm::vec4(1.0f));
    glm::vec4 c_right = parse_lua_color(L, 6, c_left);
    Engine::get().get_batch2d().draw_gradient_h(x, y, w, h, c_left, c_right);
    return 0;
}

static int l_graphics_draw_gradient_v(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    glm::vec4 c_top = parse_lua_color(L, 5, glm::vec4(1.0f));
    glm::vec4 c_bot = parse_lua_color(L, 6, c_top);
    Engine::get().get_batch2d().draw_gradient_v(x, y, w, h, c_top, c_bot);
    return 0;
}

static int l_graphics_draw_polyline(lua_State* L) {
    if (!lua_istable(L, 1)) return 0;
    int len = static_cast<int>(lua_objlen(L, 1));
    if (len < 2) return 0;

    std::vector<glm::vec2> pts;
    lua_rawgeti(L, 1, 1);
    bool is_subtable = lua_istable(L, -1);
    lua_pop(L, 1);

    if (is_subtable) {
        pts.reserve(len);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 1, i);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                pts.push_back({x, y});
            }
            lua_pop(L, 1);
        }
    } else {
        pts.reserve(len / 2);
        for (int i = 1; i + 1 <= len; i += 2) {
            lua_rawgeti(L, 1, i);     float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, 1, i + 1); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            pts.push_back({x, y});
        }
    }

    float thick = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    bool loop = lua_toboolean(L, 3);
    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_polyline(pts, col, thick, loop);
    return 0;
}

static int l_graphics_draw_bezier(lua_State* L) {
    float x0 = static_cast<float>(luaL_checknumber(L, 1));
    float y0 = static_cast<float>(luaL_checknumber(L, 2));
    float x1 = static_cast<float>(luaL_checknumber(L, 3));
    float y1 = static_cast<float>(luaL_checknumber(L, 4));
    float x2 = static_cast<float>(luaL_checknumber(L, 5));
    float y2 = static_cast<float>(luaL_checknumber(L, 6));

    const glm::vec4& col = Engine::get().get_active_color();

    if (lua_isnumber(L, 7) && lua_isnumber(L, 8)) {
        float x3 = static_cast<float>(lua_tonumber(L, 7));
        float y3 = static_cast<float>(lua_tonumber(L, 8));
        float thick = static_cast<float>(luaL_optnumber(L, 9, 1.0));
        int segs = static_cast<int>(luaL_optinteger(L, 10, 24));
        Engine::get().get_batch2d().draw_bezier_cubic({x0, y0}, {x1, y1}, {x2, y2}, {x3, y3}, col, thick, segs);
    } else {
        float thick = static_cast<float>(luaL_optnumber(L, 7, 1.0));
        int segs = static_cast<int>(luaL_optinteger(L, 8, 16));
        Engine::get().get_batch2d().draw_bezier({x0, y0}, {x1, y1}, {x2, y2}, col, thick, segs);
    }
    return 0;
}

static int l_graphics_draw_pie(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float a1 = static_cast<float>(luaL_checknumber(L, 5));
    float a2 = static_cast<float>(luaL_checknumber(L, 6));
    int segs = static_cast<int>(luaL_optinteger(L, 7, 16));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_pie(cx, cy, radius, a1, a2, col, filled, segs);
    return 0;
}

static int l_graphics_draw_rounded_rect_ex(lua_State* L) {
    const char* mode_str = luaL_checkstring(L, 1);
    bool filled = (std::string(mode_str) == "fill");
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float rtl = static_cast<float>(luaL_checknumber(L, 6));
    float rtr = static_cast<float>(luaL_checknumber(L, 7));
    float rbr = static_cast<float>(luaL_checknumber(L, 8));
    float rbl = static_cast<float>(luaL_checknumber(L, 9));
    int segs = static_cast<int>(luaL_optinteger(L, 10, 8));

    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_rounded_rect_ex(x, y, w, h, rtl, rtr, rbr, rbl, col, filled, segs);
    return 0;
}

static int l_graphics_draw_texture_rot(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_checknumber(L, 4));
    float h = static_cast<float>(luaL_checknumber(L, 5));
    float angle = static_cast<float>(luaL_checknumber(L, 6));
    float ox = static_cast<float>(luaL_optnumber(L, 7, w * 0.5f));
    float oy = static_cast<float>(luaL_optnumber(L, 8, h * 0.5f));
    const glm::vec4& col = Engine::get().get_active_color();
    Engine::get().get_batch2d().draw_sprite(tex_id, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, col, angle, ox, oy);
    return 0;
}

static int l_graphics_draw_billboard_rot(lua_State* L) {
    GLuint tex_id = check_texture(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float w = static_cast<float>(luaL_checknumber(L, 5));
    float h = static_cast<float>(luaL_checknumber(L, 6));
    float angle = static_cast<float>(luaL_checknumber(L, 7));

    BillboardMode mode = BillboardMode::Spherical;
    if (lua_isstring(L, 8)) {
        std::string s(lua_tostring(L, 8));
        if (s == "cylindrical") mode = BillboardMode::Cylindrical;
    }

    glm::vec4 col = Engine::get().get_active_color();
    if (lua_istable(L, 9)) {
        col = parse_lua_color(L, 9, col);
    }

    Engine::get().get_mesh_renderer().draw_billboard_rot(tex_id, glm::vec3(x, y, z), glm::vec2(w, h), angle, mode, col);
    return 0;
}

static int l_graphics_draw_axes_3d(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float size = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    Engine::get().get_mesh_renderer().draw_axes_3d(glm::vec3(x, y, z), size);
    return 0;
}

static int l_graphics_draw_cube_wires(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float sx = static_cast<float>(luaL_checknumber(L, 4));
    float sy = static_cast<float>(luaL_checknumber(L, 5));
    float sz = static_cast<float>(luaL_checknumber(L, 6));
    glm::vec4 col = Engine::get().get_active_color();
    glm::vec3 rot(0.0f);
    if (lua_istable(L, 7)) {
        col = parse_lua_color(L, 7, col);
        if (lua_isnumber(L, 8)) rot.x = static_cast<float>(lua_tonumber(L, 8));
        if (lua_isnumber(L, 9)) rot.y = static_cast<float>(lua_tonumber(L, 9));
        if (lua_isnumber(L, 10)) rot.z = static_cast<float>(lua_tonumber(L, 10));
    } else {
        if (lua_isnumber(L, 7)) rot.x = static_cast<float>(lua_tonumber(L, 7));
        if (lua_isnumber(L, 8)) rot.y = static_cast<float>(lua_tonumber(L, 8));
        if (lua_isnumber(L, 9)) rot.z = static_cast<float>(lua_tonumber(L, 9));
    }
    Engine::get().get_mesh_renderer().draw_cube_wires(glm::vec3(x, y, z), glm::vec3(sx, sy, sz), col, rot);
    return 0;
}

static int l_graphics_draw_capsule_wires(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float half_h = static_cast<float>(luaL_checknumber(L, 5));
    glm::vec4 col = Engine::get().get_active_color();
    glm::vec3 rot(0.0f);
    if (lua_istable(L, 6)) {
        col = parse_lua_color(L, 6, col);
        if (lua_isnumber(L, 7)) rot.x = static_cast<float>(lua_tonumber(L, 7));
        if (lua_isnumber(L, 8)) rot.y = static_cast<float>(lua_tonumber(L, 8));
        if (lua_isnumber(L, 9)) rot.z = static_cast<float>(lua_tonumber(L, 9));
    } else {
        if (lua_isnumber(L, 6)) rot.x = static_cast<float>(lua_tonumber(L, 6));
        if (lua_isnumber(L, 7)) rot.y = static_cast<float>(lua_tonumber(L, 7));
        if (lua_isnumber(L, 8)) rot.z = static_cast<float>(lua_tonumber(L, 8));
    }
    Engine::get().get_mesh_renderer().draw_capsule_wires(glm::vec3(x, y, z), radius, half_h, col, rot);
    return 0;
}

static int l_graphics_draw_cylinder_wires(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float half_h = static_cast<float>(luaL_checknumber(L, 5));
    glm::vec4 col = Engine::get().get_active_color();
    glm::vec3 rot(0.0f);
    if (lua_istable(L, 6)) {
        col = parse_lua_color(L, 6, col);
        if (lua_isnumber(L, 7)) rot.x = static_cast<float>(lua_tonumber(L, 7));
        if (lua_isnumber(L, 8)) rot.y = static_cast<float>(lua_tonumber(L, 8));
        if (lua_isnumber(L, 9)) rot.z = static_cast<float>(lua_tonumber(L, 9));
    } else {
        if (lua_isnumber(L, 6)) rot.x = static_cast<float>(lua_tonumber(L, 6));
        if (lua_isnumber(L, 7)) rot.y = static_cast<float>(lua_tonumber(L, 7));
        if (lua_isnumber(L, 8)) rot.z = static_cast<float>(lua_tonumber(L, 8));
    }
    Engine::get().get_mesh_renderer().draw_cylinder_wires(glm::vec3(x, y, z), radius, half_h, col, rot);
    return 0;
}

static int l_graphics_draw_ray_3d(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float sz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float len = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    glm::vec4 col = Engine::get().get_active_color();
    if (lua_istable(L, 8)) {
        col = parse_lua_color(L, 8, col);
    }
    Engine::get().get_mesh_renderer().draw_ray_3d(glm::vec3(sx, sy, sz), glm::vec3(dx, dy, dz), len, col);
    return 0;
}

static int l_graphics_draw_skeleton(lua_State* L) {
    if (!lua_istable(L, 1)) return 0;
    std::vector<glm::vec3> positions;
    int n = static_cast<int>(lua_objlen(L, 1));
    positions.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); float px = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); float py = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 3); float pz = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            positions.emplace_back(px, py, pz);
        } else {
            positions.emplace_back(0.0f);
        }
        lua_pop(L, 1);
    }

    std::vector<std::pair<int, int>> connections;
    if (lua_istable(L, 2)) {
        int m = static_cast<int>(lua_objlen(L, 2));
        connections.reserve(m);
        for (int i = 1; i <= m; ++i) {
            lua_rawgeti(L, 2, i);
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1); int p = static_cast<int>(lua_tointeger(L, -1)) - 1; lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); int c = static_cast<int>(lua_tointeger(L, -1)) - 1; lua_pop(L, 1);
                connections.emplace_back(p, c);
            }
            lua_pop(L, 1);
        }
    }

    glm::vec4 col = Engine::get().get_active_color();
    if (lua_istable(L, 3)) {
        col = parse_lua_color(L, 3, col);
    }

    Engine::get().get_mesh_renderer().draw_skeleton_3d(positions, connections, col);
    return 0;
}

static int l_graphics_draw_segmented_mesh(lua_State* L) {
    if (!lua_istable(L, 1) || !lua_istable(L, 2)) return 0;
    std::vector<std::shared_ptr<Mesh3D>> meshes;
    int n = static_cast<int>(lua_objlen(L, 1));
    meshes.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_isuserdata(L, -1)) {
            auto* m = static_cast<LuaModel*>(luaL_checkudata(L, -1, "Graphics.Model"));
            std::shared_ptr<Mesh3D> mesh_ptr = nullptr;
            if (m && m->model && m->model->get_part_count() > 0) {
                const auto* p = m->model->get_part(0);
                if (p) mesh_ptr = p->mesh;
            }
            meshes.push_back(mesh_ptr);
        } else {
            meshes.push_back(nullptr);
        }
        lua_pop(L, 1);
    }

    std::vector<glm::mat4> transforms;
    transforms.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        glm::mat4 mat(1.0f);
        if (lua_istable(L, -1)) {
            int mlen = static_cast<int>(lua_objlen(L, -1));
            if (mlen >= 16) {
                float* p = &mat[0][0];
                for (int m = 1; m <= 16; ++m) {
                    lua_rawgeti(L, -1, m);
                    p[m - 1] = static_cast<float>(lua_tonumber(L, -1));
                    lua_pop(L, 1);
                }
            }
        }
        transforms.push_back(mat);
        lua_pop(L, 1);
    }

    std::vector<GLuint> textures;
    if (lua_istable(L, 3)) {
        int tlen = static_cast<int>(lua_objlen(L, 3));
        textures.reserve(tlen);
        for (int i = 1; i <= tlen; ++i) {
            lua_rawgeti(L, 3, i);
            textures.push_back(check_texture(L, -1));
            lua_pop(L, 1);
        }
    }

    Engine::get().get_mesh_renderer().draw_segmented_mesh(meshes, transforms, textures);
    return 0;
}

static int l_graphics_project(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    int vw = Engine::get().get_window().get_virtual_width();
    int vh = Engine::get().get_window().get_virtual_height();
    glm::vec2 screen_pos(0.0f);
    bool vis = Engine::get().get_mesh_renderer().project(glm::vec3(x, y, z), static_cast<float>(vw), static_cast<float>(vh), screen_pos);
    lua_pushnumber(L, screen_pos.x);
    lua_pushnumber(L, screen_pos.y);
    lua_pushboolean(L, vis);
    return 3;
}

static int l_graphics_unproject(lua_State* L) {
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    int vw = Engine::get().get_window().get_virtual_width();
    int vh = Engine::get().get_window().get_virtual_height();
    glm::vec3 orig(0.0f), dir(0.0f);
    Engine::get().get_mesh_renderer().unproject(glm::vec2(sx, sy), static_cast<float>(vw), static_cast<float>(vh), orig, dir);
    lua_pushnumber(L, orig.x);
    lua_pushnumber(L, orig.y);
    lua_pushnumber(L, orig.z);
    lua_pushnumber(L, dir.x);
    lua_pushnumber(L, dir.y);
    lua_pushnumber(L, dir.z);
    return 6;
}

static int l_graphics_get_white_texture(lua_State* L) {
    GLuint id = Engine::get().get_batch2d().get_white_texture_id();
    push_texture_userdata(L, id, 1, 1);
    return 1;
}

void register_graphics_bindings(lua_State* L) {
    register_texture_metatable(L);
    register_model_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    // Color & Canvas
    lua_pushcfunction(L, l_graphics_clear);
    lua_setfield(L, -2, "clear");

    lua_pushcfunction(L, l_graphics_set_color);
    lua_setfield(L, -2, "setColor");

    lua_pushcfunction(L, l_graphics_set_retro_effects);
    lua_setfield(L, -2, "setRetroEffects");

    // Camera & Lighting
    lua_pushcfunction(L, l_graphics_set_camera3d);
    lua_setfield(L, -2, "setCamera3d");

    lua_pushcfunction(L, l_graphics_get_camera_ray);
    lua_setfield(L, -2, "getCameraRay");

    lua_pushcfunction(L, l_graphics_set_light);
    lua_setfield(L, -2, "setLight");

    lua_pushcfunction(L, l_graphics_set_point_light);
    lua_setfield(L, -2, "setPointLight");

    lua_pushcfunction(L, l_graphics_set_point_light_enabled);
    lua_setfield(L, -2, "setPointLightEnabled");

    lua_pushcfunction(L, l_graphics_set_shading_mode);
    lua_setfield(L, -2, "setShadingMode");

    // Textures & Models
    lua_pushcfunction(L, l_graphics_load_texture);
    lua_setfield(L, -2, "loadTexture");

    lua_pushcfunction(L, l_graphics_get_texture_size);
    lua_setfield(L, -2, "getTextureSize");

    lua_pushcfunction(L, l_graphics_get_white_texture);
    lua_setfield(L, -2, "getWhiteTexture");
    
    lua_pushcfunction(L, l_graphics_load_model);
    lua_setfield(L, -2, "loadModel");

    lua_pushcfunction(L, l_graphics_create_mesh);
    lua_setfield(L, -2, "createMesh");

    // 3D Rendering
    lua_pushcfunction(L, l_graphics_draw_model);
    lua_setfield(L, -2, "drawModel");

    lua_pushcfunction(L, l_graphics_draw_model_node);
    lua_setfield(L, -2, "drawModelNode");
    lua_pushcfunction(L, l_graphics_draw_model_node);
    lua_setfield(L, -2, "draw_model_node");

    lua_pushcfunction(L, l_graphics_draw_cube);
    lua_setfield(L, -2, "drawCube");

    lua_pushcfunction(L, l_graphics_draw_plane);
    lua_setfield(L, -2, "drawPlane");

    lua_pushcfunction(L, l_graphics_draw_sphere);
    lua_setfield(L, -2, "drawSphere");

    lua_pushcfunction(L, l_graphics_draw_cylinder);
    lua_setfield(L, -2, "drawCylinder");

    lua_pushcfunction(L, l_graphics_draw_cone);
    lua_setfield(L, -2, "drawCone");

    lua_pushcfunction(L, l_graphics_draw_pyramid);
    lua_setfield(L, -2, "drawPyramid");

    lua_pushcfunction(L, l_graphics_draw_torus);
    lua_setfield(L, -2, "drawTorus");

    lua_pushcfunction(L, l_graphics_draw_capsule);
    lua_setfield(L, -2, "drawCapsule");

    lua_pushcfunction(L, l_graphics_draw_billboard);
    lua_setfield(L, -2, "drawBillboard");

    lua_pushcfunction(L, l_graphics_draw_line_3d);
    lua_setfield(L, -2, "drawLine3d");

    lua_pushcfunction(L, l_graphics_draw_lines_3d);
    lua_setfield(L, -2, "drawLines3d");

    lua_pushcfunction(L, l_graphics_draw_grid_3d);
    lua_setfield(L, -2, "drawGrid3d");

    lua_pushcfunction(L, l_graphics_draw_triangle_3d);
    lua_setfield(L, -2, "drawTriangle3d");

    lua_pushcfunction(L, l_graphics_draw_quad_3d);
    lua_setfield(L, -2, "drawQuad3d");

    // 2D Rendering
    lua_pushcfunction(L, l_graphics_draw_sprite);
    lua_setfield(L, -2, "drawSprite");

    lua_pushcfunction(L, l_graphics_draw_sprite_part);
    lua_setfield(L, -2, "drawSpritePart");
    
    lua_pushcfunction(L, l_graphics_draw_sprite_tiled);
    lua_setfield(L, -2, "drawSpriteTiled");

    lua_pushcfunction(L, l_graphics_draw_sprite_9slice);
    lua_setfield(L, -2, "drawSprite9slice");

    lua_pushcfunction(L, l_graphics_draw_point);
    lua_setfield(L, -2, "drawPoint");

    lua_pushcfunction(L, l_graphics_draw_line);
    lua_setfield(L, -2, "drawLine");
    
    lua_pushcfunction(L, l_graphics_draw_rect);
    lua_setfield(L, -2, "drawRect");

    lua_pushcfunction(L, l_graphics_draw_rounded_rect);
    lua_setfield(L, -2, "drawRoundedRect");
    
    lua_pushcfunction(L, l_graphics_draw_triangle);
    lua_setfield(L, -2, "drawTriangle");

    lua_pushcfunction(L, l_graphics_draw_quad);
    lua_setfield(L, -2, "drawQuad");

    lua_pushcfunction(L, l_graphics_draw_polygon);
    lua_setfield(L, -2, "drawPolygon");

    lua_pushcfunction(L, l_graphics_draw_circle);
    lua_setfield(L, -2, "drawCircle");

    lua_pushcfunction(L, l_graphics_draw_ellipse);
    lua_setfield(L, -2, "drawEllipse");
    
    lua_pushcfunction(L, l_graphics_draw_arc);
    lua_setfield(L, -2, "drawArc");

    lua_pushcfunction(L, l_graphics_draw_ring);
    lua_setfield(L, -2, "drawRing");

    lua_pushcfunction(L, l_graphics_draw_text);
    lua_setfield(L, -2, "drawText");
    
    lua_pushcfunction(L, l_graphics_get_text_width);
    lua_setfield(L, -2, "getTextWidth");

    lua_pushcfunction(L, l_graphics_get_text_height);
    lua_setfield(L, -2, "getTextHeight");

    lua_pushcfunction(L, l_graphics_set_blend_mode);
    lua_setfield(L, -2, "setBlendMode");

    lua_pushcfunction(L, l_graphics_set_scissor);
    lua_setfield(L, -2, "setScissor");

    lua_pushcfunction(L, l_graphics_reset_scissor);
    lua_setfield(L, -2, "resetScissor");

    lua_pushcfunction(L, l_graphics_set_camera2d);
    lua_setfield(L, -2, "setCamera2d");

    lua_pushcfunction(L, l_graphics_reset_camera2d);
    lua_setfield(L, -2, "resetCamera2d");

    // Transform Stack
    lua_pushcfunction(L, l_graphics_push_matrix);
    lua_setfield(L, -2, "pushMatrix");

    lua_pushcfunction(L, l_graphics_pop_matrix);
    lua_setfield(L, -2, "popMatrix");

    lua_pushcfunction(L, l_graphics_translate);
    lua_setfield(L, -2, "translate");

    lua_pushcfunction(L, l_graphics_rotate);
    lua_setfield(L, -2, "rotate");

    lua_pushcfunction(L, l_graphics_scale);
    lua_setfield(L, -2, "scale");

    // 2D Matrix Stack
    lua_pushcfunction(L, l_graphics_push_matrix_2d);
    lua_setfield(L, -2, "pushMatrix2d");
    
    lua_pushcfunction(L, l_graphics_pop_matrix_2d);
    lua_setfield(L, -2, "popMatrix2d");

    lua_pushcfunction(L, l_graphics_translate_2d);
    lua_setfield(L, -2, "translate2d");

    lua_pushcfunction(L, l_graphics_rotate_2d);
    lua_setfield(L, -2, "rotate2d");

    lua_pushcfunction(L, l_graphics_scale_2d);
    lua_setfield(L, -2, "scale2d");

    // Scissor Stack
    lua_pushcfunction(L, l_graphics_push_scissor);
    lua_setfield(L, -2, "pushScissor");

    lua_pushcfunction(L, l_graphics_pop_scissor);
    lua_setfield(L, -2, "popScissor");

    // Advanced 2D Primitives
    lua_pushcfunction(L, l_graphics_draw_gradient_rect);
    lua_setfield(L, -2, "drawGradientRect");

    lua_pushcfunction(L, l_graphics_draw_gradient_h);
    lua_setfield(L, -2, "drawGradientH");

    lua_pushcfunction(L, l_graphics_draw_gradient_v);
    lua_setfield(L, -2, "drawGradientV");

    lua_pushcfunction(L, l_graphics_draw_polyline);
    lua_setfield(L, -2, "drawPolyline");
    
    lua_pushcfunction(L, l_graphics_draw_bezier);
    lua_setfield(L, -2, "drawBezier");

    lua_pushcfunction(L, l_graphics_draw_pie);
    lua_setfield(L, -2, "drawPie");

    lua_pushcfunction(L, l_graphics_draw_rounded_rect_ex);
    lua_setfield(L, -2, "drawRoundedRectEx");

    lua_pushcfunction(L, l_graphics_draw_texture_rot);
    lua_setfield(L, -2, "drawTextureRot");

    // Advanced 3D Helpers
    lua_pushcfunction(L, l_graphics_draw_billboard_rot);
    lua_setfield(L, -2, "drawBillboardRot");

    lua_pushcfunction(L, l_graphics_draw_axes_3d);
    lua_setfield(L, -2, "drawAxes3d");

    lua_pushcfunction(L, l_graphics_draw_cube_wires);
    lua_setfield(L, -2, "drawCubeWires");

    lua_pushcfunction(L, l_graphics_draw_capsule_wires);
    lua_setfield(L, -2, "drawCapsuleWires");

    lua_pushcfunction(L, l_graphics_draw_cylinder_wires);
    lua_setfield(L, -2, "drawCylinderWires");

    lua_pushcfunction(L, l_graphics_draw_ray_3d);
    lua_setfield(L, -2, "drawRay3d");

    lua_pushcfunction(L, l_graphics_draw_ray_3d);
    lua_setfield(L, -2, "drawRay");

    lua_pushcfunction(L, l_graphics_draw_skeleton);
    lua_setfield(L, -2, "drawSkeleton");

    lua_pushcfunction(L, l_graphics_draw_segmented_mesh);
    lua_setfield(L, -2, "drawSegmentedMesh");

    lua_pushcfunction(L, l_graphics_project);
    lua_setfield(L, -2, "project");

    lua_pushcfunction(L, l_graphics_unproject);
    lua_setfield(L, -2, "unproject");

    lua_setfield(L, -2, "graphics");
    lua_pop(L, 1);
}

} // namespace crayon
