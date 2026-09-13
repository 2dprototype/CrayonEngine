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
    bool m_is_skinned = false;
    std::vector<SubMesh> m_submeshes;
};

class MeshRenderer3D {
public:
    MeshRenderer3D();
    ~MeshRenderer3D();

    bool init();
    void shutdown();

    void begin(const Camera& camera, float aspect);
    void end();

    void draw_mesh(const Mesh3D& mesh, const glm::mat4& model, GLuint texture_id = 0);
    void draw_mesh_skinned(const Mesh3D& mesh, const glm::mat4& model, const glm::mat4* bone_matrices, size_t bone_count, GLuint texture_id = 0);
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

    void set_shading_mode(ShadingMode mode) { m_shading_mode = mode; }
    ShadingMode get_shading_mode() const { return m_shading_mode; }

    GLuint get_fallback_texture_id() const;

private:
    std::unique_ptr<Shader> m_shader;
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
    ShadingMode m_shading_mode = ShadingMode::Gouraud;
    RetroEffects m_retro;

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
    std::vector<Vertex3D> m_line_batch;
};

} // namespace crayon
