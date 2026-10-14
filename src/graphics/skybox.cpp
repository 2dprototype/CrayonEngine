#include "skybox.hpp"
#include "backend.hpp"
#include "../core/log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace crayon {

Skybox::~Skybox() {
    if (m_gl_tex != 0 && !soft_backend()) glDeleteTextures(1, &m_gl_tex);
}

bool Skybox::load_faces(const std::array<std::shared_ptr<ImageData>, 6>& faces) {
    int size = 0;
    for (const auto& f : faces) {
        if (!f) return false;
        if (f->width() != f->height()) { CRAYON_LOG_WARN("Skybox faces must be square ({}x{})", f->width(), f->height()); return false; }
        if (size == 0) size = f->width();
        if (f->width() != size) { CRAYON_LOG_WARN("Skybox faces must all be the same size"); return false; }
    }
    for (int i = 0; i < 6; ++i) {
        m_faces[i].assign(faces[i]->data(), faces[i]->data() + size_t(size) * size * 4);
    }
    m_size = size;
    if (m_gl_tex != 0 && !soft_backend()) { glDeleteTextures(1, &m_gl_tex); }
    m_gl_tex = 0;
    return true;
}

bool Skybox::load_face_files(const std::array<std::string, 6>& paths) {
    std::array<std::shared_ptr<ImageData>, 6> faces;
    for (int i = 0; i < 6; ++i) {
        faces[i] = ImageData::load(paths[i]);
        if (!faces[i]) { CRAYON_LOG_WARN("Skybox: could not load '{}'", paths[i]); return false; }
    }
    return load_faces(faces);
}

// direction of the centre of face pixel (x,y) -- inverse of the cube-map face table
static glm::vec3 face_direction(int face, float u, float v) {
    const float sc = 2.0f * u - 1.0f, tc = 2.0f * v - 1.0f;
    switch (face) {
        case 0: return {1.0f, -tc, -sc};    // +X
        case 1: return {-1.0f, -tc, sc};    // -X
        case 2: return {sc, 1.0f, tc};      // +Y
        case 3: return {sc, -1.0f, -tc};    // -Y
        case 4: return {sc, -tc, 1.0f};     // +Z
        default: return {-sc, -tc, -1.0f};  // -Z
    }
}

bool Skybox::load_equirect(const ImageData& pano, int face_size) {
    if (pano.width() < 4 || pano.height() < 2) return false;
    if (face_size <= 0) face_size = std::clamp(pano.height() / 2, 16, 2048);
    const float pi = 3.14159265358979f;
    for (int f = 0; f < 6; ++f) {
        m_faces[f].assign(size_t(face_size) * face_size * 4, 255);
        for (int y = 0; y < face_size; ++y) {
            for (int x = 0; x < face_size; ++x) {
                glm::vec3 d = glm::normalize(face_direction(f, (x + 0.5f) / face_size, (y + 0.5f) / face_size));
                const float lon = std::atan2(d.x, -d.z);                         // 0 = looking down -Z (image centre)
                const float lat = std::asin(std::clamp(d.y, -1.0f, 1.0f));
                float px = (lon / (2.0f * pi) + 0.5f) * pano.width() - 0.5f;
                float py = (0.5f - lat / pi) * pano.height() - 0.5f;
                // bilinear sample with horizontal wrap
                int x0 = int(std::floor(px)), y0 = int(std::floor(py));
                float fx = px - x0, fy = py - y0;
                auto tex = [&](int tx, int ty, int c) {
                    tx = ((tx % pano.width()) + pano.width()) % pano.width();
                    ty = std::clamp(ty, 0, pano.height() - 1);
                    return float(pano.data()[(size_t(ty) * pano.width() + tx) * 4 + c]);
                };
                uint8_t* out = &m_faces[f][(size_t(y) * face_size + x) * 4];
                for (int c = 0; c < 3; ++c) {
                    float top = tex(x0, y0, c) * (1 - fx) + tex(x0 + 1, y0, c) * fx;
                    float bot = tex(x0, y0 + 1, c) * (1 - fx) + tex(x0 + 1, y0 + 1, c) * fx;
                    out[c] = uint8_t(top * (1 - fy) + bot * fy + 0.5f);
                }
            }
        }
    }
    m_size = face_size;
    if (m_gl_tex != 0 && !soft_backend()) { glDeleteTextures(1, &m_gl_tex); }
    m_gl_tex = 0;
    return true;
}

bool Skybox::load_equirect_file(const std::string& path, int face_size) {
    auto img = ImageData::load(path);
    if (!img) { CRAYON_LOG_WARN("Skybox: could not load '{}'", path); return false; }
    return load_equirect(*img, face_size);
}

glm::vec3 Skybox::sample(const glm::vec3& d) const {
    if (m_size == 0) return glm::vec3(0.0f);
    const float ax = std::fabs(d.x), ay = std::fabs(d.y), az = std::fabs(d.z);
    int face; float sc, tc, ma;
    if (ax >= ay && ax >= az) { ma = ax; if (d.x > 0) { face = 0; sc = -d.z; tc = -d.y; } else { face = 1; sc = d.z; tc = -d.y; } }
    else if (ay >= az)        { ma = ay; if (d.y > 0) { face = 2; sc = d.x; tc = d.z; }  else { face = 3; sc = d.x; tc = -d.z; } }
    else                      { ma = az; if (d.z > 0) { face = 4; sc = d.x; tc = -d.y; } else { face = 5; sc = -d.x; tc = -d.y; } }
    if (ma <= 1e-12f) return glm::vec3(0.0f);
    const float u = 0.5f * (sc / ma + 1.0f), v = 0.5f * (tc / ma + 1.0f);
    const int x = std::clamp(int(u * m_size), 0, m_size - 1), y = std::clamp(int(v * m_size), 0, m_size - 1);
    const uint8_t* p = &m_faces[face][(size_t(y) * m_size + x) * 4];
    return glm::vec3(p[0], p[1], p[2]) * (1.0f / 255.0f);
}

GLuint Skybox::gl_texture() {
    if (soft_backend() || m_size == 0) return 0;
    if (m_gl_tex != 0) return m_gl_tex;
    glGenTextures(1, &m_gl_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_gl_tex);
    GLint prev_align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, m_size, m_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_faces[i].data());
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return m_gl_tex;
}

} // namespace crayon
