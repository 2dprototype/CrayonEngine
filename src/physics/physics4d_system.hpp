#pragma once

// Physics4DSystem: engine-side wrapper around the standalone hv4d library
// (vendor/hypervis4d).  It owns the hv4d::World, the current hyperplane slice
// used for visualisation/picking, buffered collision events, and the debug
// renderer which draws 4D bodies into the 3D scene through MeshRenderer3D.

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <hv4d/hv4d.hpp>

namespace crayon {

class MeshRenderer3D;

enum class Physics4DEventType {
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit
};

struct Physics4DEvent {
    Physics4DEventType type = Physics4DEventType::CollisionEnter;
    uint32_t bodyA = 0;
    uint32_t bodyB = 0;
    glm::vec4 normal{0.0f};
    float impulse = 0.0f;
};

struct Physics4DDebugFlags {
    bool shapes     = true;   // wireframe of every body's current 3D slice
    bool fill       = false;  // lit, filled slice surfaces
    bool projection = false;  // full 4D wireframe projected into the slice (fades with distance from it)
    bool contacts   = true;   // contact points + normals from the last step
    bool bounds     = false;  // bounding hyperspheres
    bool velocities = false;  // linear velocity arrows (projected)
    bool planes     = true;   // half-space planes (grid patch)

    glm::vec4 color        {0.20f, 1.00f, 0.40f, 1.0f};  // dynamic bodies
    glm::vec4 static_color {0.45f, 0.55f, 0.80f, 1.0f};  // static bodies / planes
    glm::vec4 sensor_color {1.00f, 0.80f, 0.20f, 1.0f};  // sensors
    glm::vec4 contact_color{1.00f, 0.25f, 0.20f, 1.0f};

    float projection_range = 3.0f;  // distance from the slice at which projections fade out
    float plane_extent     = 8.0f;  // half-size of the grid drawn for a half-space
    int   plane_divisions  = 8;
};

class Physics4DSystem {
public:
    Physics4DSystem();
    ~Physics4DSystem();

    bool init();
    void shutdown();
    void update(float dt);

    hv4d::World&       world()       { return m_world; }
    const hv4d::World& world() const { return m_world; }

    // ---- Slice (the 3D hyperplane through 4D space that gets rendered / picked) ----
    void  set_slice_w(float w);                       // classic slice: constant w
    float get_slice_w() const { return m_slice_w; }
    void  set_slice_hyperplane(const glm::vec4& normal, const glm::vec4& base);
    const hv4d::SlicePlane& slice() const { return m_slice; }
    bool  is_axis_slice() const { return m_axis_slice; }

    // ---- Events ----
    std::vector<Physics4DEvent> get_and_clear_events();

    // ---- Debug drawing (call during crayon.draw, i.e. inside the 3D pass) ----
    void draw_debug(MeshRenderer3D& renderer, const Physics4DDebugFlags& flags = Physics4DDebugFlags{});

    // ---- Helpers ----
    static hv4d::Vec4 to_hv(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }
    static glm::vec4  to_glm(const hv4d::Vec4& v) { return {v.x, v.y, v.z, v.w}; }

    // Parse "8cell", "tesseract", "600-cell", ... -> solid.  Returns false if unknown.
    static bool parse_solid(const char* name, hv4d::RegularSolid& out);
    static const char* solid_name(hv4d::RegularSolid s);

private:
    hv4d::World m_world;
    hv4d::SlicePlane m_slice = hv4d::SlicePlane::at_w(0.0f);
    float m_slice_w = 0.0f;
    bool m_axis_slice = true;
    bool m_initialized = false;

    std::vector<hv4d::SliceTriangle> m_tri_scratch;
    std::vector<hv4d::SliceSegment> m_seg_scratch;
};

} // namespace crayon
