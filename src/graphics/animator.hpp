#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "model3d.hpp"

namespace crayon {

struct NodeTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

class Animator {
public:
    explicit Animator(std::shared_ptr<Model3D> model);
    ~Animator() = default;

    // Playback
    void play(const std::string& clip_name, bool loop = true, float speed = 1.0f);
    void stop();
    void pause();
    void resume();
    bool is_playing() const { return m_playing; }

    const std::string& get_current_animation() const { return m_current_clip_name; }
    float get_time() const { return m_playback_time; }
    void set_time(float t);
    float get_duration() const;
    void set_speed(float speed) { m_speed = speed; }
    float get_speed() const { return m_speed; }

    // Smooth transition / Crossfade
    void cross_fade(const std::string& target_clip, float duration, bool loop = true);
    void cross_fade_from_current_pose(const std::string& target_clip, float duration, bool loop = true);

    // 1D Locomotion Blend Tree (walk / run phase synchronized)
    void blend(const std::string& clip_a, const std::string& clip_b, float factor);

    // Multi-Layer & Bone Masking (e.g. upper body aiming while running)
    void set_layer_clip(int layer, const std::string& clip_name, bool loop = true, float speed = 1.0f);
    void set_layer_weight(int layer, float weight);
    void set_layer_mask(int layer, const std::string& root_joint_name, bool include_children = true);

    // Open-world LOD update throttling (e.g. 30 Hz for distant crowd NPCs)
    void set_update_rate(int fps) { m_target_fps = fps; }
    void update(float dt);

    // Palette & Model
    const std::vector<glm::mat4>& get_bone_matrices() const { return m_skinning_palette; }
    size_t get_bone_count() const { return m_skinning_palette.size(); }
    std::shared_ptr<Model3D> get_model() const { return m_model; }

    // Physics Bridge
    bool apply_to_physics_pose(uint32_t pose_id, const glm::vec3* root_pos = nullptr, const glm::quat* root_rot = nullptr) const;
    bool capture_physics_pose(uint32_t pose_id);

private:
    void init_from_model();
    void sample_clip_to(const AnimationClip* clip, float time, std::vector<uint32_t>& hints, std::vector<NodeTransform>& out_transforms);
    void evaluate_matrices();

    std::shared_ptr<Model3D> m_model;

    // Base Locomotion State
    bool m_playing = false;
    float m_speed = 1.0f;
    float m_playback_time = 0.0f;
    bool m_loop = true;
    std::string m_current_clip_name;
    const AnimationClip* m_current_clip = nullptr;
    std::vector<uint32_t> m_current_hints;

    // Cross-fade State
    bool m_cross_fading = false;
    bool m_fade_from_frozen_pose = false;
    std::vector<NodeTransform> m_frozen_transforms;
    float m_fade_time = 0.0f;
    float m_fade_duration = 0.2f;
    float m_target_time = 0.0f;
    bool m_target_loop = true;
    std::string m_target_clip_name;
    const AnimationClip* m_target_clip = nullptr;
    std::vector<uint32_t> m_target_hints;

    // 1D Blend Tree State
    bool m_blend_tree_active = false;
    std::string m_blend_a_name;
    std::string m_blend_b_name;
    const AnimationClip* m_blend_clip_a = nullptr;
    const AnimationClip* m_blend_clip_b = nullptr;
    float m_blend_factor = 0.0f;
    float m_blend_phase = 0.0f;
    std::vector<uint32_t> m_blend_hints_a;
    std::vector<uint32_t> m_blend_hints_b;

    // Layer 1 (Upper-Body Mask)
    bool m_layer1_active = false;
    float m_layer1_weight = 0.0f;
    float m_layer1_time = 0.0f;
    float m_layer1_speed = 1.0f;
    bool m_layer1_loop = true;
    std::string m_layer1_clip_name;
    const AnimationClip* m_layer1_clip = nullptr;
    std::vector<uint32_t> m_layer1_hints;
    std::vector<float> m_layer1_mask; // Per node weight (0.0 to 1.0)

    // LOD Throttling
    int m_target_fps = 0; // 0 = unthrottled
    float m_accumulated_dt = 0.0f;

    // Pre-allocated Contiguous Runtime Buffers (Zero Allocation)
    std::vector<NodeTransform> m_bind_transforms;
    std::vector<NodeTransform> m_base_transforms;
    std::vector<NodeTransform> m_temp_transforms;
    std::vector<NodeTransform> m_final_transforms;
    std::vector<glm::mat4> m_node_world_matrices;
    std::vector<glm::mat4> m_skinning_palette;
};

} // namespace crayon
