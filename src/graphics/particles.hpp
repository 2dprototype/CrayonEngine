#pragma once

#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "batch2d.hpp"
#include "mesh3d.hpp"

namespace crayon {

struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 acceleration{0.0f};
    glm::vec4 color_start{1.0f};
    glm::vec4 color_end{1.0f, 1.0f, 1.0f, 0.0f};
    float size_start = 8.0f;
    float size_end = 0.0f;
    float life = 1.0f;
    float max_life = 1.0f;
    float rotation = 0.0f;
    float rot_speed = 0.0f;
    bool active = false;
};

struct ParticleConfig {
    int max_particles = 256;
    float emission_rate = 30.0f; // particles per second
    float lifetime_min = 0.5f;
    float lifetime_max = 1.5f;

    glm::vec3 position{0.0f};
    glm::vec3 position_variance{0.0f};

    glm::vec3 velocity_min{-10.0f, 20.0f, -10.0f};
    glm::vec3 velocity_max{10.0f, 60.0f, 10.0f};
    glm::vec3 acceleration{0.0f, -50.0f, 0.0f}; // Gravity

    float size_start_min = 6.0f;
    float size_start_max = 10.0f;
    float size_end = 0.0f;

    glm::vec4 color_start{1.0f, 0.8f, 0.2f, 1.0f};
    glm::vec4 color_end{0.9f, 0.1f, 0.0f, 0.0f};

    float rot_speed_min = -3.14f;
    float rot_speed_max = 3.14f;

    GLuint texture_id = 0;
    BlendMode blend_mode = BlendMode::Alpha;
    bool is_3d = false;
};

class ParticleEmitter {
public:
    ParticleEmitter();
    explicit ParticleEmitter(const ParticleConfig& config);
    ~ParticleEmitter() = default;

    void set_config(const ParticleConfig& config);
    const ParticleConfig& get_config() const { return m_config; }
    ParticleConfig& get_config_ref() { return m_config; }

    void emit(int count = 1);
    void burst(int count) { emit(count); }
    void update(float dt);
    void draw(Batch2D& batch);
    void draw_3d(MeshRenderer3D& renderer);
    void reset();

    bool is_active() const { return m_active; }
    void set_active(bool active) { m_active = active; }
    int get_alive_count() const { return m_alive_count; }

private:
    ParticleConfig m_config;
    std::vector<Particle> m_particles;
    float m_emit_accumulator = 0.0f;
    int m_alive_count = 0;
    bool m_active = true;

    std::mt19937 m_rng{1337};
    float random_float(float min_v, float max_v);
};

} // namespace crayon
