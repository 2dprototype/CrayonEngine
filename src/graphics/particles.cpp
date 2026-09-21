#include "particles.hpp"
#include <algorithm>
#include <cmath>

namespace crayon {

ParticleEmitter::ParticleEmitter() {
    set_config(ParticleConfig{});
}

ParticleEmitter::ParticleEmitter(const ParticleConfig& config) {
    set_config(config);
}

float ParticleEmitter::random_float(float min_v, float max_v) {
    if (min_v >= max_v) return min_v;
    std::uniform_real_distribution<float> dist(min_v, max_v);
    return dist(m_rng);
}

void ParticleEmitter::set_config(const ParticleConfig& config) {
    m_config = config;
    m_particles.resize(static_cast<size_t>(std::max(1, m_config.max_particles)));
    for (auto& p : m_particles) {
        p.active = false;
    }
    m_alive_count = 0;
}

void ParticleEmitter::reset() {
    for (auto& p : m_particles) {
        p.active = false;
    }
    m_alive_count = 0;
    m_emit_accumulator = 0.0f;
}

void ParticleEmitter::emit(int count) {
    for (int c = 0; c < count; ++c) {
        for (auto& p : m_particles) {
            if (!p.active) {
                p.active = true;
                p.life = random_float(m_config.lifetime_min, m_config.lifetime_max);
                p.max_life = p.life;

                p.position = m_config.position + glm::vec3(
                    random_float(-m_config.position_variance.x, m_config.position_variance.x),
                    random_float(-m_config.position_variance.y, m_config.position_variance.y),
                    random_float(-m_config.position_variance.z, m_config.position_variance.z)
                );

                p.velocity = glm::vec3(
                    random_float(m_config.velocity_min.x, m_config.velocity_max.x),
                    random_float(m_config.velocity_min.y, m_config.velocity_max.y),
                    random_float(m_config.velocity_min.z, m_config.velocity_max.z)
                );

                p.acceleration = m_config.acceleration;
                p.size_start = random_float(m_config.size_start_min, m_config.size_start_max);
                p.size_end = m_config.size_end;
                p.color_start = m_config.color_start;
                p.color_end = m_config.color_end;
                p.rotation = random_float(0.0f, 6.2831853f);
                p.rot_speed = random_float(m_config.rot_speed_min, m_config.rot_speed_max);

                m_alive_count++;
                break;
            }
        }
    }
}

void ParticleEmitter::update(float dt) {
    if (m_active && m_config.emission_rate > 0.0f) {
        m_emit_accumulator += dt;
        float interval = 1.0f / m_config.emission_rate;
        while (m_emit_accumulator >= interval) {
            emit(1);
            m_emit_accumulator -= interval;
        }
    }

    m_alive_count = 0;
    for (auto& p : m_particles) {
        if (!p.active) continue;

        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }

        p.velocity += p.acceleration * dt;
        p.position += p.velocity * dt;
        p.rotation += p.rot_speed * dt;
        m_alive_count++;
    }
}

void ParticleEmitter::draw(Batch2D& batch) {
    if (m_alive_count == 0) return;

    BlendMode prev_mode = batch.get_blend_mode();
    if (m_config.blend_mode != prev_mode) {
        batch.set_blend_mode(m_config.blend_mode);
    }

    GLuint tex = m_config.texture_id ? m_config.texture_id : batch.get_white_texture_id();

    for (const auto& p : m_particles) {
        if (!p.active) continue;

        float t = 1.0f - (p.life / p.max_life);
        t = std::clamp(t, 0.0f, 1.0f);

        float current_size = glm::mix(p.size_start, p.size_end, t);
        glm::vec4 current_color = glm::mix(p.color_start, p.color_end, t);

        float half = current_size * 0.5f;
        batch.draw_sprite(
            tex,
            p.position.x - half,
            p.position.y - half,
            current_size,
            current_size,
            0.0f, 0.0f, 1.0f, 1.0f,
            current_color,
            p.rotation,
            half, half
        );
    }

    if (m_config.blend_mode != prev_mode) {
        batch.set_blend_mode(prev_mode);
    }
}

void ParticleEmitter::draw_3d(MeshRenderer3D& renderer) {
    if (m_alive_count == 0) return;

    GLuint tex = m_config.texture_id ? m_config.texture_id : renderer.get_fallback_texture_id();

    for (const auto& p : m_particles) {
        if (!p.active) continue;

        float t = 1.0f - (p.life / p.max_life);
        t = std::clamp(t, 0.0f, 1.0f);

        float current_size = glm::mix(p.size_start, p.size_end, t);
        glm::vec4 current_color = glm::mix(p.color_start, p.color_end, t);

        renderer.draw_billboard_rot(
            tex,
            p.position,
            glm::vec2(current_size),
            p.rotation,
            BillboardMode::Spherical,
            current_color
        );
    }
}

} // namespace crayon
