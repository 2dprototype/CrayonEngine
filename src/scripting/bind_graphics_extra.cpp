// Extra graphics bindings: pixel-data textures, texture sampling state and frame capture.
//   graphics.newImageData(w, h [, r,g,b,a])      graphics.loadImageData(path)
//   graphics.newTexture(imageData [, {filter=, wrap=}])
//   graphics.screenshot(path)   graphics.captureFrame()   graphics.getCapture()
//   graphics.setTextureFilter(tex, mode)   graphics.setTextureWrap(tex, mode)
//   graphics.renderLightmap(img, ambient, lights [, view])   graphics.beginMask/useMask/endMask
//   imageData:getWidth/getHeight/getSize/getPixel/setPixel/fill/paste/copy/mapPixels/save
//   texture:setFilter/getFilter/setWrap/getWrap/update/destroy
#include "bind_graphics_extra.hpp"
#include "../core/engine.hpp"
#include "../core/log.hpp"
#include "../graphics/image_data.hpp"
#include "../graphics/lightmap2d.hpp"
#include "../graphics/model3d.hpp"
#include "../graphics/skybox.hpp"
#include "../graphics/static_batch3d.hpp"
#include "../graphics/terrain.hpp"
#include "../physics/physics_system.hpp"
#include "../graphics/texture.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <memory>
#include <new>
#include <vector>
#include <string>

namespace crayon {

GLuint crayon_lua_check_texture(lua_State* L, int idx);
void crayon_lua_push_texture(lua_State* L, GLuint id, int w, int h);
std::shared_ptr<Model3D> crayon_lua_check_model(lua_State* L, int idx);
void crayon_lua_push_physics_body(lua_State* L, uint32_t id);

namespace {

struct LuaTextureLayout { GLuint id; int width; int height; };   // mirrors LuaTexture in bind_graphics.cpp
struct LuaImageData { std::shared_ptr<ImageData> img; };

const char* kImageMeta = "Graphics.ImageData";

std::shared_ptr<ImageData> check_image(lua_State* L, int idx) {
    auto* u = static_cast<LuaImageData*>(luaL_checkudata(L, idx, kImageMeta));
    if (!u || !u->img) luaL_error(L, "ImageData is invalid");
    return u->img;
}

void push_image(lua_State* L, std::shared_ptr<ImageData> img) {
    void* mem = lua_newuserdata(L, sizeof(LuaImageData));
    new (mem) LuaImageData{std::move(img)};
    luaL_getmetatable(L, kImageMeta);
    lua_setmetatable(L, -2);
}

float num(lua_State* L, int idx, double def) { return static_cast<float>(luaL_optnumber(L, idx, def)); }

// ---------------- ImageData methods ----------------
int img_gc(lua_State* L) {
    auto* u = static_cast<LuaImageData*>(lua_touserdata(L, 1));
    if (u) u->~LuaImageData();
    return 0;
}
int img_get_width(lua_State* L) { lua_pushinteger(L, check_image(L, 1)->width()); return 1; }
int img_get_height(lua_State* L) { lua_pushinteger(L, check_image(L, 1)->height()); return 1; }
int img_get_size(lua_State* L) {
    auto img = check_image(L, 1);
    lua_pushinteger(L, img->width()); lua_pushinteger(L, img->height());
    return 2;
}
int img_get_pixel(lua_State* L) {
    auto img = check_image(L, 1);
    float r, g, b, a;
    img->get_pixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)), r, g, b, a);
    lua_pushnumber(L, r); lua_pushnumber(L, g); lua_pushnumber(L, b); lua_pushnumber(L, a);
    return 4;
}
int img_set_pixel(lua_State* L) {
    check_image(L, 1)->set_pixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)),
                                 num(L, 4, 1), num(L, 5, 1), num(L, 6, 1), num(L, 7, 1));
    return 0;
}
int img_fill(lua_State* L) {
    check_image(L, 1)->fill(num(L, 2, 0), num(L, 3, 0), num(L, 4, 0), num(L, 5, 0));
    return 0;
}
// imageData:paste(src, dx, dy [, sx, sy, sw, sh])
int img_paste(lua_State* L) {
    auto dst = check_image(L, 1);
    auto src = check_image(L, 2);
    dst->paste(*src, static_cast<int>(luaL_optinteger(L, 3, 0)), static_cast<int>(luaL_optinteger(L, 4, 0)),
               static_cast<int>(luaL_optinteger(L, 5, 0)), static_cast<int>(luaL_optinteger(L, 6, 0)),
               static_cast<int>(luaL_optinteger(L, 7, 0)), static_cast<int>(luaL_optinteger(L, 8, 0)));
    return 0;
}
int img_copy(lua_State* L) { push_image(L, check_image(L, 1)->clone()); return 1; }
int img_save(lua_State* L) {
    auto img = check_image(L, 1);
    const char* path = luaL_checkstring(L, 2);
    lua_pushboolean(L, img->save_png(path));
    return 1;
}
// imageData:mapPixels(function(x, y, r, g, b, a) return r, g, b, a end)
int img_map_pixels(lua_State* L) {
    auto img = check_image(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    for (int y = 0; y < img->height(); ++y) {
        for (int x = 0; x < img->width(); ++x) {
            float r, g, b, a;
            img->get_pixel(x, y, r, g, b, a);
            lua_pushvalue(L, 2);
            lua_pushinteger(L, x); lua_pushinteger(L, y);
            lua_pushnumber(L, r); lua_pushnumber(L, g); lua_pushnumber(L, b); lua_pushnumber(L, a);
            lua_call(L, 6, 4);
            if (!lua_isnil(L, -4)) {
                img->set_pixel(x, y, static_cast<float>(lua_tonumber(L, -4)), static_cast<float>(lua_tonumber(L, -3)),
                               static_cast<float>(lua_tonumber(L, -2)), static_cast<float>(lua_tonumber(L, -1)));
            }
            lua_pop(L, 4);
        }
    }
    return 0;
}
int img_tostring(lua_State* L) {
    auto* u = static_cast<LuaImageData*>(lua_touserdata(L, 1));
    if (u && u->img) lua_pushfstring(L, "Graphics.ImageData(%dx%d)", u->img->width(), u->img->height());
    else lua_pushstring(L, "Graphics.ImageData(invalid)");
    return 1;
}

// ---------------- sampling-state helpers ----------------
// Accepts "nearest" | "linear" | "mipmap" (also true/false: true = nearest).
bool parse_filter(lua_State* L, int idx, bool& nearest, bool& mips) {
    if (lua_isboolean(L, idx)) { nearest = lua_toboolean(L, idx) != 0; mips = !nearest; return true; }
    std::string f = luaL_checkstring(L, idx);
    if (f == "nearest" || f == "point") { nearest = true; mips = false; return true; }
    if (f == "linear" || f == "bilinear") { nearest = false; mips = false; return true; }
    if (f == "mipmap" || f == "trilinear" || f == "smooth") { nearest = false; mips = true; return true; }
    luaL_error(L, "unknown texture filter '%s' (use nearest, linear or mipmap)", f.c_str());
    return false;
}
TextureWrap parse_wrap(lua_State* L, int idx) {
    std::string w = luaL_checkstring(L, idx);
    if (w == "repeat" || w == "tile") return TextureWrap::Repeat;
    if (w == "clamp" || w == "edge") return TextureWrap::Clamp;
    if (w == "mirror" || w == "mirrored") return TextureWrap::Mirror;
    luaL_error(L, "unknown texture wrap '%s' (use repeat, clamp or mirror)", w.c_str());
    return TextureWrap::Repeat;
}

void apply_texture_opts(lua_State* L, GLuint id, int opts_idx) {
    Texture* tex = Texture::find(id);
    if (!tex || !lua_istable(L, opts_idx)) return;
    lua_getfield(L, opts_idx, "filter");
    if (!lua_isnil(L, -1)) { bool n, m; parse_filter(L, -1, n, m); tex->set_filter(n, m); }
    lua_pop(L, 1);
    lua_getfield(L, opts_idx, "wrap");
    if (!lua_isnil(L, -1)) tex->set_wrap(parse_wrap(L, -1));
    lua_pop(L, 1);
}

// ---------------- Texture methods ----------------
int tex_set_filter(lua_State* L) {
    GLuint id = crayon_lua_check_texture(L, 1);
    bool n, m; parse_filter(L, 2, n, m);
    if (Texture* t = Texture::find(id)) t->set_filter(n, m);
    return 0;
}
int tex_get_filter(lua_State* L) {
    Texture* t = Texture::find(crayon_lua_check_texture(L, 1));
    lua_pushstring(L, !t ? "nearest" : t->is_nearest() ? "nearest" : t->has_mipmaps() ? "mipmap" : "linear");
    return 1;
}
int tex_set_wrap(lua_State* L) {
    GLuint id = crayon_lua_check_texture(L, 1);
    TextureWrap w = parse_wrap(L, 2);
    if (Texture* t = Texture::find(id)) t->set_wrap(w);
    return 0;
}
int tex_get_wrap(lua_State* L) {
    Texture* t = Texture::find(crayon_lua_check_texture(L, 1));
    TextureWrap w = t ? t->get_wrap() : TextureWrap::Repeat;
    lua_pushstring(L, w == TextureWrap::Clamp ? "clamp" : w == TextureWrap::Mirror ? "mirror" : "repeat");
    return 1;
}
// texture:update(imageData)  -- same size as the texture, RGBA textures created by newTexture
int tex_update(lua_State* L) {
    GLuint id = crayon_lua_check_texture(L, 1);
    auto img = check_image(L, 2);
    Texture* t = Texture::find(id);
    if (!t) return luaL_error(L, "texture is not valid");
    if (!t->update_rgba(img->data(), img->width(), img->height()))
        return luaL_error(L, "texture:update needs an RGBA texture of the same size as the ImageData");
    return 0;
}
int tex_destroy(lua_State* L) {
    auto* ud = static_cast<LuaTextureLayout*>(luaL_checkudata(L, 1, "Graphics.Texture"));
    if (ud && ud->id != 0) {
        Engine::get().release_user_texture(ud->id);
        ud->id = 0; ud->width = 0; ud->height = 0;
    }
    return 0;
}

// ---------------- graphics.* ----------------
int g_new_image_data(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    if (w <= 0 || h <= 0 || int64_t(w) * h > 64 * 1024 * 1024) return luaL_error(L, "invalid ImageData size %dx%d", w, h);
    auto img = std::make_shared<ImageData>(w, h);
    if (!lua_isnoneornil(L, 3)) img->fill(num(L, 3, 0), num(L, 4, 0), num(L, 5, 0), num(L, 6, 1));
    push_image(L, std::move(img));
    return 1;
}
int g_load_image_data(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    auto img = ImageData::load(path);
    if (!img) {
        lua_pushnil(L);
        lua_pushfstring(L, "could not load image '%s'", path);
        return 2;
    }
    push_image(L, std::move(img));
    return 1;
}
int g_new_texture(lua_State* L) {
    auto img = check_image(L, 1);
    bool nearest = true;
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "filter");
        if (!lua_isnil(L, -1)) { bool m; parse_filter(L, -1, nearest, m); }
        lua_pop(L, 1);
    }
    auto tex = std::make_shared<Texture>();
    if (!tex->load_from_memory(img->data(), img->width(), img->height(), 4, nearest))
        return luaL_error(L, "could not create texture");
    GLuint id = Engine::get().adopt_user_texture(tex);
    if (lua_istable(L, 2)) apply_texture_opts(L, id, 2);
    crayon_lua_push_texture(L, id, img->width(), img->height());
    return 1;
}
int g_screenshot(lua_State* L) {
    Engine::get().request_screenshot(luaL_checkstring(L, 1));
    return 0;
}
int g_capture_frame(lua_State*) {
    Engine::get().request_capture();
    return 0;
}
int g_get_capture(lua_State* L) {
    auto img = Engine::get().get_last_capture();
    if (!img) { lua_pushnil(L); return 1; }
    push_image(L, img->clone());
    return 1;
}
int g_set_texture_filter(lua_State* L) { return tex_set_filter(L); }
int g_set_texture_wrap(lua_State* L) { return tex_set_wrap(L); }

// ---------------- 2D lights + masks ----------------
float field_num(lua_State* L, int tbl, const char* key, float def) {
    lua_getfield(L, tbl, key);
    float v = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : def;
    lua_pop(L, 1);
    return v;
}

// graphics.renderLightmap(imageData, {ambR, ambG, ambB}, lights [, view])
//   lights[i] = { x, y, radius, color = {r,g,b}, intensity = 1, falloff = 2,
//                 spot = false, direction = 0, cone = 0.6, softness = 0.25 }
//   view      = { offsetX = 0, offsetY = 0, scaleX = 1, scaleY = 1 }   (world -> light-map pixels)
int g_render_lightmap(lua_State* L) {
    auto img = check_image(L, 1);
    float amb[3] = {0.1f, 0.1f, 0.1f};
    if (lua_istable(L, 2)) {
        for (int i = 0; i < 3; ++i) {
            lua_rawgeti(L, 2, i + 1);
            if (lua_isnumber(L, -1)) amb[i] = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
    }
    std::vector<Light2D> lights;
    if (lua_istable(L, 3)) {
        const int n = static_cast<int>(lua_objlen(L, 3));
        lights.reserve(n);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, 3, i);
            if (lua_istable(L, -1)) {
                const int t = lua_gettop(L);
                Light2D l;
                l.x = field_num(L, t, "x", 0); l.y = field_num(L, t, "y", 0);
                l.radius = field_num(L, t, "radius", 100); l.intensity = field_num(L, t, "intensity", 1);
                l.falloff = field_num(L, t, "falloff", 2);
                l.direction = field_num(L, t, "direction", 0); l.cone = field_num(L, t, "cone", 0.6f);
                l.softness = field_num(L, t, "softness", 0.25f);
                lua_getfield(L, t, "spot"); l.spot = lua_toboolean(L, -1) != 0; lua_pop(L, 1);
                lua_getfield(L, t, "color");
                if (lua_istable(L, -1)) {
                    float* c[3] = {&l.r, &l.g, &l.b};
                    for (int k = 0; k < 3; ++k) {
                        lua_rawgeti(L, -1, k + 1);
                        if (lua_isnumber(L, -1)) *c[k] = static_cast<float>(lua_tonumber(L, -1));
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
                lights.push_back(l);
            }
            lua_pop(L, 1);
        }
    }
    LightMapView view;
    if (lua_istable(L, 4)) {
        view.offset_x = field_num(L, 4, "offsetX", 0); view.offset_y = field_num(L, 4, "offsetY", 0);
        view.scale_x = field_num(L, 4, "scaleX", 1); view.scale_y = field_num(L, 4, "scaleY", 1);
    }
    render_lightmap(*img, amb[0], amb[1], amb[2], lights, view);
    return 0;
}

int g_begin_mask(lua_State*) { Engine::get().get_batch2d().begin_mask(); return 0; }
int g_use_mask(lua_State* L) { Engine::get().get_batch2d().use_mask(lua_toboolean(L, 1) != 0); return 0; }
int g_end_mask(lua_State*) { Engine::get().get_batch2d().end_mask(); return 0; }

// ---------------- StaticBatch3D ----------------
// local batch = graphics.newStaticBatch(chunkSize)   (default 32)
// batch:add(model, x,y,z, rx,ry,rz, sx,sy,sz, texture)   same arguments as drawModel
// batch:build()   batch:draw()   batch:clear()   batch:getStats() -> instances, groups, vertices, triangles
struct LuaStaticBatch { std::shared_ptr<StaticBatch3D> batch; };
const char* kBatchMeta = "Graphics.StaticBatch";

std::shared_ptr<StaticBatch3D> check_batch(lua_State* L, int idx) {
    auto* u = static_cast<LuaStaticBatch*>(luaL_checkudata(L, idx, kBatchMeta));
    if (!u || !u->batch) luaL_error(L, "StaticBatch is invalid");
    return u->batch;
}
int g_new_static_batch(lua_State* L) {
    void* mem = lua_newuserdata(L, sizeof(LuaStaticBatch));
    new (mem) LuaStaticBatch{std::make_shared<StaticBatch3D>(num(L, 1, 32.0))};
    luaL_getmetatable(L, kBatchMeta);
    lua_setmetatable(L, -2);
    return 1;
}
int batch_gc(lua_State* L) {
    auto* u = static_cast<LuaStaticBatch*>(lua_touserdata(L, 1));
    if (u) u->~LuaStaticBatch();
    return 0;
}
int batch_add(lua_State* L) {
    auto batch = check_batch(L, 1);
    auto model = crayon_lua_check_model(L, 2);
    if (!model) return luaL_error(L, "batch:add needs a model");
    glm::vec3 pos(num(L, 3, 0), num(L, 4, 0), num(L, 5, 0));
    float rx = num(L, 6, 0), ry = num(L, 7, 0), rz = num(L, 8, 0);
    glm::vec3 sc(num(L, 9, 1), num(L, 10, 1), num(L, 11, 1));
    GLuint tex = lua_isnoneornil(L, 12) ? 0 : crayon_lua_check_texture(L, 12);
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);       // same composition as graphics.drawModel
    if (rz != 0.0f) m = glm::rotate(m, rz, glm::vec3(0, 0, 1));
    if (ry != 0.0f) m = glm::rotate(m, ry, glm::vec3(0, 1, 0));
    if (rx != 0.0f) m = glm::rotate(m, rx, glm::vec3(1, 0, 0));
    m = glm::scale(m, sc);
    lua_pushinteger(L, static_cast<lua_Integer>(batch->add(*model, m, tex)));
    return 1;
}
int batch_build(lua_State* L) { check_batch(L, 1)->build(); return 0; }
int batch_draw(lua_State* L) { check_batch(L, 1)->draw(Engine::get().get_mesh_renderer()); return 0; }
int batch_clear(lua_State* L) { check_batch(L, 1)->clear(); return 0; }
int batch_stats(lua_State* L) {
    auto b = check_batch(L, 1);
    const auto& s = b->stats();
    lua_pushinteger(L, static_cast<lua_Integer>(s.instances)); lua_pushinteger(L, static_cast<lua_Integer>(s.groups));
    lua_pushinteger(L, static_cast<lua_Integer>(s.vertices)); lua_pushinteger(L, static_cast<lua_Integer>(s.triangles));
    return 4;
}

// ---------------- Skybox ----------------
// graphics.loadSkybox("panorama.jpg" | ImageData [, faceSize])      equirectangular panorama
// graphics.loadSkybox({ "px","nx","py","ny","pz","nz" })           six images (paths or ImageData) in +X -X +Y -Y +Z -Z order
// graphics.loadSkybox({ right=..., left=..., top=..., bottom=..., front=..., back=... })
// graphics.drawSkybox(sky [, r, g, b])    fills only pixels nothing else covers (call first or last)
struct LuaSkybox { std::shared_ptr<Skybox> sky; };
const char* kSkyMeta = "Graphics.Skybox";

std::shared_ptr<Skybox> check_sky(lua_State* L, int idx) {
    auto* u = static_cast<LuaSkybox*>(luaL_checkudata(L, idx, kSkyMeta));
    if (!u || !u->sky) luaL_error(L, "Skybox is invalid");
    return u->sky;
}
int g_load_skybox(lua_State* L) {
    auto sky = std::make_shared<Skybox>();
    bool ok = false;
    if (lua_isstring(L, 1)) {
        ok = sky->load_equirect_file(lua_tostring(L, 1), static_cast<int>(luaL_optinteger(L, 2, 0)));
    } else if (lua_isuserdata(L, 1)) {                       // one ImageData = equirectangular panorama
        ok = sky->load_equirect(*check_image(L, 1), static_cast<int>(luaL_optinteger(L, 2, 0)));
    } else if (lua_istable(L, 1)) {
        static const char* names[6] = {"right", "left", "top", "bottom", "front", "back"};
        std::array<std::string, 6> paths;
        std::array<std::shared_ptr<ImageData>, 6> images;
        bool from_images = false;
        for (int i = 0; i < 6; ++i) {
            lua_rawgeti(L, 1, i + 1);
            if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_getfield(L, 1, names[i]); }
            if (lua_isuserdata(L, -1)) { images[i] = check_image(L, -1); from_images = true; }
            else if (lua_isstring(L, -1)) paths[i] = lua_tostring(L, -1);
            else return luaL_error(L, "loadSkybox: face %d (%s) is missing", i + 1, names[i]);
            lua_pop(L, 1);
        }
        if (from_images) {
            for (int i = 0; i < 6; ++i) if (!images[i]) return luaL_error(L, "loadSkybox: mix of paths and ImageData is not supported");
            ok = sky->load_faces(images);
        } else {
            ok = sky->load_face_files(paths);
        }
    } else {
        return luaL_argerror(L, 1, "expected a panorama path/ImageData or a table of six faces (paths or ImageData)");
    }
    if (!ok) {
        lua_pushnil(L);
        lua_pushstring(L, "could not load skybox (check the paths; faces must be square and equal in size)");
        return 2;
    }
    void* mem = lua_newuserdata(L, sizeof(LuaSkybox));
    new (mem) LuaSkybox{std::move(sky)};
    luaL_getmetatable(L, kSkyMeta);
    lua_setmetatable(L, -2);
    return 1;
}
int g_draw_skybox(lua_State* L) {
    auto sky = check_sky(L, 1);
    Engine::get().get_mesh_renderer().draw_skybox(*sky, glm::vec3(num(L, 2, 1), num(L, 3, 1), num(L, 4, 1)));
    return 0;
}
int sky_gc(lua_State* L) {
    auto* u = static_cast<LuaSkybox*>(lua_touserdata(L, 1));
    if (u) u->~LuaSkybox();
    return 0;
}
int sky_face_size(lua_State* L) { lua_pushinteger(L, check_sky(L, 1)->face_size()); return 1; }
int sky_draw(lua_State* L) { return g_draw_skybox(L); }

// ---------------- Terrain ----------------
// local t = graphics.createTerrain(source, { width = 200, height = 30, depth = 200,
//                                            x = -100, y = 0, z = -100, uvRepeat = 32 | {32, 32},
//                                            chunkCells = 32, colorLow = {r,g,b}, colorHigh = {r,g,b} })
//   source: ImageData / image path (brightness = height) or a table of rows { {0,.1,...}, {...} } (0..1)
// t:draw([texture [, r,g,b,a]])     t:getHeightAt(x,z)     t:getNormalAt(x,z) -> nx,ny,nz
// t:contains(x,z)   t:raycast(ox,oy,oz, dx,dy,dz [, maxDist]) -> hx,hy,hz | nil   t:getInfo() -> columns, rows, chunks, triangles
struct LuaTerrain { std::shared_ptr<Terrain> terrain; };
const char* kTerrainMeta = "Graphics.Terrain";

std::shared_ptr<Terrain> check_terrain(lua_State* L, int idx) {
    auto* u = static_cast<LuaTerrain*>(luaL_checkudata(L, idx, kTerrainMeta));
    if (!u || !u->terrain) luaL_error(L, "Terrain is invalid");
    return u->terrain;
}
bool read_color3(lua_State* L, int tbl, const char* key, glm::vec4& out) {
    lua_getfield(L, tbl, key);
    bool ok = lua_istable(L, -1);
    if (ok) {
        float c[3] = {out.r, out.g, out.b};
        for (int i = 0; i < 3; ++i) { lua_rawgeti(L, -1, i + 1); if (lua_isnumber(L, -1)) c[i] = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1); }
        out = glm::vec4(c[0], c[1], c[2], 1.0f);
    }
    lua_pop(L, 1);
    return ok;
}
int g_create_terrain(lua_State* L) {
    Terrain::Options o;
    if (lua_istable(L, 2)) {
        o.size = glm::vec3(field_num(L, 2, "width", 100), field_num(L, 2, "height", 10), field_num(L, 2, "depth", 100));
        o.origin = glm::vec3(field_num(L, 2, "x", 0), field_num(L, 2, "y", 0), field_num(L, 2, "z", 0));
        o.chunk_cells = static_cast<int>(field_num(L, 2, "chunkCells", 32));
        lua_getfield(L, 2, "uvRepeat");
        if (lua_isnumber(L, -1)) { o.uv_repeat_x = o.uv_repeat_z = static_cast<float>(lua_tonumber(L, -1)); }
        else if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); if (lua_isnumber(L, -1)) o.uv_repeat_x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); if (lua_isnumber(L, -1)) o.uv_repeat_z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        }
        lua_pop(L, 1);
        glm::vec4 lo(1, 1, 1, 0), hi(1, 1, 1, 0);
        bool a = read_color3(L, 2, "colorLow", lo), b = read_color3(L, 2, "colorHigh", hi);
        if (a || b) { o.color_low = a ? lo : glm::vec4(1); o.color_high = b ? hi : glm::vec4(1); }
    }
    auto terrain = std::make_shared<Terrain>();
    bool ok = false;
    if (lua_isstring(L, 1)) {
        auto img = ImageData::load(lua_tostring(L, 1));
        if (!img) return luaL_error(L, "createTerrain: could not load heightmap '%s'", lua_tostring(L, 1));
        ok = terrain->build_from_image(*img, o);
    } else if (lua_isuserdata(L, 1)) {
        ok = terrain->build_from_image(*check_image(L, 1), o);
    } else if (lua_istable(L, 1)) {
        const int rows = static_cast<int>(lua_objlen(L, 1));
        std::vector<float> h; int cols = 0;
        for (int r = 1; r <= rows; ++r) {
            lua_rawgeti(L, 1, r);
            if (!lua_istable(L, -1)) return luaL_error(L, "createTerrain: row %d is not a table", r);
            const int c = static_cast<int>(lua_objlen(L, -1));
            if (r == 1) cols = c; else if (c != cols) return luaL_error(L, "createTerrain: rows must all have %d values", cols);
            for (int i = 1; i <= c; ++i) { lua_rawgeti(L, -1, i); h.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); }
            lua_pop(L, 1);
        }
        ok = terrain->build(h, cols, rows, o);
    } else {
        return luaL_argerror(L, 1, "expected an ImageData, an image path or a table of height rows");
    }
    if (!ok) return luaL_error(L, "createTerrain: need at least a 2x2 grid of heights");
    void* mem = lua_newuserdata(L, sizeof(LuaTerrain));
    new (mem) LuaTerrain{std::move(terrain)};
    luaL_getmetatable(L, kTerrainMeta);
    lua_setmetatable(L, -2);
    return 1;
}
int ter_gc(lua_State* L) { auto* u = static_cast<LuaTerrain*>(lua_touserdata(L, 1)); if (u) u->~LuaTerrain(); return 0; }
int ter_draw(lua_State* L) {
    auto t = check_terrain(L, 1);
    GLuint tex = lua_isnoneornil(L, 2) ? 0 : crayon_lua_check_texture(L, 2);
    glm::vec4 tint(num(L, 3, 1), num(L, 4, 1), num(L, 5, 1), num(L, 6, 1));
    t->draw(Engine::get().get_mesh_renderer(), tex, tint);
    return 0;
}
int ter_height(lua_State* L) { lua_pushnumber(L, check_terrain(L, 1)->height_at(num(L, 2, 0), num(L, 3, 0))); return 1; }
int ter_normal(lua_State* L) {
    glm::vec3 n = check_terrain(L, 1)->normal_at(num(L, 2, 0), num(L, 3, 0));
    lua_pushnumber(L, n.x); lua_pushnumber(L, n.y); lua_pushnumber(L, n.z);
    return 3;
}
int ter_contains(lua_State* L) { lua_pushboolean(L, check_terrain(L, 1)->contains(num(L, 2, 0), num(L, 3, 0))); return 1; }
int ter_raycast(lua_State* L) {
    glm::vec3 hit;
    bool ok = check_terrain(L, 1)->raycast(glm::vec3(num(L, 2, 0), num(L, 3, 0), num(L, 4, 0)),
                                           glm::vec3(num(L, 5, 0), num(L, 6, -1), num(L, 7, 0)), num(L, 8, 1000.0), hit);
    if (!ok) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, hit.x); lua_pushnumber(L, hit.y); lua_pushnumber(L, hit.z);
    return 3;
}
// terrain:createBody([friction = 0.6 [, restitution = 0]]) -> physics body
// A static heightfield matching the terrain (heights are resampled to Jolt's grid; differences from
// the visual mesh stay well below one cell).
int ter_create_body(lua_State* L) {
    auto t = check_terrain(L, 1);
    const Terrain::Options& o = t->options();
    PhysicsSystem& ps = Engine::get().get_physics();
    uint32_t id = ps.create_heightfield_body(o.origin, t->heights(), t->columns(), t->rows(), o.size, num(L, 2, 0.6), num(L, 3, 0.0));
    if (id == 0) return luaL_error(L, "terrain:createBody failed (is the physics3D module enabled?)");
    crayon_lua_push_physics_body(L, id);
    return 1;
}
int ter_info(lua_State* L) {
    auto t = check_terrain(L, 1);
    lua_pushinteger(L, t->columns()); lua_pushinteger(L, t->rows());
    lua_pushinteger(L, static_cast<lua_Integer>(t->chunk_count())); lua_pushinteger(L, static_cast<lua_Integer>(t->triangle_count()));
    return 4;
}

// ---------------- Model collision helpers ----------------
// model:hasCollision()                 true when the model has authored collision geometry
// model:getCollisionParts()            { { name=, kind="mesh"|"convex"|"box"|"sphere"|"capsule", node=index, hidden=bool }, ... }
// model:setCollisionVisible(bool)      draw the (normally hidden) collision geometry for debugging
int model_has_collision(lua_State* L) { lua_pushboolean(L, crayon_lua_check_model(L, 1)->has_collision_parts()); return 1; }
int model_set_collision_visible(lua_State* L) { crayon_lua_check_model(L, 1)->set_collision_visible(lua_toboolean(L, 2) != 0); return 0; }
int model_get_collision_parts(lua_State* L) {
    auto m = crayon_lua_check_model(L, 1);
    lua_newtable(L);
    int n = 0;
    for (const auto& part : m->get_parts()) {
        if (part.collision == CollisionKind::None) continue;
        lua_newtable(L);
        const bool node_ok = part.node_index >= 0 && part.node_index < static_cast<int>(m->get_nodes().size());
        lua_pushstring(L, (node_ok ? m->get_nodes()[part.node_index].name : part.name).c_str()); lua_setfield(L, -2, "name");
        lua_pushstring(L, collision_kind_name(part.collision)); lua_setfield(L, -2, "kind");
        lua_pushinteger(L, part.node_index); lua_setfield(L, -2, "node");
        lua_pushboolean(L, part.collision_only); lua_setfield(L, -2, "hidden");
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

void register_methods(lua_State* L) {
    luaL_getmetatable(L, "Graphics.Model");
    struct { const char* name; lua_CFunction fn; } mm[] = {
        {"hasCollision", model_has_collision}, {"getCollisionParts", model_get_collision_parts},
        {"setCollisionVisible", model_set_collision_visible}};
    for (auto& m : mm) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);

    luaL_newmetatable(L, kTerrainMeta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    struct { const char* name; lua_CFunction fn; } tm[] = {
        {"draw", ter_draw}, {"getHeightAt", ter_height}, {"getNormalAt", ter_normal}, {"contains", ter_contains},
        {"raycast", ter_raycast}, {"getInfo", ter_info}, {"createBody", ter_create_body}, {"__gc", ter_gc}};
    for (auto& m : tm) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);

    luaL_newmetatable(L, kSkyMeta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    struct { const char* name; lua_CFunction fn; } sm[] = {{"getFaceSize", sky_face_size}, {"draw", sky_draw}, {"__gc", sky_gc}};
    for (auto& m : sm) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);

    luaL_newmetatable(L, kBatchMeta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    struct { const char* name; lua_CFunction fn; } bm[] = {
        {"add", batch_add}, {"build", batch_build}, {"draw", batch_draw}, {"clear", batch_clear},
        {"getStats", batch_stats}, {"__gc", batch_gc},
    };
    for (auto& m : bm) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);

    luaL_newmetatable(L, kImageMeta);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    struct { const char* name; lua_CFunction fn; } methods[] = {
        {"getWidth", img_get_width}, {"getHeight", img_get_height}, {"getSize", img_get_size},
        {"getPixel", img_get_pixel}, {"setPixel", img_set_pixel}, {"fill", img_fill},
        {"paste", img_paste}, {"copy", img_copy}, {"save", img_save}, {"mapPixels", img_map_pixels},
        {"__tostring", img_tostring}, {"__gc", img_gc},
    };
    for (auto& m : methods) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);

    // extend the existing Graphics.Texture metatable (its __index points at itself)
    luaL_getmetatable(L, "Graphics.Texture");
    struct { const char* name; lua_CFunction fn; } tmethods[] = {
        {"setFilter", tex_set_filter}, {"getFilter", tex_get_filter}, {"setWrap", tex_set_wrap},
        {"getWrap", tex_get_wrap}, {"update", tex_update}, {"destroy", tex_destroy},
    };
    for (auto& m : tmethods) { lua_pushcfunction(L, m.fn); lua_setfield(L, -2, m.name); }
    lua_pop(L, 1);
}

} // namespace

// Applies `{ filter=..., wrap=... }` from a table at `opts_idx` to a texture id (used by loadTexture).
void crayon_lua_apply_texture_opts(lua_State* L, GLuint id, int opts_idx) { apply_texture_opts(L, id, opts_idx); }

void register_graphics_extra_bindings(lua_State* L) {
    register_methods(L);
    struct { const char* name; lua_CFunction fn; } fns[] = {
        {"newImageData", g_new_image_data}, {"loadImageData", g_load_image_data}, {"newTexture", g_new_texture},
        {"screenshot", g_screenshot}, {"captureFrame", g_capture_frame}, {"getCapture", g_get_capture},
        {"setTextureFilter", g_set_texture_filter}, {"setTextureWrap", g_set_texture_wrap},
        {"renderLightmap", g_render_lightmap}, {"beginMask", g_begin_mask}, {"useMask", g_use_mask},
        {"endMask", g_end_mask}, {"newStaticBatch", g_new_static_batch},
        {"loadSkybox", g_load_skybox}, {"drawSkybox", g_draw_skybox}, {"createTerrain", g_create_terrain},
    };
    for (auto& f : fns) { lua_pushcfunction(L, f.fn); lua_setfield(L, -2, f.name); }
}

} // namespace crayon
