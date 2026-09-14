#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace crayon::editor {

enum class EntityType {
    Camera3D,
    Model3D,
    MeshPrimitive,
    Sprite2D,
    PhysicsBody,
    Light
};

struct SceneObject {
    int id = 0;
    std::string name;
    EntityType type = EntityType::Model3D;
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f}; // Euler degrees
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string asset_path = "";
    bool visible = true;
    bool wireframe = false;

    // Physics props
    bool has_physics = false;
    int physics_type = 0; // 0=Static, 1=Dynamic, 2=Kinematic
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.2f;
};

class SceneContext {
public:
    static SceneContext& get() {
        static SceneContext instance;
        return instance;
    }

    std::vector<SceneObject>& get_objects() { return m_objects; }
    const std::vector<SceneObject>& get_objects() const { return m_objects; }

    int get_selected_id() const { return m_selected_id; }
    void set_selected_id(int id) { m_selected_id = id; }

    SceneObject* get_selected_object() {
        for (auto& obj : m_objects) {
            if (obj.id == m_selected_id) return &obj;
        }
        return nullptr;
    }

    void add_object(const SceneObject& obj) {
        SceneObject new_obj = obj;
        new_obj.id = m_next_id++;
        m_objects.push_back(new_obj);
        m_selected_id = new_obj.id;
    }

    void remove_object(int id) {
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
            if (it->id == id) {
                m_objects.erase(it);
                if (m_selected_id == id) {
                    m_selected_id = m_objects.empty() ? -1 : m_objects.front().id;
                }
                break;
            }
        }
    }

    void clear() {
        m_objects.clear();
        m_selected_id = -1;
    }

    void populate_default_retro_scene() {
        clear();
        SceneObject cam;
        cam.name = "Main Camera 3D";
        cam.type = EntityType::Camera3D;
        cam.position = glm::vec3(0.0f, 3.0f, 6.0f);
        cam.rotation = glm::vec3(-20.0f, 0.0f, 0.0f);
        add_object(cam);

        SceneObject ground;
        ground.name = "Ground Plane";
        ground.type = EntityType::MeshPrimitive;
        ground.position = glm::vec3(0.0f, -0.5f, 0.0f);
        ground.scale = glm::vec3(10.0f, 0.2f, 10.0f);
        ground.color = glm::vec4(0.25f, 0.35f, 0.25f, 1.0f);
        ground.has_physics = true;
        ground.physics_type = 0; // Static
        add_object(ground);

        SceneObject hero;
        hero.name = "Player Character";
        hero.type = EntityType::Model3D;
        hero.position = glm::vec3(0.0f, 0.5f, 0.0f);
        hero.scale = glm::vec3(1.0f, 1.0f, 1.0f);
        hero.color = glm::vec4(0.85f, 0.35f, 0.25f, 1.0f);
        hero.asset_path = "models/cube.obj";
        hero.has_physics = true;
        hero.physics_type = 1; // Dynamic
        add_object(hero);
    }

private:
    SceneContext() {
        populate_default_retro_scene();
    }

    std::vector<SceneObject> m_objects;
    int m_selected_id = 0;
    int m_next_id = 1;
};

} // namespace crayon::editor
