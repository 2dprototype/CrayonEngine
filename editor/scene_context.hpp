#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <lua.hpp>
#include "../../src/scripting/lua_runtime.hpp"
#include "../../src/graphics/mesh3d.hpp"
#include "../../src/graphics/model3d.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/core/log.hpp"

namespace crayon::editor {

struct SceneEntity {
    int id = 0;
    std::string name = "Entity";
    std::string type = "cube"; // "cube", "plane", "sphere", "cylinder", "model"
    std::string asset_path = ""; // for "model" type (.obj, .gltf, .glb)

    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f}; // Euler angles in degrees
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec4 color{0.8f, 0.8f, 0.85f, 1.0f};

    bool has_collider = true;
    std::string collider_type = "box"; // "box", "sphere", "plane"
    std::string collider_motion = "static"; // "static", "dynamic"
    bool visible = true;

    // Cache model pointer for instant rendering without lookups
    std::shared_ptr<crayon::Model3D> cached_model;
    std::string last_loaded_path;
};

class SceneContext {
public:
    static SceneContext& get() {
        static SceneContext instance;
        return instance;
    }

    std::vector<SceneEntity>& get_entities() { return m_entities; }
    const std::vector<SceneEntity>& get_entities() const { return m_entities; }

    int get_selected_id() const { return m_selected_id; }
    void set_selected_id(int id) { m_selected_id = id; }

    SceneEntity* get_selected_entity() {
        for (auto& entity : m_entities) {
            if (entity.id == m_selected_id) return &entity;
        }
        return nullptr;
    }

    void add_entity(const SceneEntity& entity) {
        SceneEntity new_entity = entity;
        new_entity.id = m_next_id++;
        m_entities.push_back(new_entity);
        m_selected_id = new_entity.id;
    }

    void remove_entity(int id) {
        for (auto it = m_entities.begin(); it != m_entities.end(); ++it) {
            if (it->id == id) {
                m_entities.erase(it);
                if (m_selected_id == id) {
                    m_selected_id = m_entities.empty() ? -1 : m_entities.front().id;
                }
                break;
            }
        }
    }

    void clear() {
        m_entities.clear();
        m_selected_id = -1;
    }

    void reset_to_default_scene() {
        clear();

        // 1. Ground Plane
        SceneEntity ground;
        ground.name = "Ground";
        ground.type = "plane";
        ground.position = glm::vec3(0.0f, 0.0f, 0.0f);
        ground.scale = glm::vec3(20.0f, 1.0f, 20.0f);
        ground.color = glm::vec4(0.3f, 0.45f, 0.3f, 1.0f);
        ground.has_collider = true;
        ground.collider_type = "box";
        ground.collider_motion = "static";
        add_entity(ground);

        // 2. Pillars
        SceneEntity p1;
        p1.name = "Pillar_Left";
        p1.type = "cube";
        p1.position = glm::vec3(-4.0f, 1.5f, -3.0f);
        p1.scale = glm::vec3(1.0f, 3.0f, 1.0f);
        p1.color = glm::vec4(0.6f, 0.6f, 0.65f, 1.0f);
        add_entity(p1);

        SceneEntity p2;
        p2.name = "Pillar_Right";
        p2.type = "cube";
        p2.position = glm::vec3(4.0f, 1.5f, -3.0f);
        p2.scale = glm::vec3(1.0f, 3.0f, 1.0f);
        p2.color = glm::vec4(0.6f, 0.6f, 0.65f, 1.0f);
        add_entity(p2);

        // 3. Center Altar
        SceneEntity altar;
        altar.name = "Center_Altar";
        altar.type = "cylinder";
        altar.position = glm::vec3(0.0f, 0.5f, 0.0f);
        altar.scale = glm::vec3(2.0f, 1.0f, 2.0f);
        altar.color = glm::vec4(0.85f, 0.55f, 0.25f, 1.0f);
        add_entity(altar);

        m_scene_path = "game/scenes/default.scene.lua";
    }

    bool save_to_lua(const std::string& filepath) {
        std::filesystem::create_directories(std::filesystem::path(filepath).parent_path());

        std::ofstream out(filepath);
        if (!out.is_open()) {
            CRAYON_LOG_ERROR("Failed to open file for saving scene: {}", filepath);
            return false;
        }

        out << "-- Crayon 3D Scene File\n";
        out << "return {\n";
        out << "    name = \"" << m_scene_name << "\",\n";
        out << "    objects = {\n";

        for (size_t i = 0; i < m_entities.size(); ++i) {
            const auto& e = m_entities[i];
            out << "        {\n";
            out << "            name = \"" << e.name << "\",\n";
            out << "            type = \"" << e.type << "\",\n";
            if (e.type == "model" && !e.asset_path.empty()) {
                out << "            asset = \"" << e.asset_path << "\",\n";
            }
            out << std::format("            pos = {{ {:.2f}, {:.2f}, {:.2f} }},\n", e.position.x, e.position.y, e.position.z);
            out << std::format("            rot = {{ {:.2f}, {:.2f}, {:.2f} }},\n", e.rotation.x, e.rotation.y, e.rotation.z);
            out << std::format("            scale = {{ {:.2f}, {:.2f}, {:.2f} }},\n", e.scale.x, e.scale.y, e.scale.z);
            out << std::format("            color = {{ {:.2f}, {:.2f}, {:.2f}, {:.2f} }},\n", e.color.r, e.color.g, e.color.b, e.color.a);
            out << "            has_collider = " << (e.has_collider ? "true" : "false") << ",\n";
            out << "            collider_type = \"" << e.collider_type << "\",\n";
            out << "            collider_motion = \"" << e.collider_motion << "\",\n";
            out << "            visible = " << (e.visible ? "true" : "false") << "\n";
            out << "        }" << (i + 1 < m_entities.size() ? ",\n" : "\n");
        }

        out << "    }\n";
        out << "}\n";

        m_scene_path = filepath;
        CRAYON_LOG_INFO("Saved 3D scene to '{}' ({} objects)", filepath, m_entities.size());
        return true;
    }

    bool load_from_lua(const std::string& filepath, crayon::LuaRuntime& lua) {
        if (!std::filesystem::exists(filepath)) {
            CRAYON_LOG_ERROR("Scene file does not exist: {}", filepath);
            return false;
        }

        lua_State* L = lua.get_state();
        if (luaL_loadfile(L, filepath.c_str()) != 0 || lua_pcall(L, 0, 1, 0) != 0) {
            const char* err = lua_tostring(L, -1);
            CRAYON_LOG_ERROR("Failed to parse scene file '{}': {}", filepath, err ? err : "unknown");
            lua_pop(L, 1);
            return false;
        }

        if (!lua_istable(L, -1)) {
            CRAYON_LOG_ERROR("Scene file '{}' did not return a table", filepath);
            lua_pop(L, 1);
            return false;
        }

        clear();

        lua_getfield(L, -1, "name");
        if (lua_isstring(L, -1)) {
            m_scene_name = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "objects");
        if (lua_istable(L, -1)) {
            int len = (int)lua_objlen(L, -1);
            for (int i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                if (lua_istable(L, -1)) {
                    SceneEntity e;
                    lua_getfield(L, -1, "name");
                    if (lua_isstring(L, -1)) e.name = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "type");
                    if (lua_isstring(L, -1)) e.type = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "asset");
                    if (lua_isstring(L, -1)) e.asset_path = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "pos");
                    if (lua_istable(L, -1)) {
                        lua_rawgeti(L, -1, 1); e.position.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 2); e.position.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 3); e.position.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "rot");
                    if (lua_istable(L, -1)) {
                        lua_rawgeti(L, -1, 1); e.rotation.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 2); e.rotation.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 3); e.rotation.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "scale");
                    if (lua_istable(L, -1)) {
                        lua_rawgeti(L, -1, 1); e.scale.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 2); e.scale.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 3); e.scale.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "color");
                    if (lua_istable(L, -1)) {
                        lua_rawgeti(L, -1, 1); e.color.r = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 2); e.color.g = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 3); e.color.b = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                        lua_rawgeti(L, -1, 4); e.color.a = (float)lua_tonumber(L, -1); lua_pop(L, 1);
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "has_collider");
                    if (lua_isboolean(L, -1)) e.has_collider = lua_toboolean(L, -1) != 0;
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "collider_type");
                    if (lua_isstring(L, -1)) e.collider_type = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "collider_motion");
                    if (lua_isstring(L, -1)) e.collider_motion = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "visible");
                    if (lua_isboolean(L, -1)) e.visible = lua_toboolean(L, -1) != 0;
                    lua_pop(L, 1);

                    add_entity(e);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1); // pop objects table
        lua_pop(L, 1); // pop scene root table

        m_scene_path = filepath;
        CRAYON_LOG_INFO("Loaded 3D scene from '{}' ({} objects)", filepath, m_entities.size());
        return true;
    }

    void render_scene_3d(crayon::MeshRenderer3D& renderer, crayon::Engine& engine) {
        for (auto& e : m_entities) {
            if (!e.visible) continue;

            glm::vec3 rot_rad = glm::radians(e.rotation);
            engine.set_active_color(e.color.r, e.color.g, e.color.b, e.color.a);

            if (e.type == "cube") {
                renderer.draw_cube(e.position, e.scale, 0, rot_rad);
            } else if (e.type == "plane") {
                renderer.draw_plane(e.position, e.scale.x, e.scale.z, 0, rot_rad);
            } else if (e.type == "sphere") {
                renderer.draw_sphere(e.position, e.scale.x * 0.5f, 0, rot_rad);
            } else if (e.type == "cylinder") {
                renderer.draw_cylinder(e.position, e.scale.x * 0.5f, e.scale.y, 0, rot_rad);
            } else if (e.type == "model" && !e.asset_path.empty()) {
                if (!e.cached_model || e.last_loaded_path != e.asset_path) {
                    e.cached_model = engine.load_model3d(e.asset_path);
                    e.last_loaded_path = e.asset_path;
                }

                if (e.cached_model) {
                    glm::mat4 t{1.0f};
                    t = glm::translate(t, e.position);
                    if (e.rotation.x != 0.0f) t = glm::rotate(t, rot_rad.x, glm::vec3(1, 0, 0));
                    if (e.rotation.y != 0.0f) t = glm::rotate(t, rot_rad.y, glm::vec3(0, 1, 0));
                    if (e.rotation.z != 0.0f) t = glm::rotate(t, rot_rad.z, glm::vec3(0, 0, 1));
                    t = glm::scale(t, e.scale);
                    renderer.draw_model(*e.cached_model, t);
                } else {
                    renderer.draw_cube(e.position, e.scale, 0, rot_rad);
                }
            }

            // Draw highlight wireframe for selected entity
            if (e.id == m_selected_id) {
                renderer.draw_cube_wires(e.position, e.scale * 1.05f, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f), rot_rad);
            }
        }
    }

    const std::string& get_scene_path() const { return m_scene_path; }
    void set_scene_path(const std::string& p) { m_scene_path = p; }
    const std::string& get_scene_name() const { return m_scene_name; }
    void set_scene_name(const std::string& n) { m_scene_name = n; }

private:
    SceneContext() {
        reset_to_default_scene();
    }

    std::vector<SceneEntity> m_entities;
    std::string m_scene_path = "game/scenes/default.scene.lua";
    std::string m_scene_name = "Default Map";
    int m_selected_id = 0;
    int m_next_id = 1;
};

} // namespace crayon::editor
