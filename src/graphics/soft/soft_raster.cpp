#include "soft_raster.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>

namespace crayon::soft {

// ===========================================================================
// Texture
// ===========================================================================
static inline bool is_pow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

static inline uint32_t pack_rgba8(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return r | (g << 8) | (b << 16) | (a << 24);
}

bool Texture::create(const unsigned char* data, int width, int height, int channels, bool build_mips, bool smooth) {
    m_levels.clear();
    m_has_alpha = false;
    m_smooth = smooth;
    if (!data || width <= 0 || height <= 0) return false;

    Level l0;
    l0.w = width;
    l0.h = height;
    l0.px.resize(static_cast<size_t>(width) * height);
    for (size_t i = 0, n = l0.px.size(); i < n; ++i) {
        uint32_t r, g, b, a = 255;
        switch (channels) {
            case 1: r = g = b = data[i]; break;
            case 2: r = g = b = data[i * 2]; a = data[i * 2 + 1]; break;
            case 3: r = data[i * 3]; g = data[i * 3 + 1]; b = data[i * 3 + 2]; break;
            default: r = data[i * 4]; g = data[i * 4 + 1]; b = data[i * 4 + 2]; a = data[i * 4 + 3]; break;
        }
        if (a < 255) m_has_alpha = true;
        l0.px[i] = pack_rgba8(r, g, b, a);
    }
    l0.pow2 = is_pow2(width) && is_pow2(height);
    l0.wmask = width - 1;
    l0.hmask = height - 1;
    m_levels.push_back(std::move(l0));

    if (build_mips) generate_mips();
    return true;
}

void Texture::generate_mips() {
    m_levels.resize(1);
    while ((m_levels.back().w > 1 || m_levels.back().h > 1)) {
        const Level& s = m_levels.back();
        Level d;
        d.w = std::max(1, s.w / 2);
        d.h = std::max(1, s.h / 2);
        d.px.resize(static_cast<size_t>(d.w) * d.h);
        for (int y = 0; y < d.h; ++y) {
            int y0 = std::min(y * 2, s.h - 1), y1 = std::min(y * 2 + 1, s.h - 1);
            for (int x = 0; x < d.w; ++x) {
                int x0 = std::min(x * 2, s.w - 1), x1 = std::min(x * 2 + 1, s.w - 1);
                uint32_t p[4] = {s.px[y0 * s.w + x0], s.px[y0 * s.w + x1], s.px[y1 * s.w + x0], s.px[y1 * s.w + x1]};
                uint32_t sum[4] = {0, 0, 0, 0};
                for (uint32_t q : p) {
                    sum[0] += q & 255; sum[1] += (q >> 8) & 255; sum[2] += (q >> 16) & 255; sum[3] += q >> 24;
                }
                d.px[y * d.w + x] = pack_rgba8((sum[0] + 2) >> 2, (sum[1] + 2) >> 2, (sum[2] + 2) >> 2, (sum[3] + 2) >> 2);
            }
        }
        d.pow2 = is_pow2(d.w) && is_pow2(d.h);
        d.wmask = d.w - 1;
        d.hmask = d.h - 1;
        m_levels.push_back(std::move(d));
    }
}

void Texture::set_mipmaps(bool enabled) {
    if (m_levels.empty()) return;
    m_levels.resize(1);
    if (enabled) generate_mips();
}

bool Texture::update(const unsigned char* rgba, int width, int height) {
    if (m_levels.empty() || !rgba || width != m_levels[0].w || height != m_levels[0].h) return false;
    const bool had_mips = m_levels.size() > 1;
    Level& l0 = m_levels[0];
    m_has_alpha = false;
    for (size_t i = 0, n = l0.px.size(); i < n; ++i) {
        const unsigned char* p = rgba + i * 4;
        if (p[3] < 255) m_has_alpha = true;
        l0.px[i] = pack_rgba8(p[0], p[1], p[2], p[3]);
    }
    if (had_mips) { m_levels.resize(1); generate_mips(); }
    return true;
}

void Texture::create_solid(uint32_t rgba) {
    m_levels.clear();
    Level l;
    l.w = l.h = 1;
    l.wmask = l.hmask = 0;
    l.pow2 = true;
    l.px.assign(1, rgba);
    m_levels.push_back(std::move(l));
    m_has_alpha = (rgba >> 24) < 255;
    m_smooth = false;
}

static inline int wrap_coord(int v, int n, int mode) {
    switch (mode) {
        case 1: return v < 0 ? 0 : (v >= n ? n - 1 : v);                      // clamp to edge
        case 2: { int p = n * 2; int m = v % p; if (m < 0) m += p; return m < n ? m : p - 1 - m; } // mirrored repeat
        default: { int m = v % n; return m < 0 ? m + n : m; }                // repeat
    }
}

static inline uint32_t fetch_texel(const Texture::Level& L, int x, int y, int wrap) {
    if (wrap == 0 && L.pow2) {
        x &= L.wmask;
        y &= L.hmask;
    } else {
        x = wrap_coord(x, L.w, wrap);
        y = wrap_coord(y, L.h, wrap);
    }
    return L.px[static_cast<size_t>(y) * L.w + x];
}

// ===========================================================================
// Texture registry + shared framebuffer
// ===========================================================================
namespace {
std::vector<std::shared_ptr<Texture>>& tex_table() { static std::vector<std::shared_ptr<Texture>> t; return t; }
std::vector<uint32_t>& tex_free() { static std::vector<uint32_t> f; return f; }
}

uint32_t TextureRegistry::add(std::shared_ptr<Texture> tex) {
    auto& t = tex_table();
    auto& f = tex_free();
    if (!f.empty()) {
        uint32_t id = f.back();
        f.pop_back();
        t[id - 1] = std::move(tex);
        return id;
    }
    t.push_back(std::move(tex));
    return static_cast<uint32_t>(t.size());
}

const Texture* TextureRegistry::get(uint32_t id) {
    auto& t = tex_table();
    if (id == 0 || id > t.size()) return nullptr;
    return t[id - 1].get();
}

void TextureRegistry::remove(uint32_t id) {
    auto& t = tex_table();
    if (id == 0 || id > t.size() || !t[id - 1]) return;
    t[id - 1].reset();
    tex_free().push_back(id);
}

Rasterizer& framebuffer() {
    static Rasterizer fb;
    return fb;
}

// ===========================================================================
// Rasterizer setup
// ===========================================================================
void Rasterizer::resize(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == m_w && height == m_h) return;
    m_w = width;
    m_h = height;
    m_color.assign(static_cast<size_t>(m_w) * m_h, 0u);
    m_depth.assign(static_cast<size_t>(m_w) * m_h, 2.0f);
    m_mask.assign(static_cast<size_t>(m_w) * m_h, 0);
}

void Rasterizer::clear_mask() { std::fill(m_mask.begin(), m_mask.end(), 0); }

void Rasterizer::clear(const glm::vec4& c, bool clear_depth) {
    auto q = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    uint32_t packed = pack_rgba8(q(c.r), q(c.g), q(c.b), q(c.a));
    std::fill(m_color.begin(), m_color.end(), packed);
    if (clear_depth) std::fill(m_depth.begin(), m_depth.end(), 2.0f);
}

void Rasterizer::begin(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cam_position) {
    m_view = view;
    m_proj = proj;
    m_viewproj = proj * view;
    m_cam = cam_position;
}

// ===========================================================================
// Lighting (mirrors the GL fragment shader)
// ===========================================================================
static glm::vec3 light_at(const Settings& s, const glm::vec3& L, const glm::vec3& P, const glm::vec3& N) {
    float diff = std::max(glm::dot(N, L), 0.0f);
    glm::vec3 lighting = s.ambient + s.light_color * diff;

    for (const auto& pl : s.points) {
        if (!pl.enabled) continue;
        glm::vec3 d = pl.pos - P;
        float dist = glm::length(d);
        if (dist < pl.radius && dist > 0.0001f) {
            float nd = std::max(glm::dot(N, d / dist), 0.0f);
            float at = std::clamp(1.0f - dist / pl.radius, 0.0f, 1.0f);
            lighting += pl.color * (nd * at * at * pl.intensity);
        }
    }
    for (const auto& sl : s.spots) {
        if (!sl.enabled) continue;
        glm::vec3 d = sl.pos - P;
        float dist = glm::length(d);
        if (dist < sl.radius && dist > 0.0001f) {
            glm::vec3 sL = d / dist;
            float theta = glm::dot(sL, glm::normalize(-sl.dir));
            float eps = sl.inner_cos - sl.outer_cos;
            float spot = std::clamp((theta - sl.outer_cos) / std::max(eps, 0.0001f), 0.0f, 1.0f);
            if (spot > 0.0f) {
                float nd = std::max(glm::dot(N, sL), 0.0f);
                float at = std::clamp(1.0f - dist / sl.radius, 0.0f, 1.0f);
                lighting += sl.color * (nd * at * at * sl.intensity * spot);
            }
        }
    }
    return lighting;
}

static inline uint8_t outcode(const glm::vec4& c) {
    return static_cast<uint8_t>((c.x < -c.w) | ((c.x > c.w) << 1) | ((c.y < -c.w) << 2) |
                                ((c.y > c.w) << 3) | ((c.z < -c.w) << 4) | ((c.z > c.w) << 5));
}

// ===========================================================================
// Frustum test
// ===========================================================================
bool Rasterizer::aabb_visible(const glm::vec3& mn, const glm::vec3& mx, const glm::mat4& model) const {
    glm::mat4 mvp = m_viewproj * model;
    uint8_t all = 0x3F;
    for (int i = 0; i < 8; ++i) {
        glm::vec4 p(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z, 1.0f);
        all &= outcode(mvp * p);
        if (!all) return true;
    }
    return all == 0;
}

// ===========================================================================
// Clipping
// ===========================================================================
static inline float plane_dist(const glm::vec4& c, int plane) {
    switch (plane) {
        case 0: return c.w + c.x;
        case 1: return c.w - c.x;
        case 2: return c.w + c.y;
        case 3: return c.w - c.y;
        case 4: return c.w + c.z;
        default: return c.w - c.z;
    }
}

template <typename SVt>
static inline SVt lerp_sv(const SVt& a, const SVt& b, float t) {
    SVt r;
    r.clip = a.clip + (b.clip - a.clip) * t;
    r.uv = a.uv + (b.uv - a.uv) * t;
    r.col = a.col + (b.col - a.col) * t;
    return r;
}

template <typename SVt>
static int clip_against(const SVt* in, int n, SVt* out, int plane) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const SVt& cur = in[i];
        const SVt& nxt = in[(i + 1) % n];
        float dc = plane_dist(cur.clip, plane);
        float dn = plane_dist(nxt.clip, plane);
        if (dc >= 0.0f) out[m++] = cur;
        if ((dc >= 0.0f) != (dn >= 0.0f)) {
            float t = dc / (dc - dn);
            out[m++] = lerp_sv(cur, nxt, t);
        }
    }
    return m;
}

// ===========================================================================
// Draw (vertex stage + triangle setup)
// ===========================================================================
void Rasterizer::draw(const Vertex* verts, size_t vert_count,
                      const uint32_t* indices, size_t index_count,
                      const glm::mat4& model, const Texture* tex, const glm::vec4& tint) {
    if (m_w == 0 || vert_count == 0 || index_count < 3) return;
    stats.triangles_in += index_count / 3;

    const Settings& S = settings;
    const glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(model)));
    const glm::vec3 L = glm::normalize(-S.light_dir);

    if (m_pv.size() < vert_count) m_pv.resize(vert_count);

    // ---- vertex stage: transform, light, fog, outcode ----
    for (size_t i = 0; i < vert_count; ++i) {
        const Vertex& v = verts[i];
        PV& o = m_pv[i];
        glm::vec4 w = model * glm::vec4(v.position, 1.0f);
        o.wpos = glm::vec3(w);
        o.clip = m_viewproj * w;
        o.uv = v.uv;
        o.base = v.color * tint;
        if (S.shading == Shading::Gouraud) {
            glm::vec3 N = glm::normalize(nm * v.normal);
            o.light = light_at(S, L, o.wpos, N);
        } else {
            o.light = glm::vec3(1.0f);
        }
        o.oc = outcode(o.clip);
    }

    // ---- triangle stage ----
    SV poly_a[12], poly_b[12];
    for (size_t t = 0; t + 2 < index_count; t += 3) {
        uint32_t i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
        if (i0 >= vert_count || i1 >= vert_count || i2 >= vert_count) continue;
        const PV& p0 = m_pv[i0];
        const PV& p1 = m_pv[i1];
        const PV& p2 = m_pv[i2];
        if (p0.oc & p1.oc & p2.oc) continue; // trivially outside one plane

        glm::vec3 flat_light(1.0f);
        if (S.shading == Shading::Flat) {
            glm::vec3 fn = glm::cross(p1.wpos - p0.wpos, p2.wpos - p0.wpos);
            float len = glm::length(fn);
            glm::vec3 centroid = (p0.wpos + p1.wpos + p2.wpos) * (1.0f / 3.0f);
            if (len > 1e-12f) {
                fn /= len;
                if (glm::dot(fn, m_cam - centroid) < 0.0f) fn = -fn; // face the viewer (like dFdx x dFdy)
            } else {
                fn = glm::vec3(0, 1, 0);
            }
            flat_light = light_at(S, L, centroid, fn);
        }

        const PV* pp[3] = {&p0, &p1, &p2};
        SV s[3];
        for (int k = 0; k < 3; ++k) {
            const glm::vec3 lit = (S.shading == Shading::Flat) ? flat_light : pp[k]->light;
            s[k].clip = pp[k]->clip;
            s[k].uv = pp[k]->uv;
            s[k].col = glm::vec4(glm::vec3(pp[k]->base) * lit, pp[k]->base.a);
        }

        if ((p0.oc | p1.oc | p2.oc) == 0) {
            raster_polygon(s, 3, tex);
        } else {
            SV* in = poly_a;
            SV* out = poly_b;
            int n = 3;
            std::memcpy(in, s, sizeof(SV) * 3);
            for (int plane = 0; plane < 6 && n >= 3; ++plane) {
                n = clip_against(in, n, out, plane);
                std::swap(in, out);
            }
            if (n >= 3) raster_polygon(in, n, tex);
        }
    }
}

// ===========================================================================
// Polygon -> screen space -> triangle fan
// ===========================================================================
void Rasterizer::raster_polygon(const SV* poly, int n, const Texture* tex) {
    SPoint pts[12];
    const Settings& S = settings;
    const float hw = m_w * 0.5f, hh = m_h * 0.5f;
    for (int i = 0; i < n; ++i) {
        const glm::vec4& c = poly[i].clip;
        float iw = 1.0f / c.w;
        float nx = c.x * iw, ny = c.y * iw, nz = c.z * iw;
        if (S.jitter && S.jitter_res.x > 0.0f && S.jitter_res.y > 0.0f) {
            nx = std::floor(nx * S.jitter_res.x * 0.5f + 0.5f) / (S.jitter_res.x * 0.5f);
            ny = std::floor(ny * S.jitter_res.y * 0.5f + 0.5f) / (S.jitter_res.y * 0.5f);
        }
        pts[i].x = (nx + 1.0f) * hw;
        pts[i].y = (1.0f - ny) * hh;
        pts[i].z = nz;
        pts[i].iw = iw;
        pts[i].uv = poly[i].uv;
        pts[i].col = poly[i].col;
    }
    for (int i = 1; i + 1 < n; ++i) raster_triangle(pts[0], pts[i], pts[i + 1], tex);
}

// ===========================================================================
// Span rasterizer
// ===========================================================================
static const float kBayer4[16] = {
     0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f,
    12.0f / 16.0f,  4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f,
     3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f,
    15.0f / 16.0f,  7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f};

namespace {
struct Grad { float v0, dx, dy; };
inline Grad make_grad(float fa, float fb, float fc, const float* ax, const float* ay,
                      float bax, float bay, float cax, float cay, float inv_area) {
    (void)ax; (void)ay;
    Grad g;
    g.v0 = fa;
    g.dx = ((fb - fa) * cay - (fc - fa) * bay) * inv_area;
    g.dy = ((fc - fa) * bax - (fb - fa) * cax) * inv_area;
    return g;
}
} // namespace

void Rasterizer::raster_triangle(const SPoint& a, const SPoint& b, const SPoint& c, const Texture* tex) {
    const Settings& S = settings;

    float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    // screen y points down, so CCW (front) faces have negative area
    if (S.cull == Cull::Back && area >= 0.0f) return;
    if (S.cull == Cull::Front && area <= 0.0f) return;
    if (std::fabs(area) < 1e-6f) return;

    float min_y = std::min({a.y, b.y, c.y}), max_y = std::max({a.y, b.y, c.y});
    float min_x = std::min({a.x, b.x, c.x}), max_x = std::max({a.x, b.x, c.x});
    if (max_x < 0.0f || min_x > float(m_w) || max_y < 0.0f || min_y > float(m_h)) return;
    int y_start = std::max(0, int(std::ceil(min_y - 0.5f)));
    int y_end = std::min(m_h, int(std::ceil(max_y - 0.5f)));
    if (y_start >= y_end) return;

    stats.triangles_rasterized++;

    const float inv_area = 1.0f / area;
    const float bax = b.x - a.x, bay = b.y - a.y, cax = c.x - a.x, cay = c.y - a.y;
    auto G = [&](float fa, float fb, float fc) {
        return make_grad(fa, fb, fc, nullptr, nullptr, bax, bay, cax, cay, inv_area);
    };

    Grad gz = G(a.z, b.z, c.z);
    Grad gr = G(a.col.r, b.col.r, c.col.r), gg = G(a.col.g, b.col.g, c.col.g);
    Grad gb = G(a.col.b, b.col.b, c.col.b), ga = G(a.col.a, b.col.a, c.col.a);
    Grad giw = G(a.iw, b.iw, c.iw);
    Grad guw = G(a.uv.x * a.iw, b.uv.x * b.iw, c.uv.x * c.iw);
    Grad gvw = G(a.uv.y * a.iw, b.uv.y * b.iw, c.uv.y * c.iw);
    Grad gu = G(a.uv.x, b.uv.x, c.uv.x);
    Grad gv = G(a.uv.y, b.uv.y, c.uv.y);

    // ---- texture / LOD setup ----
    const Texture::Level* base_level = nullptr;
    int nlevels = 1;
    float lod_ref = 0.0f, w_ref = 1.0f;
    if (tex && tex->level_count() > 0) {
        base_level = &tex->level(0);
        nlevels = tex->level_count();
        if (S.mipmaps && nlevels > 1) {
            float uv_area = std::fabs((b.uv.x - a.uv.x) * (c.uv.y - a.uv.y) -
                                      (c.uv.x - a.uv.x) * (b.uv.y - a.uv.y)) * 0.5f;
            float texel_area = uv_area * float(base_level->w) * float(base_level->h);
            float ratio = std::max(texel_area / (std::fabs(area) * 0.5f), 1e-9f);
            lod_ref = 0.5f * std::log2(ratio);
            w_ref = 3.0f / std::max(a.iw + b.iw + c.iw, 1e-12f);
        } else {
            nlevels = 1;
        }
    }

    const SPoint* P[3] = {&a, &b, &c};
    struct Edge { float x0, y0, y1, slope; } edges[3];
    int ecount = 0;
    for (int i = 0; i < 3; ++i) {
        const SPoint* p = P[i];
        const SPoint* q = P[(i + 1) % 3];
        // canonical order (top-to-bottom, then left-to-right) so shared edges produce
        // bit-identical intersections in neighbouring triangles (no cracks / double hits)
        if (p->y > q->y || (p->y == q->y && p->x > q->x)) std::swap(p, q);
        if (p->y == q->y) continue;
        edges[ecount++] = {p->x, p->y, q->y, (q->x - p->x) / (q->y - p->y)};
    }

    const int wrap = tex ? static_cast<int>(tex->wrap()) : 0;
    const bool bilin = S.bilinear || (tex && tex->smooth());
    const bool depth_test = S.depth_test, depth_write = S.depth_write, blend_on = S.blend;
    const bool use_affine = S.affine;
    const bool dither = S.dither;
    const float dl = std::max(S.dither_levels - 1.0f, 1.0f);
    const bool fog_on = S.fog;
    const float cutoff = S.alpha_cutoff;
    const glm::vec3 fogc = S.fog_color;
    const float fog_end = S.fog_end;
    const float fog_inv = 1.0f / std::max(S.fog_end - S.fog_start, 0.0001f);

    uint32_t* const color = m_color.data();
    float* const depth = m_depth.data();

    for (int y = y_start; y < y_end; ++y) {
        const float yc = float(y) + 0.5f;
        float xl = 1e30f, xr = -1e30f;
        for (int e = 0; e < ecount; ++e) {
            if (yc >= edges[e].y0 && yc < edges[e].y1) {
                float x = edges[e].x0 + (yc - edges[e].y0) * edges[e].slope;
                xl = std::min(xl, x);
                xr = std::max(xr, x);
            }
        }
        if (xl > xr) continue;
        int x0 = std::max(0, int(std::ceil(xl - 0.5f)));
        int x1 = std::min(m_w, int(std::ceil(xr - 0.5f)));
        if (x0 >= x1) continue;

        const float dxs = (float(x0) + 0.5f) - a.x;
        const float dys = yc - a.y;
        auto at = [&](const Grad& g) { return g.v0 + g.dx * dxs + g.dy * dys; };

        float z = at(gz);
        float cr = at(gr), cg = at(gg), cb = at(gb), ca = at(ga);
        float iw0 = at(giw), uw0 = at(guw), vw0 = at(gvw);
        float u_aff = at(gu), v_aff = at(gv);

        uint32_t* row = color + size_t(y) * m_w;
        float* drow = depth + size_t(y) * m_w;

        const Texture::Level* lvl = base_level;
        float fw = base_level ? float(base_level->w) : 1.0f;
        float fh = base_level ? float(base_level->h) : 1.0f;
        // per-chunk perspective state
        float pu = 0, pv = 0, pdu = 0, pdv = 0;
        int chunk_end = x0;

        for (int x = x0; x < x1; ++x) {
            float u = 0.0f, v = 0.0f;
            if (base_level) {
                if (use_affine) {
                    u = u_aff + gu.dx * float(x - x0);
                    v = v_aff + gv.dx * float(x - x0);
                    if (x == x0) {
                        float lod = lod_ref;
                        int L = std::clamp(int(std::floor(lod + 0.5f)), 0, nlevels - 1);
                        lvl = &tex->level(L);
                        fw = float(lvl->w); fh = float(lvl->h);
                    }
                } else {
                    if (x == chunk_end) {
                        // perspective-correct every 8 pixels, linear in between
                        int ce = std::min(x + 8, x1);
                        float t0 = float(x - x0), t1 = float(ce - x0);
                        float iw_a = iw0 + giw.dx * t0, iw_b = iw0 + giw.dx * (t1);
                        float ra = 1.0f / iw_a, rb = 1.0f / iw_b;
                        float u_a = (uw0 + guw.dx * t0) * ra, u_b = (uw0 + guw.dx * t1) * rb;
                        float v_a = (vw0 + gvw.dx * t0) * ra, v_b = (vw0 + gvw.dx * t1) * rb;
                        float inv_n = 1.0f / float(ce - x);
                        pu = u_a; pv = v_a;
                        pdu = (u_b - u_a) * inv_n;
                        pdv = (v_b - v_a) * inv_n;
                        chunk_end = ce;
                        if (nlevels > 1) {
                            float lod = lod_ref + std::log2(std::max(ra / w_ref, 1e-6f));
                            int L = std::clamp(int(std::floor(lod + 0.5f)), 0, nlevels - 1);
                            lvl = &tex->level(L);
                            fw = float(lvl->w); fh = float(lvl->h);
                        }
                    }
                    u = pu; v = pv;
                    pu += pdu; pv += pdv;
                }
            }

            // depth test (LEQUAL)
            if (!depth_test || z <= drow[x]) {
                float r = cr, g = cg, bl = cb, al = ca;
                if (lvl) {
                    uint32_t texel;
                    if (bilin) {
                        float tx = u * fw - 0.5f, ty = v * fh - 0.5f;
                        float fx = std::floor(tx), fy = std::floor(ty);
                        int ix = int(fx), iy = int(fy);
                        float wx = tx - fx, wy = ty - fy;
                        uint32_t t00 = fetch_texel(*lvl, ix, iy, wrap), t10 = fetch_texel(*lvl, ix + 1, iy, wrap);
                        uint32_t t01 = fetch_texel(*lvl, ix, iy + 1, wrap), t11 = fetch_texel(*lvl, ix + 1, iy + 1, wrap);
                        auto ch = [&](int sh) {
                            float a0 = float((t00 >> sh) & 255), a1 = float((t10 >> sh) & 255);
                            float b0 = float((t01 >> sh) & 255), b1 = float((t11 >> sh) & 255);
                            return (a0 + (a1 - a0) * wx) + ((b0 + (b1 - b0) * wx) - (a0 + (a1 - a0) * wx)) * wy;
                        };
                        r *= ch(0) * (1.0f / 255.0f); g *= ch(8) * (1.0f / 255.0f);
                        bl *= ch(16) * (1.0f / 255.0f); al *= ch(24) * (1.0f / 255.0f);
                    } else {
                        texel = fetch_texel(*lvl, int(std::floor(u * fw)), int(std::floor(v * fh)), wrap);
                        r *= float(texel & 255) * (1.0f / 255.0f);
                        g *= float((texel >> 8) & 255) * (1.0f / 255.0f);
                        bl *= float((texel >> 16) & 255) * (1.0f / 255.0f);
                        al *= float(texel >> 24) * (1.0f / 255.0f);
                    }
                }
                if (al >= cutoff) {
                    if (fog_on) {
                        // depth fog from the perspective-correct 1/w (view-space depth)
                        float d = 1.0f / (iw0 + giw.dx * float(x - x0));
                        float f = std::clamp((fog_end - d) * fog_inv, 0.0f, 1.0f);
                        r = fogc.r + (r - fogc.r) * f;
                        g = fogc.g + (g - fogc.g) * f;
                        bl = fogc.b + (bl - fogc.b) * f;
                    }
                    if (dither) {
                        float bay = (kBayer4[((y & 3) << 2) | (x & 3)]) + (0.5f / 16.0f);
                        r = std::floor(std::clamp(r, 0.0f, 1.0f) * dl + bay) / dl;
                        g = std::floor(std::clamp(g, 0.0f, 1.0f) * dl + bay) / dl;
                        bl = std::floor(std::clamp(bl, 0.0f, 1.0f) * dl + bay) / dl;
                    }
                    auto q8 = [](float v) {
                        return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
                    };
                    if (blend_on && al < 1.0f) {
                        uint32_t d = row[x];
                        float a = std::clamp(al, 0.0f, 1.0f), ia = 1.0f - a;
                        r = r * a + float(d & 255) * (1.0f / 255.0f) * ia;
                        g = g * a + float((d >> 8) & 255) * (1.0f / 255.0f) * ia;
                        bl = bl * a + float((d >> 16) & 255) * (1.0f / 255.0f) * ia;
                    }
                    row[x] = pack_rgba8(q8(r), q8(g), q8(bl), 255u);
                    if (depth_write) drow[x] = z;
                    stats.pixels_shaded++;
                }
            }

            z += gz.dx;
            cr += gr.dx; cg += gg.dx; cb += gb.dx; ca += ga.dx;
        }
    }
}

// ===========================================================================
// Lines (debug draw)
// ===========================================================================
void Rasterizer::draw_line(const glm::vec3& pa, const glm::vec3& pb, const glm::vec4& color) {
    if (m_w == 0) return;
    glm::vec4 ca = m_viewproj * glm::vec4(pa, 1.0f);
    glm::vec4 cb = m_viewproj * glm::vec4(pb, 1.0f);
    // clip against the near plane only; x/y are clamped by the pixel loop
    const float eps = 1e-4f;
    float da = ca.z + ca.w, db = cb.z + cb.w;
    if (da < eps && db < eps) return;
    if (da < eps) { float t = (eps - da) / (db - da); ca = ca + (cb - ca) * t; }
    else if (db < eps) { float t = (eps - db) / (da - db); cb = cb + (ca - cb) * t; }
    if (ca.w <= 0.0f || cb.w <= 0.0f) return;

    auto to_screen = [&](const glm::vec4& c, float& sx, float& sy, float& sz) {
        float iw = 1.0f / c.w;
        sx = (c.x * iw + 1.0f) * 0.5f * m_w;
        sy = (1.0f - c.y * iw) * 0.5f * m_h;
        sz = c.z * iw;
    };
    float x0, y0, z0, x1, y1, z1;
    to_screen(ca, x0, y0, z0);
    to_screen(cb, x1, y1, z1);

    float dx = x1 - x0, dy = y1 - y0;
    int steps = int(std::ceil(std::max(std::fabs(dx), std::fabs(dy))));
    steps = std::min(steps, (m_w + m_h) * 4); // guard against absurd off-screen lines
    if (steps < 1) steps = 1;
    auto q8 = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    uint32_t packed = pack_rgba8(q8(color.r), q8(color.g), q8(color.b), 255u);
    for (int i = 0; i <= steps; ++i) {
        float t = float(i) / float(steps);
        int px = int(x0 + dx * t), py = int(y0 + dy * t);
        if (px < 0 || py < 0 || px >= m_w || py >= m_h) continue;
        float z = z0 + (z1 - z0) * t - 1e-4f; // tiny bias so lines sit on top of coplanar surfaces
        size_t idx = size_t(py) * m_w + px;
        if (z <= m_depth[idx]) {
            m_color[idx] = packed;
            m_depth[idx] = z;
        }
    }
}

// ===========================================================================
// 2D batch rasterization
// ===========================================================================
namespace {
inline int32_t to_fx(float v) {
    v = std::clamp(v, -16000.0f, 16000.0f);
    return static_cast<int32_t>(std::lround(v * 256.0f));
}
// Odd predicate (p(d) + p(-d) == 1) -> consistent tie-break on shared edges (watertight).
inline bool tie_in(int32_t dx, int32_t dy) { return dy > 0 || (dy == 0 && dx > 0); }
}

void Rasterizer::draw_2d(const Vertex2D* v, size_t count, const glm::mat4& pv, const Texture* tex,
                         Blend2D mode, const IRect* sc) {
    if (m_w == 0 || count < 4) return;
    int cx0 = 0, cy0 = 0, cx1 = m_w, cy1 = m_h;
    if (sc) {
        cx0 = std::max(0, sc->x); cy0 = std::max(0, sc->y);
        cx1 = std::min(m_w, sc->x + sc->w); cy1 = std::min(m_h, sc->y + sc->h);
    }
    if (cx0 >= cx1 || cy0 >= cy1) return;

    const MaskMode mask_mode = m_mask_mode;
    const Texture::Level* lvl = (tex && tex->level_count() > 0) ? &tex->level(0) : nullptr;
    const bool smooth = tex && tex->smooth();
    const int wrap = tex ? static_cast<int>(tex->wrap()) : 0;
    const float fw = lvl ? float(lvl->w) : 1.0f, fh = lvl ? float(lvl->h) : 1.0f;
    const float hw = m_w * 0.5f, hh = m_h * 0.5f;

    auto q8 = [](float x) { return static_cast<uint32_t>(std::clamp(x, 0.0f, 1.0f) * 255.0f + 0.5f); };

    struct P { int32_t x, y; glm::vec2 uv; glm::vec4 col; };

    auto tri = [&](P a, P b, P c) {
        int64_t area = int64_t(b.x - a.x) * (c.y - a.y) - int64_t(c.x - a.x) * (b.y - a.y);
        if (area == 0) return;
        if (area < 0) { std::swap(b, c); area = -area; }

        int32_t minx = std::min({a.x, b.x, c.x}), maxx = std::max({a.x, b.x, c.x});
        int32_t miny = std::min({a.y, b.y, c.y}), maxy = std::max({a.y, b.y, c.y});
        int x0 = std::max(cx0, int(minx >> 8)), x1 = std::min(cx1 - 1, int(maxx >> 8));
        int y0 = std::max(cy0, int(miny >> 8)), y1 = std::min(cy1 - 1, int(maxy >> 8));
        if (x0 > x1 || y0 > y1) return;

        // edge i opposes vertex i:  E0 = edge(b,c), E1 = edge(c,a), E2 = edge(a,b)
        const int32_t ex[3] = {c.x - b.x, a.x - c.x, b.x - a.x};
        const int32_t ey[3] = {c.y - b.y, a.y - c.y, b.y - a.y};
        const int32_t ox[3] = {b.x, c.x, a.x};
        const int32_t oy[3] = {b.y, c.y, a.y};
        const bool top[3] = {tie_in(ex[0], ey[0]), tie_in(ex[1], ey[1]), tie_in(ex[2], ey[2])};
        const float inv_area = 1.0f / float(area);

        for (int y = y0; y <= y1; ++y) {
            const int32_t py = y * 256 + 128;
            int64_t e[3];
            const int32_t px0 = x0 * 256 + 128;
            for (int i = 0; i < 3; ++i)
                e[i] = int64_t(ex[i]) * (py - oy[i]) - int64_t(ey[i]) * (px0 - ox[i]);
            uint32_t* row = m_color.data() + size_t(y) * m_w;
            for (int x = x0; x <= x1; ++x) {
                bool in = true;
                for (int i = 0; i < 3; ++i) {
                    if (e[i] < 0 || (e[i] == 0 && !top[i])) { in = false; break; }
                }
                if (in && mask_mode != MaskMode::Off) {
                    uint8_t& m = m_mask[size_t(y) * m_w + x];
                    if (mask_mode == MaskMode::Write) { m = 1; in = false; }              // mark, don't paint
                    else if ((mask_mode == MaskMode::Test) != (m != 0)) in = false;       // outside the allowed region
                }
                if (in) {
                    float l0 = float(e[0]) * inv_area, l1 = float(e[1]) * inv_area, l2 = float(e[2]) * inv_area;
                    glm::vec4 col = a.col * l0 + b.col * l1 + c.col * l2;
                    if (lvl) {
                        glm::vec2 uv = a.uv * l0 + b.uv * l1 + c.uv * l2;
                        float tr, tg, tb, ta;
                        if (smooth) {
                            float tx = uv.x * fw - 0.5f, ty = uv.y * fh - 0.5f;
                            float fx = std::floor(tx), fy = std::floor(ty);
                            int ix = int(fx), iy = int(fy);
                            float wx = tx - fx, wy = ty - fy;
                            uint32_t t00 = fetch_texel(*lvl, ix, iy, wrap), t10 = fetch_texel(*lvl, ix + 1, iy, wrap);
                            uint32_t t01 = fetch_texel(*lvl, ix, iy + 1, wrap), t11 = fetch_texel(*lvl, ix + 1, iy + 1, wrap);
                            auto ch = [&](int sh) {
                                float a0 = float((t00 >> sh) & 255), a1 = float((t10 >> sh) & 255);
                                float b0 = float((t01 >> sh) & 255), b1 = float((t11 >> sh) & 255);
                                float top_v = a0 + (a1 - a0) * wx, bot_v = b0 + (b1 - b0) * wx;
                                return (top_v + (bot_v - top_v) * wy) * (1.0f / 255.0f);
                            };
                            tr = ch(0); tg = ch(8); tb = ch(16); ta = ch(24);
                        } else {
                            uint32_t t = fetch_texel(*lvl, int(std::floor(uv.x * fw)), int(std::floor(uv.y * fh)), wrap);
                            tr = float(t & 255) * (1.0f / 255.0f); tg = float((t >> 8) & 255) * (1.0f / 255.0f);
                            tb = float((t >> 16) & 255) * (1.0f / 255.0f); ta = float(t >> 24) * (1.0f / 255.0f);
                        }
                        col.r *= tr; col.g *= tg; col.b *= tb; col.a *= ta;
                    }
                    uint32_t d = row[x];
                    float dr = float(d & 255) * (1.0f / 255.0f), dg = float((d >> 8) & 255) * (1.0f / 255.0f),
                          db = float((d >> 16) & 255) * (1.0f / 255.0f);
                    float orr, og, ob;
                    switch (mode) {
                        case Blend2D::Alpha: {
                            float al = std::clamp(col.a, 0.0f, 1.0f);
                            orr = col.r * al + dr * (1.0f - al); og = col.g * al + dg * (1.0f - al); ob = col.b * al + db * (1.0f - al);
                            break;
                        }
                        case Blend2D::Additive: {
                            float al = std::clamp(col.a, 0.0f, 1.0f);
                            orr = dr + col.r * al; og = dg + col.g * al; ob = db + col.b * al;
                            break;
                        }
                        case Blend2D::Multiply:
                            orr = col.r * dr; og = col.g * dg; ob = col.b * db;
                            break;
                        default:
                            orr = col.r; og = col.g; ob = col.b;
                            break;
                    }
                    row[x] = pack_rgba8(q8(orr), q8(og), q8(ob), 255u);
                }
                for (int i = 0; i < 3; ++i) e[i] -= int64_t(ey[i]) * 256;
            }
        }
    };

    for (size_t q = 0; q + 3 < count; q += 4) {
        P p[4];
        bool ok = true;
        for (int i = 0; i < 4; ++i) {
            const Vertex2D& vv = v[q + i];
            glm::vec4 c = pv * glm::vec4(vv.position, 0.0f, 1.0f);
            if (c.w == 0.0f) { ok = false; break; }
            float iw = 1.0f / c.w;
            p[i].x = to_fx((c.x * iw + 1.0f) * hw);
            p[i].y = to_fx((1.0f - c.y * iw) * hh);
            p[i].uv = vv.uv;
            p[i].col = vv.color;
        }
        if (!ok) continue;
        tri(p[0], p[1], p[2]);
        tri(p[2], p[3], p[0]);
    }
}

// ===========================================================================
// Post effects (CPU equivalents of the GL post shader's dither / scanlines / vignette)
// ===========================================================================
void Rasterizer::post_process(const PostOptions& o) {
    if (!o.any() || m_w == 0) return;
    const float dl = std::max(o.dither_levels - 1.0f, 1.0f);
    std::vector<float> vx(m_w), vy(m_h);
    for (int x = 0; x < m_w; ++x) { float n = (float(x) + 0.5f) / m_w * 2.0f - 1.0f; vx[x] = n * n; }
    for (int y = 0; y < m_h; ++y) { float n = (float(y) + 0.5f) / m_h * 2.0f - 1.0f; vy[y] = n * n; }

    for (int y = 0; y < m_h; ++y) {
        uint32_t* row = m_color.data() + size_t(y) * m_w;
        const float scan = (o.scanlines && (y & 1)) ? (1.0f - o.scanline_strength) : 1.0f;
        for (int x = 0; x < m_w; ++x) {
            uint32_t p = row[x];
            float r = float(p & 255) * (1.0f / 255.0f), g = float((p >> 8) & 255) * (1.0f / 255.0f),
                  b = float((p >> 16) & 255) * (1.0f / 255.0f);
            float k = scan;
            if (o.vignette) k *= std::clamp(1.0f - o.vignette_strength * (vx[x] + vy[y]) * 0.5f, 0.0f, 1.0f);
            r *= k; g *= k; b *= k;
            if (o.dither) {
                float bay = kBayer4[((y & 3) << 2) | (x & 3)] + (0.5f / 16.0f);
                r = std::floor(std::clamp(r, 0.0f, 1.0f) * dl + bay) / dl;
                g = std::floor(std::clamp(g, 0.0f, 1.0f) * dl + bay) / dl;
                b = std::floor(std::clamp(b, 0.0f, 1.0f) * dl + bay) / dl;
            }
            auto q8 = [](float x2) { return static_cast<uint32_t>(std::clamp(x2, 0.0f, 1.0f) * 255.0f + 0.5f); };
            row[x] = pack_rgba8(q8(r), q8(g), q8(b), 255u);
        }
    }
}

void Rasterizer::draw_skybox(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& tint,
                             const std::function<glm::vec3(const glm::vec3&)>& sample) {
    if (m_w == 0) return;
    glm::mat4 rot_view = view;
    rot_view[3] = glm::vec4(0, 0, 0, 1);                       // rotation only: the sky follows the camera
    const glm::mat4 inv = glm::inverse(proj * rot_view);
    auto corner = [&](float nx, float ny) {
        glm::vec4 p = inv * glm::vec4(nx, ny, 1.0f, 1.0f);
        return glm::vec3(p) / p.w;
    };
    const glm::vec3 d00 = corner(-1, 1), d10 = corner(1, 1), d01 = corner(-1, -1), d11 = corner(1, -1);
    auto q = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    for (int y = 0; y < m_h; ++y) {
        const float fy = (float(y) + 0.5f) / float(m_h);
        const glm::vec3 left = d00 + (d01 - d00) * fy, right = d10 + (d11 - d10) * fy;
        uint32_t* row = m_color.data() + size_t(y) * m_w;
        const float* drow = m_depth.data() + size_t(y) * m_w;
        for (int x = 0; x < m_w; ++x) {
            if (drow[x] < 1.99f) continue;                      // geometry already here
            const float fx = (float(x) + 0.5f) / float(m_w);
            const glm::vec3 c = sample(left + (right - left) * fx) * tint;
            row[x] = q(c.r) | (q(c.g) << 8) | (q(c.b) << 16) | (255u << 24);
        }
    }
}

} // namespace crayon::soft
