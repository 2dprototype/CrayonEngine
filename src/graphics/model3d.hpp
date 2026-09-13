#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glad/glad.h>
#include "mesh3d.hpp"
#include "texture.hpp"

namespace crayon {

class MeshRenderer3D;

struct ModelPart {
    std::shared_ptr<Mesh3D> mesh;
    glm::mat4 transform{1.0f};
    GLuint texture_id = 0;
    glm::vec4 color{1.0f};
    std::string name;
    std::string material_name;
    int node_index = -1;

    glm::vec3 min_bounds{0.0f};
    glm::vec3 max_bounds{0.0f};

    // Stored for collision trimesh & spatial queries
    std::vector<Vertex3D> cpu_vertices;
    std::vector<GLuint> cpu_indices;
};

struct ModelNode {
    std::string name;
    int parent_index = -1;
    std::vector<int> children;

    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    glm::vec3 scale{1.0f};

    glm::mat4 local_matrix{1.0f};
    glm::mat4 world_matrix{1.0f};

    std::vector<int> part_indices;
};

class Model3D {
public:
    Model3D();
    ~Model3D();

    bool load_from_file(const std::string& filepath);
    bool load_from_gltf(const std::string& filepath);
    bool load_from_obj(const std::string& filepath);

    static std::shared_ptr<Model3D> create_from_mesh(std::shared_ptr<Mesh3D> mesh, const std::string& name = "mesh");

    void draw(MeshRenderer3D& renderer, const glm::mat4& world_transform, GLuint override_texture = 0) const;
    void draw_node(MeshRenderer3D& renderer, int node_index, const glm::mat4& world_transform, GLuint override_texture = 0) const;
    void draw_node(MeshRenderer3D& renderer, const std::string& node_name, const glm::mat4& world_transform, GLuint override_texture = 0) const;
    void draw_part(MeshRenderer3D& renderer, int part_index, const glm::mat4& world_transform, GLuint override_texture = 0) const;

    bool is_valid() const { return m_valid; }
    const std::string& get_filepath() const { return m_filepath; }

    // Hierarchy & Nodes
    size_t get_node_count() const { return m_nodes.size(); }
    const ModelNode* get_node(size_t index) const;
    int find_node_index(const std::string& name) const;
    const ModelNode* find_node(const std::string& name) const;
    const std::vector<ModelNode>& get_nodes() const { return m_nodes; }

    // Parts / Submeshes
    size_t get_part_count() const { return m_parts.size(); }
    const ModelPart* get_part(size_t index) const;
    ModelPart* get_part(size_t index);
    const std::vector<ModelPart>& get_parts() const { return m_parts; }

    void set_part_texture(size_t index, GLuint texture_id);
    void set_part_color(size_t index, const glm::vec4& color);

    // Bounding Box
    void get_bounds(glm::vec3& out_min, glm::vec3& out_max) const;
    glm::vec3 get_center() const;
    glm::vec3 get_size() const;

    // Collision geometry extraction (for Jolt trimesh & queries)
    void get_collision_data(std::vector<glm::vec3>& out_vertices, std::vector<uint32_t>& out_indices, bool apply_transforms = true) const;

private:
    void compute_bounds();
    void update_node_world_matrices();

    std::vector<ModelPart> m_parts;
    std::vector<ModelNode> m_nodes;
    std::vector<std::shared_ptr<Texture>> m_owned_textures;

    glm::vec3 m_min_bounds{0.0f};
    glm::vec3 m_max_bounds{0.0f};

    bool m_valid = false;
    std::string m_filepath;
};

} // namespace crayon
