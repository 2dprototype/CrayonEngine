#include "physics4d_system.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "../core/log.hpp"
#include "../graphics/mesh3d.hpp"

namespace crayon {

namespace {

inline glm::vec3 g3(const hv4d::Vec3& v) { return glm::vec3(v.x, v.y, v.z); }

// Fade a colour towards "nothing" (black + transparent) as a feature moves away from the slice.
inline glm::vec4 faded(const glm::vec4& c, float f) { return glm::vec4(c.r * f, c.g * f, c.b * f, c.a * f); }

inline float fade_of(float dist, float range) {
    if (range <= 0.0f) return 1.0f;
    return std::min(1.0f, std::max(0.0f, 1.0f - std::fabs(dist) / range));
}

// Stable pseudo-random value in [0,1) for colouring individual cells.
inline float hash01(int i) {
    const float x = std::sin(static_cast<float>(i) * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

inline const glm::vec4& body_color(const hv4d::Body& b, const Physics4DDebugFlags& f) {
    if (b.sensor) return f.sensor_color;
    if (b.stationary) return f.static_color;
    return f.color;
}

std::string normalize_name(const char* s) {
    std::string out;
    for (const char* p = s; p && *p; ++p) {
        if (std::isalnum(static_cast<unsigned char>(*p))) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    return out;
}

} // namespace

Physics4DSystem::Physics4DSystem() = default;
Physics4DSystem::~Physics4DSystem() = default;

bool Physics4DSystem::init() {
    m_world.clear();
    m_world.gravity = hv4d::Vec4(0.0f, -9.8f, 0.0f, 0.0f);
    m_slice_w = 0.0f;
    m_slice = hv4d::SlicePlane::at_w(0.0f);
    m_axis_slice = true;
    m_initialized = true;
    CRAYON_LOG_INFO("Physics4DSystem initialized (hv4d)");
    return true;
}

void Physics4DSystem::shutdown() {
    m_world.clear();
    m_tri_scratch.clear();
    m_seg_scratch.clear();
    m_initialized = false;
}

void Physics4DSystem::update(float dt) {
    if (!m_initialized) return;
    m_world.update(dt);
}

void Physics4DSystem::set_slice_w(float w) {
    m_slice_w = w;
    m_slice = hv4d::SlicePlane::at_w(w);
    m_axis_slice = true;
}

void Physics4DSystem::set_slice_hyperplane(const glm::vec4& normal, const glm::vec4& base) {
    const hv4d::Vec4 n = to_hv(normal);
    if (!(n.length() > 1e-6f)) return;  // ignore degenerate normals
    m_slice = hv4d::SlicePlane::from_normal(n, to_hv(base));
    m_slice_w = base.w;
    m_axis_slice = false;
}

std::vector<Physics4DEvent> Physics4DSystem::get_and_clear_events() {
    std::vector<Physics4DEvent> out;
    for (const hv4d::WorldEvent& e : m_world.take_events()) {
        Physics4DEvent ev;
        switch (e.type) {
            case hv4d::WorldEventType::CollisionEnter: ev.type = Physics4DEventType::CollisionEnter; break;
            case hv4d::WorldEventType::CollisionExit:  ev.type = Physics4DEventType::CollisionExit;  break;
            case hv4d::WorldEventType::TriggerEnter:   ev.type = Physics4DEventType::TriggerEnter;   break;
            case hv4d::WorldEventType::TriggerExit:    ev.type = Physics4DEventType::TriggerExit;    break;
        }
        ev.bodyA = e.a;
        ev.bodyB = e.b;
        ev.normal = to_glm(e.normal);
        ev.impulse = e.impulse;
        out.push_back(ev);
    }
    return out;
}

bool Physics4DSystem::parse_solid(const char* name, hv4d::RegularSolid& out) {
    const std::string n = normalize_name(name);
    using S = hv4d::RegularSolid;
    if (n == "5cell" || n == "pentachoron" || n == "simplex" || n == "hypertetrahedron") { out = S::FiveCell; return true; }
    if (n == "8cell" || n == "tesseract" || n == "hypercube" || n == "cube4d")           { out = S::EightCell; return true; }
    if (n == "16cell" || n == "hexadecachoron" || n == "crosspolytope")                   { out = S::SixteenCell; return true; }
    if (n == "24cell" || n == "icositetrachoron" || n == "octaplex")                      { out = S::TwentyFourCell; return true; }
    if (n == "120cell" || n == "hecatonicosachoron" || n == "dodecaplex")                 { out = S::OneTwentyCell; return true; }
    if (n == "600cell" || n == "hexacosichoron" || n == "tetraplex")                      { out = S::SixHundredCell; return true; }
    return false;
}

const char* Physics4DSystem::solid_name(hv4d::RegularSolid s) {
    switch (s) {
        case hv4d::RegularSolid::FiveCell:       return "5cell";
        case hv4d::RegularSolid::EightCell:      return "8cell";
        case hv4d::RegularSolid::SixteenCell:    return "16cell";
        case hv4d::RegularSolid::TwentyFourCell: return "24cell";
        case hv4d::RegularSolid::OneTwentyCell:  return "120cell";
        case hv4d::RegularSolid::SixHundredCell: return "600cell";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Debug rendering
// ---------------------------------------------------------------------------
void Physics4DSystem::draw_debug(MeshRenderer3D& r, const Physics4DDebugFlags& f) {
    if (!m_initialized) return;

    const hv4d::SlicePlane& plane = m_slice;
    const auto& bodies = m_world.bodies();

    // ---- 1. filled slice surfaces (immediate, lit) ----------------------------------
    if (f.fill) {
        const glm::vec4 saved_model_color = r.get_model_color();
        for (const hv4d::Body& b : bodies) {
            const glm::vec4& base = body_color(b, f);
            if (b.collider.type == hv4d::ColliderType::Mesh) {
                m_tri_scratch.clear();
                hv4d::slice_body_triangles(b, plane, m_tri_scratch);
                if (m_tri_scratch.empty()) continue;

                // The slice of a convex body is a convex polyhedron: the area-less average of the
                // triangle centroids lies inside it, so we can orient every triangle outwards from it.
                glm::vec3 center(0.0f);
                for (const auto& t : m_tri_scratch) center += (g3(t.p[0]) + g3(t.p[1]) + g3(t.p[2])) / 3.0f;
                center /= static_cast<float>(m_tri_scratch.size());

                for (const auto& t : m_tri_scratch) {
                    glm::vec3 p0 = g3(t.p[0]), p1 = g3(t.p[1]), p2 = g3(t.p[2]);
                    const glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
                    const glm::vec3 c = (p0 + p1 + p2) / 3.0f;
                    if (glm::dot(n, c - center) < 0.0f) std::swap(p1, p2);
                    const float shade = 0.60f + 0.40f * hash01(t.cell);
                    r.draw_triangle_3d(p0, p1, p2, glm::vec4(base.r * shade, base.g * shade, base.b * shade, base.a));
                }
            } else if (b.collider.type == hv4d::ColliderType::Sphere) {
                hv4d::Vec3 c;
                float rad = 0.0f;
                if (hv4d::slice_sphere(b, plane, c, rad)) {
                    r.set_model_color(base);
                    r.draw_sphere(g3(c), rad);
                }
            }
        }
        r.set_model_color(saved_model_color);
    }

    // ---- 2. everything else goes through the batched line renderer ------------------
    r.begin_line_batch();

    if (f.shapes || f.projection || f.bounds || f.velocities) {
        for (const hv4d::Body& b : bodies) {
            const glm::vec4& col = body_color(b, f);
            const float body_dist = plane.signed_distance(b.pos);

            switch (b.collider.type) {
                case hv4d::ColliderType::Mesh: {
                    const hv4d::Mesh& mesh = *b.collider.mesh;

                    if (f.shapes) {
                        m_seg_scratch.clear();
                        hv4d::slice_body_wireframe(b, plane, m_seg_scratch);
                        for (const auto& s : m_seg_scratch) r.add_line_to_batch(g3(s.a), g3(s.b), col);
                    }

                    if (f.projection && std::fabs(body_dist) < mesh.radius + f.projection_range) {
                        const hv4d::Mat4 rot = b.rotation.to_matrix();
                        std::vector<hv4d::Vec4> world(mesh.vertices.size());
                        std::vector<glm::vec3> proj(mesh.vertices.size());
                        std::vector<float> fade(mesh.vertices.size());
                        for (size_t i = 0; i < world.size(); ++i) {
                            world[i] = rot * mesh.vertices[i] + b.pos;
                            proj[i] = g3(plane.project(world[i]));
                            fade[i] = fade_of(plane.signed_distance(world[i]), f.projection_range);
                        }
                        for (const hv4d::Edge& e : mesh.edges) {
                            const float fe = 0.5f * (fade[e.hd_vertex] + fade[e.tl_vertex]);
                            if (fe < 0.02f) continue;
                            r.add_line_to_batch(proj[e.hd_vertex], proj[e.tl_vertex], faded(col, fe * 0.6f));
                        }
                    }
                    break;
                }
                case hv4d::ColliderType::Sphere: {
                    const float R = b.collider.radius;
                    if (f.shapes) {
                        hv4d::Vec3 c;
                        float rad = 0.0f;
                        if (hv4d::slice_sphere(b, plane, c, rad)) r.batch_wire_sphere(g3(c), rad, col, 8, 20);
                    }
                    if (f.projection && std::fabs(body_dist) < R + f.projection_range) {
                        const float fe = fade_of(std::max(0.0f, std::fabs(body_dist) - R), f.projection_range);
                        if (fe > 0.02f) {
                            const glm::vec3 c = g3(plane.project(b.pos));
                            r.batch_wire_sphere(c, R, faded(col, fe * 0.5f), 6, 16);
                        }
                    }
                    break;
                }
                case hv4d::ColliderType::HalfSpace: {
                    if (!f.planes) break;
                    hv4d::Vec3 n3;
                    float offset = 0.0f;
                    if (!hv4d::slice_half_space(b, plane, n3, offset)) break;

                    const float l = hv4d::length(n3);
                    const hv4d::Vec3 n = n3 / l;
                    const float d = offset / l;
                    const hv4d::Vec3 foot = n * d;

                    // orthonormal basis of the plane
                    const hv4d::Vec3 helper = std::fabs(n.y) < 0.9f ? hv4d::Vec3(0, 1, 0) : hv4d::Vec3(1, 0, 0);
                    const hv4d::Vec3 u = hv4d::normalize(hv4d::cross(n, helper));
                    const hv4d::Vec3 v = hv4d::cross(n, u);

                    const int N = std::max(1, f.plane_divisions);
                    const float E = f.plane_extent;
                    const glm::vec4 pc = faded(f.static_color, 0.7f);
                    for (int i = -N; i <= N; ++i) {
                        const float t = E * static_cast<float>(i) / static_cast<float>(N);
                        r.add_line_to_batch(g3(foot + u * t - v * E), g3(foot + u * t + v * E), pc);
                        r.add_line_to_batch(g3(foot + v * t - u * E), g3(foot + v * t + u * E), pc);
                    }
                    break;
                }
            }

            if (f.bounds && b.collider.type != hv4d::ColliderType::HalfSpace) {
                const float R = b.collider.bounding_radius();
                const float r2 = R * R - body_dist * body_dist;
                if (r2 > 0.0f) {
                    const glm::vec3 c = g3(plane.project(b.pos - plane.normal * body_dist));
                    r.batch_wire_sphere(c, std::sqrt(r2), glm::vec4(0.6f, 0.6f, 0.6f, 0.6f), 6, 16);
                }
            }

            if (f.velocities && !b.stationary && b.collider.type != hv4d::ColliderType::HalfSpace) {
                const float fe = fade_of(std::max(0.0f, std::fabs(body_dist) - b.collider.bounding_radius()),
                                         f.projection_range);
                if (fe > 0.02f) {
                    const glm::vec3 p0 = g3(plane.project(b.pos));
                    const glm::vec3 v3(b.vel.linear.dot(plane.e[0]), b.vel.linear.dot(plane.e[1]),
                                       b.vel.linear.dot(plane.e[2]));
                    const glm::vec4 vc(1.0f * fe, 0.9f * fe, 0.2f * fe, fe);
                    r.add_line_to_batch(p0, p0 + v3 * 0.3f, vc);
                    // a short tick whose length shows motion along the slice normal (the "w" velocity)
                    const float vn = b.vel.linear.dot(plane.normal);
                    if (std::fabs(vn) > 0.05f) {
                        const float s = std::min(0.6f, std::fabs(vn) * 0.15f);
                        const glm::vec4 wc = vn > 0.0f ? glm::vec4(0.9f * fe, 0.3f * fe, 1.0f * fe, fe)
                                                       : glm::vec4(0.3f * fe, 0.9f * fe, 1.0f * fe, fe);
                        r.add_line_to_batch(p0 + glm::vec3(-s, 0, 0), p0 + glm::vec3(s, 0, 0), wc);
                        r.add_line_to_batch(p0 + glm::vec3(0, -s, 0), p0 + glm::vec3(0, s, 0), wc);
                    }
                }
            }
        }
    }

    // ---- 3. contacts ---------------------------------------------------------------
    if (f.contacts) {
        for (const hv4d::ContactDebugInfo& c : m_world.last_contacts()) {
            for (const hv4d::Vec4& p : c.points) {
                const float fe = fade_of(plane.signed_distance(p), f.projection_range);
                if (fe < 0.02f) continue;
                const glm::vec3 q = g3(plane.project(p));
                const glm::vec4 cc = faded(f.contact_color, fe);
                const float s = 0.07f;
                r.add_line_to_batch(q + glm::vec3(-s, 0, 0), q + glm::vec3(s, 0, 0), cc);
                r.add_line_to_batch(q + glm::vec3(0, -s, 0), q + glm::vec3(0, s, 0), cc);
                r.add_line_to_batch(q + glm::vec3(0, 0, -s), q + glm::vec3(0, 0, s), cc);
                const glm::vec3 n3(c.normal.dot(plane.e[0]), c.normal.dot(plane.e[1]), c.normal.dot(plane.e[2]));
                r.add_line_to_batch(q, q + n3 * 0.4f, cc);
            }
        }
    }

    r.end_line_batch();
}

} // namespace crayon
