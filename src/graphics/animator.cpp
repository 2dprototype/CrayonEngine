#include "animator.hpp"
#include "../core/engine.hpp"
#include "../physics/physics_system.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace crayon {

static void sample_vec3(const std::vector<float>& times, const std::vector<glm::vec3>& values,
                        AnimationInterpolation interp, float t, uint32_t& hint, glm::vec3& out) {
    size_t count = times.size();
    if (count == 0 || values.empty()) return;
    if (count == 1 || values.size() == 1) { out = values[0]; return; }

    uint32_t i = hint;
    if (i + 1 < count && t >= times[i] && t <= times[i + 1]) {
        // Cache hit
    } else if (i + 2 < count && t >= times[i + 1] && t <= times[i + 2]) {
        i = hint = i + 1;
    } else {
        auto it = std::upper_bound(times.begin(), times.end(), t);
        i = hint = (it == times.begin()) ? 0 : static_cast<uint32_t>(std::distance(times.begin(), it) - 1);
    }

    if (i + 1 >= count || i + 1 >= values.size()) {
        out = values.back();
        return;
    }

    float t0 = times[i];
    float t1 = times[i + 1];
    float factor = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
    if (interp == AnimationInterpolation::Step) {
        out = values[i];
    } else {
        out = glm::mix(values[i], values[i + 1], factor);
    }
}

static void sample_quat(const std::vector<float>& times, const std::vector<glm::quat>& values,
                        AnimationInterpolation interp, float t, uint32_t& hint, glm::quat& out) {
    size_t count = times.size();
    if (count == 0 || values.empty()) return;
    if (count == 1 || values.size() == 1) { out = values[0]; return; }

    uint32_t i = hint;
    if (i + 1 < count && t >= times[i] && t <= times[i + 1]) {
        // Cache hit
    } else if (i + 2 < count && t >= times[i + 1] && t <= times[i + 2]) {
        i = hint = i + 1;
    } else {
        auto it = std::upper_bound(times.begin(), times.end(), t);
        i = hint = (it == times.begin()) ? 0 : static_cast<uint32_t>(std::distance(times.begin(), it) - 1);
    }

    if (i + 1 >= count || i + 1 >= values.size()) {
        out = values.back();
        return;
    }

    float t0 = times[i];
    float t1 = times[i + 1];
    float factor = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
    if (interp == AnimationInterpolation::Step) {
        out = values[i];
    } else {
        out = glm::slerp(values[i], values[i + 1], factor);
    }
}

Animator::Animator(std::shared_ptr<Model3D> model)
    : m_model(std::move(model)) {
    init_from_model();
}

void Animator::init_from_model() {
    if (!m_model) return;

    size_t node_count = m_model->get_node_count();
    m_bind_transforms.resize(node_count);
    m_base_transforms.resize(node_count);
    m_temp_transforms.resize(node_count);
    m_final_transforms.resize(node_count);
    m_node_world_matrices.resize(node_count, glm::mat4(1.0f));
    m_layer1_mask.resize(node_count, 0.0f);

    const auto& nodes = m_model->get_nodes();
    for (size_t i = 0; i < node_count; ++i) {
        m_bind_transforms[i].translation = nodes[i].translation;
        m_bind_transforms[i].rotation = nodes[i].rotation;
        m_bind_transforms[i].scale = nodes[i].scale;

        m_base_transforms[i] = m_bind_transforms[i];
        m_final_transforms[i] = m_bind_transforms[i];
    }

    // Allocate skinning palette
    const auto* skin = m_model->get_skin(0);
    if (skin) {
        m_skinning_palette.resize(skin->joints.size(), glm::mat4(1.0f));
    }

    // Default play first clip if available
    if (m_model->get_animation_count() > 0) {
        m_current_clip = m_model->get_animation(0);
        if (m_current_clip) {
            m_current_clip_name = m_current_clip->name;
            m_current_hints.resize(m_current_clip->channels.size(), 0);
        }
    }

    evaluate_matrices();
}

void Animator::play(const std::string& clip_name, bool loop, float speed) {
    if (!m_model) return;

    const auto* clip = m_model->find_animation(clip_name);
    if (!clip) return;

    m_current_clip = clip;
    m_current_clip_name = clip_name;
    m_playback_time = 0.0f;
    m_loop = loop;
    m_speed = speed;
    m_playing = true;
    m_cross_fading = false;
    m_blend_tree_active = false;

    m_current_hints.assign(clip->channels.size(), 0);
}

void Animator::stop() {
    m_playing = false;
    m_playback_time = 0.0f;
    m_cross_fading = false;
    m_blend_tree_active = false;
}

void Animator::pause() {
    m_playing = false;
}

void Animator::resume() {
    m_playing = true;
}

void Animator::set_time(float t) {
    m_playback_time = t;
    if (m_current_clip && m_current_clip->duration > 0.0f) {
        if (m_loop) {
            m_playback_time = std::fmod(m_playback_time, m_current_clip->duration);
            if (m_playback_time < 0.0f) m_playback_time += m_current_clip->duration;
        } else {
            m_playback_time = std::clamp(m_playback_time, 0.0f, m_current_clip->duration);
        }
    }
}

float Animator::get_duration() const {
    if (m_current_clip) return m_current_clip->duration;
    return 0.0f;
}

void Animator::cross_fade(const std::string& target_clip, float duration, bool loop) {
    if (!m_model) return;
    if (target_clip == m_current_clip_name && !m_cross_fading) return;

    const auto* clip = m_model->find_animation(target_clip);
    if (!clip) return;

    m_target_clip = clip;
    m_target_clip_name = target_clip;
    m_target_time = 0.0f;
    m_target_loop = loop;
    m_fade_time = 0.0f;
    m_fade_duration = (duration > 0.001f) ? duration : 0.001f;
    m_cross_fading = true;
    m_blend_tree_active = false;
    m_playing = true;

    m_target_hints.assign(clip->channels.size(), 0);
}

void Animator::blend(const std::string& clip_a, const std::string& clip_b, float factor) {
    if (!m_model) return;

    const auto* ca = m_model->find_animation(clip_a);
    const auto* cb = m_model->find_animation(clip_b);
    if (!ca || !cb) return;

    m_blend_clip_a = ca;
    m_blend_clip_b = cb;
    m_blend_a_name = clip_a;
    m_blend_b_name = clip_b;
    m_blend_factor = std::clamp(factor, 0.0f, 1.0f);
    m_blend_tree_active = true;
    m_cross_fading = false;
    m_playing = true;

    if (m_blend_hints_a.size() != ca->channels.size()) m_blend_hints_a.assign(ca->channels.size(), 0);
    if (m_blend_hints_b.size() != cb->channels.size()) m_blend_hints_b.assign(cb->channels.size(), 0);
}

void Animator::set_layer_clip(int layer, const std::string& clip_name, bool loop, float speed) {
    if (layer != 1 || !m_model) return;

    if (clip_name.empty()) {
        m_layer1_active = false;
        m_layer1_clip = nullptr;
        m_layer1_clip_name.clear();
        return;
    }

    const auto* clip = m_model->find_animation(clip_name);
    if (!clip) return;

    m_layer1_clip = clip;
    m_layer1_clip_name = clip_name;
    m_layer1_loop = loop;
    m_layer1_speed = speed;
    m_layer1_time = 0.0f;
    m_layer1_active = true;
    m_layer1_hints.assign(clip->channels.size(), 0);
}

void Animator::set_layer_weight(int layer, float weight) {
    if (layer == 1) {
        m_layer1_weight = std::clamp(weight, 0.0f, 1.0f);
    }
}

void Animator::set_layer_mask(int layer, const std::string& root_joint_name, bool include_children) {
    if (layer != 1 || !m_model) return;

    std::fill(m_layer1_mask.begin(), m_layer1_mask.end(), 0.0f);

    int start_node = m_model->find_node_index(root_joint_name);
    if (start_node < 0) return;

    m_layer1_mask[start_node] = 1.0f;

    if (include_children) {
        const auto& nodes = m_model->get_nodes();
        // Propagate downward using parent links
        for (size_t i = 0; i < nodes.size(); ++i) {
            int p = nodes[i].parent_index;
            while (p >= 0) {
                if (p == start_node || m_layer1_mask[p] > 0.5f) {
                    m_layer1_mask[i] = 1.0f;
                    break;
                }
                p = nodes[p].parent_index;
            }
        }
    }
}

void Animator::sample_clip_to(const AnimationClip* clip, float time, std::vector<uint32_t>& hints, std::vector<NodeTransform>& out_transforms) {
    if (!clip) return;

    for (size_t c = 0; c < clip->channels.size(); ++c) {
        const auto& ch = clip->channels[c];
        if (ch.target_node < 0 || ch.target_node >= (int)out_transforms.size()) continue;

        uint32_t& hint = (c < hints.size()) ? hints[c] : hints[0];
        NodeTransform& tr = out_transforms[ch.target_node];

        if (ch.path == AnimationPath::Translation) {
            sample_vec3(ch.timestamps, ch.vec3_values, ch.interpolation, time, hint, tr.translation);
        } else if (ch.path == AnimationPath::Rotation) {
            sample_quat(ch.timestamps, ch.quat_values, ch.interpolation, time, hint, tr.rotation);
        } else if (ch.path == AnimationPath::Scale) {
            sample_vec3(ch.timestamps, ch.vec3_values, ch.interpolation, time, hint, tr.scale);
        }
    }
}

void Animator::update(float dt) {
    if (!m_model) return;

    // LOD throttling
    if (m_target_fps > 0) {
        m_accumulated_dt += dt;
        float interval = 1.0f / static_cast<float>(m_target_fps);
        if (m_accumulated_dt < interval) {
            return;
        }
        dt = m_accumulated_dt;
        m_accumulated_dt = 0.0f;
    }

    if (!m_playing) return;

    // Reset base transforms to bind pose
    m_base_transforms = m_bind_transforms;

    if (m_blend_tree_active && m_blend_clip_a && m_blend_clip_b) {
        // 1D Blend Tree: Phase-synchronized playback
        float dur_a = m_blend_clip_a->duration > 0.001f ? m_blend_clip_a->duration : 1.0f;
        float dur_b = m_blend_clip_b->duration > 0.001f ? m_blend_clip_b->duration : 1.0f;
        float blended_dur = glm::mix(dur_a, dur_b, m_blend_factor);

        m_blend_phase += (dt * m_speed) / blended_dur;
        m_blend_phase = std::fmod(m_blend_phase, 1.0f);
        if (m_blend_phase < 0.0f) m_blend_phase += 1.0f;

        float t_a = m_blend_phase * dur_a;
        float t_b = m_blend_phase * dur_b;

        m_temp_transforms = m_bind_transforms;
        sample_clip_to(m_blend_clip_a, t_a, m_blend_hints_a, m_base_transforms);
        sample_clip_to(m_blend_clip_b, t_b, m_blend_hints_b, m_temp_transforms);

        // Blend A and B into m_base_transforms
        for (size_t i = 0; i < m_base_transforms.size(); ++i) {
            m_base_transforms[i].translation = glm::mix(m_base_transforms[i].translation, m_temp_transforms[i].translation, m_blend_factor);
            m_base_transforms[i].rotation = glm::slerp(m_base_transforms[i].rotation, m_temp_transforms[i].rotation, m_blend_factor);
            m_base_transforms[i].scale = glm::mix(m_base_transforms[i].scale, m_temp_transforms[i].scale, m_blend_factor);
        }
    } else if (m_current_clip) {
        // Advance current clip
        m_playback_time += dt * m_speed;
        if (m_current_clip->duration > 0.0f) {
            if (m_loop) {
                m_playback_time = std::fmod(m_playback_time, m_current_clip->duration);
                if (m_playback_time < 0.0f) m_playback_time += m_current_clip->duration;
            } else if (m_playback_time > m_current_clip->duration) {
                m_playback_time = m_current_clip->duration;
            }
        }

        sample_clip_to(m_current_clip, m_playback_time, m_current_hints, m_base_transforms);

        // Cross-fade
        if (m_cross_fading && m_target_clip) {
            m_fade_time += dt;
            float alpha = std::clamp(m_fade_time / m_fade_duration, 0.0f, 1.0f);

            m_target_time += dt * m_speed;
            if (m_target_clip->duration > 0.0f) {
                if (m_target_loop) {
                    m_target_time = std::fmod(m_target_time, m_target_clip->duration);
                    if (m_target_time < 0.0f) m_target_time += m_target_clip->duration;
                } else if (m_target_time > m_target_clip->duration) {
                    m_target_time = m_target_clip->duration;
                }
            }

            m_temp_transforms = m_bind_transforms;
            sample_clip_to(m_target_clip, m_target_time, m_target_hints, m_temp_transforms);

            for (size_t i = 0; i < m_base_transforms.size(); ++i) {
                m_base_transforms[i].translation = glm::mix(m_base_transforms[i].translation, m_temp_transforms[i].translation, alpha);
                m_base_transforms[i].rotation = glm::slerp(m_base_transforms[i].rotation, m_temp_transforms[i].rotation, alpha);
                m_base_transforms[i].scale = glm::mix(m_base_transforms[i].scale, m_temp_transforms[i].scale, alpha);
            }

            if (alpha >= 1.0f) {
                m_cross_fading = false;
                m_current_clip = m_target_clip;
                m_current_clip_name = m_target_clip_name;
                m_playback_time = m_target_time;
                m_loop = m_target_loop;
                m_current_hints = std::move(m_target_hints);
                m_target_clip = nullptr;
            }
        }
    }

    m_final_transforms = m_base_transforms;

    // Apply Layer 1 (Masked action/aim)
    if (m_layer1_active && m_layer1_clip && m_layer1_weight > 0.001f) {
        m_layer1_time += dt * m_layer1_speed;
        if (m_layer1_clip->duration > 0.0f) {
            if (m_layer1_loop) {
                m_layer1_time = std::fmod(m_layer1_time, m_layer1_clip->duration);
                if (m_layer1_time < 0.0f) m_layer1_time += m_layer1_clip->duration;
            } else if (m_layer1_time > m_layer1_clip->duration) {
                m_layer1_time = m_layer1_clip->duration;
            }
        }

        m_temp_transforms = m_bind_transforms;
        sample_clip_to(m_layer1_clip, m_layer1_time, m_layer1_hints, m_temp_transforms);

        for (size_t i = 0; i < m_final_transforms.size(); ++i) {
            float w = m_layer1_mask[i] * m_layer1_weight;
            if (w > 0.001f) {
                m_final_transforms[i].translation = glm::mix(m_final_transforms[i].translation, m_temp_transforms[i].translation, w);
                m_final_transforms[i].rotation = glm::slerp(m_final_transforms[i].rotation, m_temp_transforms[i].rotation, w);
                m_final_transforms[i].scale = glm::mix(m_final_transforms[i].scale, m_temp_transforms[i].scale, w);
            }
        }
    }

    evaluate_matrices();
}

void Animator::evaluate_matrices() {
    if (!m_model) return;

    const auto& nodes = m_model->get_nodes();
    size_t node_count = nodes.size();

    // 1. Flat linear hierarchy evaluation (Topological parent < child)
    for (size_t i = 0; i < node_count; ++i) {
        const auto& tr = m_final_transforms[i];
        glm::mat4 local_m = glm::translate(glm::mat4(1.0f), tr.translation) *
                            glm::mat4_cast(tr.rotation) *
                            glm::scale(glm::mat4(1.0f), tr.scale);

        int p = nodes[i].parent_index;
        if (p >= 0 && p < (int)node_count) {
            m_node_world_matrices[i] = m_node_world_matrices[p] * local_m;
        } else {
            m_node_world_matrices[i] = local_m;
        }
    }

    // 2. Compute final skinning palette
    const auto* skin = m_model->get_skin(0);
    if (skin) {
        size_t joint_count = skin->joints.size();
        if (m_skinning_palette.size() != joint_count) {
            m_skinning_palette.resize(joint_count);
        }

        for (size_t j = 0; j < joint_count; ++j) {
            int node_idx = skin->joints[j].node_index;
            if (node_idx >= 0 && node_idx < (int)node_count) {
                m_skinning_palette[j] = m_node_world_matrices[node_idx] * skin->joints[j].inverse_bind_matrix;
            } else {
                m_skinning_palette[j] = glm::mat4(1.0f);
            }
        }
    }
}

bool Animator::apply_to_physics_pose(uint32_t pose_id) const {
    if (!m_model || pose_id == 0) return false;
    const auto* skin = m_model->get_skin(0);
    if (!skin) return false;

    auto& ps = Engine::get().get_physics();
    int count = ps.skeleton_pose_get_joint_count(pose_id);
    int apply_count = std::min((int)skin->joints.size(), count);

    for (int j = 0; j < apply_count; ++j) {
        int node_idx = skin->joints[j].node_index;
        if (node_idx >= 0 && node_idx < (int)m_final_transforms.size()) {
            const auto& tr = m_final_transforms[node_idx];
            ps.skeleton_pose_set_joint(pose_id, j, tr.translation, tr.rotation);
        }
    }

    ps.skeleton_pose_calculate_matrices(pose_id);
    return true;
}

bool Animator::capture_physics_pose(uint32_t pose_id) {
    if (!m_model || pose_id == 0) return false;
    const auto* skin = m_model->get_skin(0);
    if (!skin) return false;

    auto& ps = Engine::get().get_physics();
    int count = ps.skeleton_pose_get_joint_count(pose_id);
    int apply_count = std::min((int)skin->joints.size(), count);

    for (int j = 0; j < apply_count; ++j) {
        int node_idx = skin->joints[j].node_index;
        if (node_idx >= 0 && node_idx < (int)m_base_transforms.size()) {
            glm::mat4 m = ps.skeleton_pose_get_joint_matrix(pose_id, j);
            m_base_transforms[node_idx].translation = glm::vec3(m[3]);
            m_base_transforms[node_idx].rotation = glm::quat_cast(m);
        }
    }

    m_final_transforms = m_base_transforms;
    evaluate_matrices();
    return true;
}

} // namespace crayon
