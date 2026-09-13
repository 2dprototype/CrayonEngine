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
    bool is_skinned = false;
    int skin_index = -1;

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

struct ModelJoint {
    std::string name;
    int node_index = -1;
    int parent_joint_index = -1;
    glm::mat4 inverse_bind_matrix{1.0f};
};

struct ModelSkin {
    std::string name;
    int skeleton_root_node = -1;
    std::vector<ModelJoint> joints;
    std::unordered_map<std::string, int> joint_name_to_index;
};

enum class AnimationPath : uint8_t {
    Translation,
    Rotation,
    Scale
};

enum class AnimationInterpolation : uint8_t {
    Step,
    Linear,
    CubicSpline
};

struct AnimationChannel {
    int target_node = -1;
    AnimationPath path = AnimationPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<float> timestamps;
    std::vector<glm::vec3> vec3_values; // For Translation / Scale
    std::vector<glm::quat> quat_values; // For Rotation
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
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
    void draw_skinned(MeshRenderer3D& renderer, const glm::mat4& world_transform, const glm::mat4* bone_matrices, size_t bone_count, GLuint override_texture = 0) const;
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

    // Skinning & Animation
    bool is_skinned() const { return !m_skins.empty(); }
    size_t get_skin_count() const { return m_skins.size(); }
    const ModelSkin* get_skin(size_t index = 0) const;
    const std::vector<ModelSkin>& get_skins() const { return m_skins; }

    size_t get_animation_count() const { return m_animations.size(); }
    const AnimationClip* get_animation(size_t index) const;
    const AnimationClip* find_animation(const std::string& name) const;
    int find_animation_index(const std::string& name) const;
    const std::vector<AnimationClip>& get_animations() const { return m_animations; }

    std::vector<std::pair<std::string, int>> get_physics_skeleton_joints(size_t skin_index = 0) const;

private:
    void compute_bounds();
    void update_node_world_matrices();

    std::vector<ModelPart> m_parts;
    std::vector<ModelNode> m_nodes;
    std::vector<std::shared_ptr<Texture>> m_owned_textures;
    std::vector<ModelSkin> m_skins;
    std::vector<AnimationClip> m_animations;

    glm::vec3 m_min_bounds{0.0f};
    glm::vec3 m_max_bounds{0.0f};

    bool m_valid = false;
    std::string m_filepath;
};

} // namespace crayon
