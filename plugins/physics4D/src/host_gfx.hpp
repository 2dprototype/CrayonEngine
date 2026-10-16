#pragma once

// HostGfx: thin C++ wrapper over the 3D debug-drawing part of CrayonHostAPI, so the physics code can
// keep using glm types.  Every call is a no-op if the engine does not provide the function
// (older engine, or t.modules.mesh3D = false).

#include <cstddef>
#include <glm/glm.hpp>
#include "crayon_plugin.h"

namespace crayon {

struct HostGfx {
    const CrayonHostAPI* h = nullptr;

    glm::vec4 get_model_color() const {
        float c[4] = {1, 1, 1, 1};
        if (CRAYON_HOST_HAS(h, get_model_color)) h->get_model_color(c);
        return glm::vec4(c[0], c[1], c[2], c[3]);
    }
    void set_model_color(const glm::vec4& c) const {
        if (CRAYON_HOST_HAS(h, set_model_color)) h->set_model_color(&c.x);
    }
    void draw_triangle_3d(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec4& col) const {
        if (CRAYON_HOST_HAS(h, draw_triangle_3d)) h->draw_triangle_3d(&a.x, &b.x, &c.x, &col.x);
    }
    void draw_sphere(const glm::vec3& c, float r) const {
        if (CRAYON_HOST_HAS(h, draw_sphere_3d)) h->draw_sphere_3d(&c.x, r);
    }
    void begin_line_batch() const {
        if (CRAYON_HOST_HAS(h, line_batch_begin)) h->line_batch_begin();
    }
    void add_line_to_batch(const glm::vec3& a, const glm::vec3& b, const glm::vec4& col) const {
        if (CRAYON_HOST_HAS(h, line_batch_add)) h->line_batch_add(&a.x, &b.x, &col.x);
    }
    void batch_wire_sphere(const glm::vec3& c, float r, const glm::vec4& col, int rings, int sectors) const {
        if (CRAYON_HOST_HAS(h, line_batch_wire_sphere)) h->line_batch_wire_sphere(&c.x, r, &col.x, rings, sectors);
    }
    void end_line_batch() const {
        if (CRAYON_HOST_HAS(h, line_batch_end)) h->line_batch_end();
    }
};

} // namespace crayon
