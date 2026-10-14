#pragma once
// Crayon software rasterizer: a complete CPU 3D pipeline for machines with no GPU
// (or no usable OpenGL driver). GL-free on purpose so it builds and tests headless.
//
// Pipeline:  vertex transform + vertex lighting + fog  ->  clip (6 planes)  ->
//            backface cull  ->  scanline raster with 32-bit depth  ->  texture
//            (mip-mapped, optional PS1-style affine UVs)  ->  alpha-test, fog, dither.
//
// Conventions match the GL path so the same models/scripts look the same:
//   * right-handed, CCW front faces, OpenGL clip space (z in [-w, w])
//   * framebuffer pixels are packed RGBA8 (r | g<<8 | b<<16 | a<<24), row 0 = top
//   * depth test is LEQUAL, blending is off (alpha only drives the cutout test)
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace crayon::soft {

// Layout-identical to crayon::Vertex3D (checked with a static_assert in the
// integration layer) so GL-side CPU copies can be fed in without conversion.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;
};

// ---------------------------------------------------------------------------
class Texture {
public:
    struct Level {
        int w = 0, h = 0;
        int wmask = 0, hmask = 0;
        bool pow2 = false;
        std::vector<uint32_t> px;
    };

    // channels: 1 = grey, 2 = grey+alpha, 3 = RGB, 4 = RGBA (same as GL Texture class).
    // Builds a full box-filtered mip chain unless build_mips is false.
    // `smooth` = bilinear sampling (the GL path's non-"nearest" textures); otherwise nearest.
    bool create(const unsigned char* data, int width, int height, int channels,
                bool build_mips = true, bool smooth = false);
    // Fills with one colour (used for the 1x1 white / checker fallbacks).
    void create_solid(uint32_t rgba);

    int  width()  const { return m_levels.empty() ? 0 : m_levels[0].w; }
    int  height() const { return m_levels.empty() ? 0 : m_levels[0].h; }
    int  level_count() const { return static_cast<int>(m_levels.size()); }
    const Level& level(int i) const { return m_levels[i]; }
    bool has_alpha() const { return m_has_alpha; }
    bool smooth() const { return m_smooth; }

    // Sampling state, switchable at runtime (graphics texture:setFilter / setWrap).
    enum class Wrap : int { Repeat = 0, Clamp = 1, Mirror = 2 };
    Wrap wrap() const { return m_wrap; }
    void set_wrap(Wrap w) { m_wrap = w; }
    void set_smooth(bool s) { m_smooth = s; }
    // Rebuilds (or drops) the mip chain from level 0.
    void set_mipmaps(bool enabled);
    // Replaces level 0 pixels in place (same size) and rebuilds the mips if it had them.
    bool update(const unsigned char* rgba, int width, int height);

private:
    void generate_mips();
    std::vector<Level> m_levels;
    Wrap m_wrap = Wrap::Repeat;
    bool m_has_alpha = false;
    bool m_smooth = false;
};

// The rest of the engine passes textures around as integer ids (GLuint on the GL path).
// In software mode crayon::Texture hands out ids from this registry instead, so ModelPart,
// Batch2D, the Lua API etc. keep working unchanged. Id 0 means "no texture".
class TextureRegistry {
public:
    static uint32_t add(std::shared_ptr<Texture> tex);
    static const Texture* get(uint32_t id);
    static void remove(uint32_t id);
};

// ---------------------------------------------------------------------------
enum class Shading : int { Gouraud = 0, Flat = 1, Unlit = 2 }; // same values as ShadingMode
enum class Cull : int { None = 0, Back = 1, Front = 2 };

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
    float inner_cos = 0.95f;
    float outer_cos = 0.90f;
    bool enabled = false;
};

struct Settings {
    // retro look
    bool jitter = false;                 // snap vertices to a coarse grid (PS1 wobble)
    glm::vec2 jitter_res{160.0f, 120.0f};
    bool affine = false;                 // screen-linear UVs (PS1 warp); false = perspective-correct
    bool dither = false;
    float dither_levels = 32.0f;
    bool fog = false;
    float fog_start = 5.0f, fog_end = 25.0f;
    glm::vec3 fog_color{0.08f, 0.08f, 0.12f};

    // state
    Shading shading = Shading::Gouraud;
    Cull cull = Cull::Back;
    bool mipmaps = true;
    bool bilinear = false;               // force bilinear on every texture (per-texture flag otherwise)
    float alpha_cutoff = 0.05f;
    bool depth_test = true;
    bool depth_write = true;
    bool blend = false;                  // src-alpha blending (billboards / particles)

    // lighting (same defaults as MeshRenderer3D)
    glm::vec3 light_dir{-0.5f, -1.0f, -0.7f};
    glm::vec3 light_color{1.0f, 0.95f, 0.9f};
    glm::vec3 ambient{0.25f, 0.25f, 0.3f};
    PointLight points[4];
    SpotLight spots[2];
};

struct Stats {
    uint64_t triangles_in = 0;
    uint64_t triangles_rasterized = 0;
    uint64_t pixels_shaded = 0;
    void reset() { *this = Stats{}; }
};

// ---------------------------------------------------------------------------
class Rasterizer {
public:
    Settings settings;
    Stats stats;

    void resize(int width, int height);
    void clear(const glm::vec4& color, bool clear_depth = true);

    // Set camera for the following draw calls.
    void begin(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cam_position);

    // Draw an indexed triangle list. `tex` may be null (= untextured white).
    // `tint` multiplies vertex colour (same as u_color in the GL shader).
    void draw(const Vertex* verts, size_t vert_count,
              const uint32_t* indices, size_t index_count,
              const glm::mat4& model, const Texture* tex,
              const glm::vec4& tint = glm::vec4(1.0f));

    // Depth-tested world-space line (debug draw, wireframes).
    void draw_line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);

    // Cheap axis-aligned test of a local-space AABB under `model` against the view frustum.
    bool aabb_visible(const glm::vec3& mn, const glm::vec3& mx, const glm::mat4& model) const;

    // ---- skybox ----
    // `sample` returns linear RGB for a world direction (e.g. crayon::Skybox::sample). Paints only
    // pixels nothing has been drawn on yet (depth untouched), so it can be called at any point of
    // the frame -- drawing it last is cheapest. View directions are affine in NDC for a perspective
    // camera, so each pixel's direction is bilinear from the four screen corners.
    void draw_skybox(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& tint,
                     const std::function<glm::vec3(const glm::vec3&)>& sample);

    // ---- 2D batch (mirrors Batch2D::flush) ----
    struct Vertex2D { glm::vec2 position; glm::vec2 uv; glm::vec4 color; };
    struct IRect { int x, y, w, h; };
    enum class Blend2D { Alpha, Additive, Multiply, None };
    // `v` holds `count` vertices (4 per quad, indices 0,1,2 / 2,3,0). Positions are transformed
    // by `proj_view` exactly like the GL 2D shader (y-down ortho). Exact (watertight) fixed-point
    // edge rules so alpha-blended quads never double-blend along the diagonal.
    void draw_2d(const Vertex2D* v, size_t count, const glm::mat4& proj_view, const Texture* tex,
                 Blend2D mode, const IRect* scissor);

    // ---- 2D stencil-style mask (graphics.beginMask / useMask / endMask) ----
    // Write: shapes drawn set the mask and leave colour untouched. Test: drawing only affects
    // pixels inside the mask (TestInverted: outside). Off: normal drawing.
    enum class MaskMode { Off, Write, Test, TestInverted };
    void set_mask_mode(MaskMode m) { m_mask_mode = m; }
    MaskMode mask_mode() const { return m_mask_mode; }
    void clear_mask();

    // ---- full-screen post effects (CPU equivalents of the GL post shader) ----
    struct PostOptions {
        bool dither = false;       float dither_levels = 32.0f;
        bool scanlines = false;    float scanline_strength = 0.25f;
        bool vignette = false;     float vignette_strength = 0.25f;
        bool any() const { return dither || scanlines || vignette; }
    };
    void post_process(const PostOptions& opt);

    int width() const { return m_w; }
    int height() const { return m_h; }
    const uint32_t* pixels() const { return m_color.data(); }
    uint32_t* pixels() { return m_color.data(); }

private:
    // post-projection vertex (clip-space position + attributes, all linear in clip space)
    struct SV {
        glm::vec4 clip;
        glm::vec2 uv;
        glm::vec4 col;   // lit vertex colour * tint
    };
    // per-source-vertex data produced by the vertex stage
    struct PV {
        glm::vec4 clip;
        glm::vec3 wpos;
        glm::vec2 uv;
        glm::vec4 base;  // vertex colour * tint
        glm::vec3 light; // per-vertex lighting (Gouraud / unlit)
        uint8_t oc;      // clip outcode
    };
    // screen-space vertex consumed by the span rasterizer
    struct SPoint {
        float x, y, z, iw;
        glm::vec2 uv;
        glm::vec4 col;
    };

    void raster_polygon(const SV* poly, int n, const Texture* tex);
    void raster_triangle(const SPoint& a, const SPoint& b, const SPoint& c, const Texture* tex);

    int m_w = 0, m_h = 0;
    std::vector<uint32_t> m_color;
    std::vector<float> m_depth;
    std::vector<uint8_t> m_mask;
    MaskMode m_mask_mode = MaskMode::Off;

    glm::mat4 m_view{1.0f}, m_proj{1.0f}, m_viewproj{1.0f};
    glm::vec3 m_cam{0.0f};

    // scratch reused across draws (no per-frame allocation)
    std::vector<PV> m_pv;
};

// The one framebuffer the whole software backend draws into (3D, 2D and present).
Rasterizer& framebuffer();

} // namespace crayon::soft
