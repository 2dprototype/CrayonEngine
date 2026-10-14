#pragma once

#include <vector>
#include <string>
#include <memory>
#include <stack>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glad/glad.h>
#include "shader.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "backend.hpp"
#include "soft/soft_raster.hpp"
#include "skybox.hpp"

namespace crayon {

struct Vertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;
};

struct SkinnedVertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color{1.0f};
    glm::uvec4 joints{0, 0, 0, 0};
    glm::vec4 weights{0.0f, 0.0f, 0.0f, 0.0f};
};

// One draw range within a Mesh3D that shares a single texture/material.
// Populated automatically when a source has more than one material (e.g. a
// glTF file with several materials) so Mesh3D::draw() renders each range
// with its own texture instead of collapsing every part onto whichever
// single texture the caller happened to have bound. Meshes with only one
// material (procedural shapes, single-material OBJ/glTF) have no submeshes
// and draw exactly as before.
struct SubMesh {
    GLuint index_offset = 0;
    GLuint index_count = 0;
    GLuint texture_id = 0;
    glm::vec4 color{1.0f};
};

struct PointLight {
    glm::vec3 pos{0.0f};
    glm::vec3 color{1.0f};
    float radius = 10.0f;
    float intensity = 1.0f;
    bool enabled = false;
};

struct SpotLight {
    glm::vec3 pos{0.0f};
    glm::vec3 dir{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float radius = 15.0f;
    float intensity = 1.0f;
    float inner_cutoff_cos = 0.95f; // cos(~18 deg)
    float outer_cutoff_cos = 0.90f; // cos(~25 deg)
    bool enabled = false;
};

enum class ShadingMode {
    Gouraud = 0,
    Flat = 1,
    Unlit = 2
};

enum class BillboardMode {
    Spherical = 0,
    Cylindrical = 1
};

struct RetroEffects {
    bool jitter_enabled = true;
    glm::vec2 jitter_resolution{160.0f, 120.0f};
    float affine_blend = 1.0f; // 0.0 = perspective, 1.0 = affine
    bool dither_enabled = true;
    float dither_levels = 32.0f;

    bool fog_enabled = true;
    float fog_start = 5.0f;
    float fog_end = 25.0f;
    glm::vec3 fog_color{0.08f, 0.08f, 0.12f};

    // CRT effects
    bool crt_scanlines = false;
    float scanline_strength = 0.25f;
    bool crt_curvature = false;
    float curvature_distort = 0.05f;
    bool vignette = false;
    float vignette_strength = 0.25f;
};

class Model3D;

class Mesh3D {
public:
    Mesh3D();
    ~Mesh3D();

    bool load_from_obj(const std::string& filepath);
    bool load_from_gltf(const std::string& filepath);
    void create_from_data(const std::vector<Vertex3D>& vertices, const std::vector<GLuint>& indices);
    void create_skinned_from_data(const std::vector<SkinnedVertex3D>& vertices, const std::vector<GLuint>& indices);

    void draw() const;
    bool is_skinned() const { return m_is_skinned; }

    // True when this mesh was loaded from a multi-material source (e.g. a
    // glTF file with several materials) and therefore carries per-range
    // textures that draw() will bind automatically.
    bool has_submeshes() const { return !m_submeshes.empty(); }
    const std::vector<SubMesh>& get_submeshes() const { return m_submeshes; }

    // Software backend only: the mesh lives in these CPU arrays instead of GL buffers.
    const std::vector<Vertex3D>& cpu_vertices() const { return m_cpu_vertices; }
    const std::vector<SkinnedVertex3D>& cpu_skinned_vertices() const { return m_cpu_skinned; }
    const std::vector<GLuint>& cpu_indices() const { return m_cpu_indices; }

    // Local-space AABB measured from the real vertex data at creation time. Used for automatic
    // frustum / distance culling of every mesh (primitives, custom meshes, models).
    // Keep a CPU copy of the vertices/indices even on the GPU backend (always on for primitives).
    void set_retain_cpu(bool on) { m_retain_cpu = on; }
    bool has_bounds() const { return m_bounds_valid; }
    const glm::vec3& bounds_min() const { return m_bounds_min; }
    const glm::vec3& bounds_max() const { return m_bounds_max; }

    static std::shared_ptr<Mesh3D> create_cube(float size = 1.0f);
    static std::shared_ptr<Mesh3D> create_plane(float width = 10.0f, float depth = 10.0f, int grid_subdivisions = 10);
    static std::shared_ptr<Mesh3D> create_sphere(float radius = 0.5f, int rings = 12, int sectors = 12);
    static std::shared_ptr<Mesh3D> create_cylinder(float radius = 0.5f, float height = 1.0f, int sectors = 12);
    static std::shared_ptr<Mesh3D> create_cone(float radius = 0.5f, float height = 1.0f, int sectors = 12);
    static std::shared_ptr<Mesh3D> create_pyramid(float base_size = 1.0f, float height = 1.0f);
    static std::shared_ptr<Mesh3D> create_torus(float radius = 0.8f, float tube_radius = 0.25f, int radial_segments = 16, int tubular_segments = 12);
    static std::shared_ptr<Mesh3D> create_capsule(float radius = 0.4f, float height = 0.8f, int rings = 8, int sectors = 12);
    static std::shared_ptr<Mesh3D> create_grid(float size = 20.0f, int divisions = 20);

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLsizei m_index_count = 0;
    GLsizei m_vertex_count = 0;
    GLenum m_index_type = GL_UNSIGNED_INT;   // GL_UNSIGNED_SHORT when < 65536 vertices (halves index bandwidth)
    size_t m_index_size = sizeof(GLuint);
    bool m_is_skinned = false;
    std::vector<SubMesh> m_submeshes;
    std::vector<Vertex3D> m_cpu_vertices;           // software backend storage
    std::vector<SkinnedVertex3D> m_cpu_skinned;
    std::vector<GLuint> m_cpu_indices;
    glm::vec3 m_bounds_min{0.0f};
    glm::vec3 m_bounds_max{0.0f};
    bool m_bounds_valid = false;
    bool m_retain_cpu = false;
};

class MeshRenderer3D {
public:
    MeshRenderer3D();
    ~MeshRenderer3D();

    bool init();
    void shutdown();

    void begin(const Camera& camera, float aspect);
    void end();

    // If bounds_min/bounds_max (mesh-local AABB) are given, the draw is skipped when the
    // box is entirely outside the camera frustum. Leave null for meshes with unknown bounds.
    void draw_mesh(const Mesh3D& mesh, const glm::mat4& model,
                   GLuint texture_id = 0,
                   const glm::vec4& tint = glm::vec4(1.0f),
                   const glm::vec3* bounds_min = nullptr,
                   const glm::vec3* bounds_max = nullptr,
                   bool single_sided = false,
                   bool alpha_blend = false);

    // ---- transparency ----
    // A draw is transparent when its colour alpha < 1 or it is flagged alpha_blend (glTF
    // alphaMode BLEND). Transparent draws are queued, then drawn back-to-front after the opaque
    // geometry with alpha blending and no depth writes. The queue is flushed automatically at the
    // end of the 3D pass, when depth testing is turned off (HUD time) and by flush_transparent().
    void flush_transparent();

    // Draws the skybox behind everything already drawn (and anything drawn later is unaffected):
    // it only fills pixels that nothing has covered, so call it first or last. `tint` multiplies it.
    void draw_skybox(Skybox& sky, const glm::vec3& tint = glm::vec3(1.0f));
    size_t get_transparent_queue_size() const { return m_transparent.size(); }

    // Frustum test of a local-space AABB transformed by `model` (includes the matrix stack).
    bool aabb_visible(const glm::vec3& mn, const glm::vec3& mx, const glm::mat4& model) const;
    // World-space sphere vs frustum.
    bool sphere_visible(const glm::vec3& center, float radius) const;

    // ---- culling controls ----
    // Auto culling = frustum + max-distance culling for every mesh, and backface culling for
    // parts known to be single-sided (closed primitives, glTF materials with doubleSided=false).
    // Two-sided geometry (planes, OBJ models, doubleSided glTF) is never backface-culled.
    void set_auto_cull(bool on) { m_auto_cull = on; }
    bool get_auto_cull() const { return m_auto_cull; }
    // Objects farther than this from the camera are skipped (0 = unlimited).
    void set_max_draw_distance(float d) { m_max_draw_distance = d; }
    float get_max_draw_distance() const { return m_max_draw_distance; }
    // Skinned meshes are culled with their bind-pose box grown by this fraction per axis,
    // since animation can move vertices outside it.
    void set_skinned_cull_margin(float m) { m_skinned_cull_margin = m; }
    const glm::vec3& get_camera_position() const { return m_cam_position; }

    // Number of draws skipped by frustum culling since begin() (for profiling).
    uint32_t get_culled_draw_count() const { return m_culled_draws; }

    void draw_mesh_skinned(const Mesh3D& mesh, const glm::mat4& model,
                           const glm::mat4* bone_matrices, size_t bone_count,
                           GLuint texture_id = 0,
                           const glm::vec4& tint = glm::vec4(1.0f));
    void draw_model(const Model3D& model, const glm::mat4& transform, GLuint override_texture = 0);
    void draw_model_skinned(const Model3D& model, const glm::mat4& transform, const glm::mat4* bone_matrices, size_t bone_count, GLuint override_texture = 0);

    // Direct Primitive Rendering Helpers
    void draw_cube(const glm::vec3& pos, const glm::vec3& size, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_plane(const glm::vec3& pos, float width, float depth, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_sphere(const glm::vec3& pos, float radius, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_cylinder(const glm::vec3& pos, float radius, float height, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_cone(const glm::vec3& pos, float radius, float height, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_pyramid(const glm::vec3& pos, float base_size, float height, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_torus(const glm::vec3& pos, float radius, float tube_radius, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));
    void draw_capsule(const glm::vec3& pos, float radius, float height, GLuint texture_id = 0, const glm::vec3& rot = glm::vec3(0.0f));

    // 3D Billboards
    void draw_billboard(GLuint texture_id, const glm::vec3& position, const glm::vec2& size,
                        BillboardMode mode = BillboardMode::Spherical, const glm::vec4& color = glm::vec4(1.0f),
                        float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);
    void draw_billboard_rot(GLuint texture_id, const glm::vec3& position, const glm::vec2& size,
                            float angle_rad, BillboardMode mode = BillboardMode::Spherical,
                            const glm::vec4& color = glm::vec4(1.0f),
                            float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);

    // 3D Immediate Geometry & Lines
    void draw_line_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color = glm::vec4(1.0f));
    void draw_lines_3d(const std::vector<glm::vec3>& points, const glm::vec4& color = glm::vec4(1.0f));
    void draw_lines_3d_batched(const Vertex3D* vertices, size_t count);

    // Fast batch line accumulator for debug renderers
    void begin_line_batch();
    void add_line_to_batch(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color);
    void batch_wire_box(const glm::vec3& center, const glm::vec3& half_extent, const glm::quat& rot, const glm::vec4& color);
    void batch_wire_sphere(const glm::vec3& center, float radius, const glm::vec4& color, int rings = 12, int sectors = 12);
    void batch_wire_capsule(const glm::vec3& center, float radius, float half_height, const glm::quat& rot, const glm::vec4& color, int segments = 12);
    void batch_wire_cylinder(const glm::vec3& center, float radius, float half_height, const glm::quat& rot, const glm::vec4& color, int segments = 12);
    void end_line_batch();

    void draw_grid_3d(float size, int divisions, float y_level = 0.0f, const glm::vec4& color = glm::vec4(0.4f, 0.4f, 0.5f, 1.0f));
    void draw_axes_3d(const glm::vec3& pos, float size = 1.0f);
    void draw_cube_wires(const glm::vec3& pos, const glm::vec3& size, const glm::vec4& color = glm::vec4(1.0f), const glm::vec3& rot = glm::vec3(0.0f));
    void draw_capsule_wires(const glm::vec3& pos, float radius, float half_height, const glm::vec4& color = glm::vec4(1.0f), const glm::vec3& rot = glm::vec3(0.0f));
    void draw_cylinder_wires(const glm::vec3& pos, float radius, float half_height, const glm::vec4& color = glm::vec4(1.0f), const glm::vec3& rot = glm::vec3(0.0f));
    void draw_ray_3d(const glm::vec3& start, const glm::vec3& dir, float length, const glm::vec4& color = glm::vec4(1.0f));
    void draw_skeleton_3d(const std::vector<glm::vec3>& joint_positions, const std::vector<std::pair<int, int>>& connections, const glm::vec4& color = glm::vec4(0.2f, 0.9f, 1.0f, 1.0f));
    void draw_segmented_mesh(const std::vector<std::shared_ptr<Mesh3D>>& meshes, const std::vector<glm::mat4>& transforms, const std::vector<GLuint>& textures);

    void draw_triangle_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                          const glm::vec4& color = glm::vec4(1.0f), GLuint texture_id = 0,
                          const glm::vec2& uv1 = {0,0}, const glm::vec2& uv2 = {1,0}, const glm::vec2& uv3 = {0,1});
    void draw_quad_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4,
                      const glm::vec4& color = glm::vec4(1.0f), GLuint texture_id = 0,
                      const glm::vec2& uv1 = {0,0}, const glm::vec2& uv2 = {1,0}, const glm::vec2& uv3 = {1,1}, const glm::vec2& uv4 = {0,1});

    // 3D Spatial Queries
    bool project(const glm::vec3& world_pos, float view_w, float view_h, glm::vec2& out_screen_pos) const;
    void unproject(const glm::vec2& screen_pos, float view_w, float view_h, glm::vec3& out_ray_orig, glm::vec3& out_ray_dir) const;

    // Transform Stack
    void push_matrix();
    void pop_matrix();
    void load_identity();
    void translate(const glm::vec3& translation);
    void rotate(float angle_rad, const glm::vec3& axis);
    void scale(const glm::vec3& factors);
    const glm::mat4& get_current_transform() const;

    // Retro effects and lighting
    void set_retro_effects(const RetroEffects& effects) { m_retro = effects; }
    RetroEffects& get_retro_effects() { return m_retro; }
    const RetroEffects& get_retro_effects() const { return m_retro; }

    void set_directional_light(const glm::vec3& dir, const glm::vec3& color, const glm::vec3& ambient);

    void set_point_light(int index, const glm::vec3& pos, const glm::vec3& color, float radius, float intensity);
    void set_point_light_enabled(int index, bool enabled);

    void set_spot_light(int index, const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float radius, float intensity, float inner_angle_deg = 15.0f, float outer_angle_deg = 25.0f);
    void set_spot_light_enabled(int index, bool enabled);

    void set_shading_mode(ShadingMode mode) { m_shading_mode = mode; }
    ShadingMode get_shading_mode() const { return m_shading_mode; }
    
    // Frame-wide tint applied by the procedural primitive helpers
    // (draw_cube, draw_sphere, ...). Does not affect draw_model* — those
    // use each part's own color. Not to be confused with setColor(), which
    // is 2D-only.
    void set_model_color(const glm::vec4& c) { m_active_color_3d = c; }
    const glm::vec4& get_model_color() const { return m_active_color_3d; }

    GLuint get_fallback_texture_id() const;

    // Fixed-function state that used to be raw glEnable/glDisable calls in the Lua bindings.
    // The GL path still sets GL state itself; these keep the software backend in sync.
    void set_cull_mode(int mode) { m_cull_mode = mode; }   // 0 = off, 1 = back, 2 = front
    void set_depth_test_enabled(bool on) { m_depth_test = on; }
    void set_depth_write_enabled(bool on) { m_depth_write = on; }

private:
    // Program state for the optimized mesh path. Per-frame uniforms (camera, lights, fog,
    // retro) are uploaded only when something changed since this program last saw them,
    // instead of ~35 glUniform calls on every draw.
    struct MeshProgState {
        std::unique_ptr<Shader> shader;
        GLint loc_model = -1, loc_normal = -1, loc_color = -1, loc_bones = -1;
        uint64_t uploaded_version = ~0ull;
        RetroEffects uploaded_retro;
        ShadingMode uploaded_shading = ShadingMode::Gouraud;
    };
    bool init_mesh_prog(MeshProgState& st, bool skinned);
    void upload_frame_uniforms(MeshProgState& st);

    // Legacy all-in-one program, still used by billboards, lines, immediate triangles, grid.
    // ---- software backend (mesh3d_soft.cpp) ----
    void soft_sync();
    void soft_draw_geometry(const Vertex3D* v, size_t nv, const GLuint* idx, size_t ni,
                            const glm::mat4& model, GLuint tex, const glm::vec4& tint);
    void soft_draw_mesh(const Mesh3D& mesh, const glm::mat4& model, GLuint tex, const glm::vec4& tint);
    void soft_draw_mesh_skinned(const Mesh3D& mesh, const glm::mat4& model, const glm::mat4* bones,
                                size_t bone_count, GLuint tex, const glm::vec4& tint);
    void soft_draw_billboard(GLuint tex, const Vertex3D quad[6]);
    void soft_draw_lines(const Vertex3D* v, size_t count, const glm::mat4& model);
    void soft_draw_triangle(const Vertex3D tri[3], GLuint tex);
    void soft_draw_grid(float size, int divisions, float y, const glm::vec4& color);
    std::vector<Vertex3D> m_soft_scratch;
    std::vector<GLuint> m_soft_index_scratch;
    bool m_auto_cull = true;
    float m_max_draw_distance = 0.0f;
    float m_skinned_cull_margin = 1.0f;
    struct TransparentDraw {
        const Mesh3D* mesh;
        glm::mat4 model;           // final model matrix (matrix stack already applied)
        GLuint texture;
        glm::vec4 tint;
        bool single_sided;
        bool has_bounds;
        glm::vec3 lo, hi;
        float distance;            // camera distance, used for back-to-front sorting
    };
    std::vector<TransparentDraw> m_transparent;
    std::unique_ptr<Shader> m_sky_shader;
    GLuint m_sky_vao = 0, m_sky_vbo = 0;
    bool m_in_transparent_flush = false;
    bool m_blend_pass = false;
    bool m_force_cull_back = false;     // set around a single draw when auto backface culling applies
    int m_cull_mode = 0;
    bool m_depth_test = true;
    bool m_depth_write = true;

    std::unique_ptr<Shader> m_shader;
    MeshProgState m_prog_static;    // draw_mesh(): no skinning code, CPU normal matrix
    MeshProgState m_prog_skinned;   // draw_mesh_skinned()
    int m_max_bones = 128;
    uint64_t m_frame_version = 0;   // bumped when camera / lights change
    glm::mat4 m_viewproj{1.0f};
    uint32_t m_culled_draws = 0;
    std::shared_ptr<Texture> m_checker_texture;
    std::shared_ptr<Texture> m_white_texture;

    std::stack<glm::mat4> m_matrix_stack;
    glm::mat4 m_current_matrix{1.0f};

    glm::mat4 m_view{1.0f};
    glm::mat4 m_proj{1.0f};
    glm::vec3 m_cam_position{0.0f};

    glm::vec3 m_light_dir{-0.5f, -1.0f, -0.7f};
    glm::vec3 m_light_color{1.0f, 0.95f, 0.9f};
    glm::vec3 m_ambient_color{0.25f, 0.25f, 0.3f};

    PointLight m_point_lights[4];
    SpotLight m_spot_lights[2];
    ShadingMode m_shading_mode = ShadingMode::Gouraud;
    RetroEffects m_retro;
    glm::vec4 m_active_color_3d{1.0f};

    // Pre-allocated procedural meshes for fast primitive rendering
    std::shared_ptr<Mesh3D> m_prim_cube;
    std::shared_ptr<Mesh3D> m_prim_plane;
    std::shared_ptr<Mesh3D> m_prim_sphere;
    std::shared_ptr<Mesh3D> m_prim_cylinder;
    std::shared_ptr<Mesh3D> m_prim_cone;
    std::shared_ptr<Mesh3D> m_prim_pyramid;
    std::shared_ptr<Mesh3D> m_prim_torus;
    std::shared_ptr<Mesh3D> m_prim_capsule;

    // Immediate dynamic buffer for 3D lines, triangles, billboards
    GLuint m_dyn_vao = 0;
    GLuint m_dyn_vbo = 0;
    size_t m_dyn_capacity = 0;              // vertices currently allocated in m_dyn_vbo
    std::vector<Vertex3D> m_line_batch;

    // Cached grid geometry (see draw_grid_3d)
    GLuint m_grid_vao = 0;
    GLuint m_grid_vbo = 0;
    size_t m_grid_count = 0;
    float  m_grid_size = -1.0f;
    int    m_grid_divs = -1;
    float  m_grid_y = 99999.0f;
    glm::vec4 m_grid_color{-1.0f};
};

} // namespace crayon
