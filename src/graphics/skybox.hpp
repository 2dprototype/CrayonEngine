#pragma once
// Skybox: six RGBA8 cube faces in OpenGL cubemap order (+X -X +Y -Y +Z -Z, image row 0 = top),
// built from six images or from one equirectangular panorama (converted to faces at load).
// The CPU keeps the faces; the GL backend uploads a GL_TEXTURE_CUBE_MAP lazily, the software
// backend samples the faces directly.
#include "image_data.hpp"
#include <array>
#include <memory>
#include <string>
#include <glm/glm.hpp>

#include <glad/glad.h>

namespace crayon {

class Skybox {
public:
    ~Skybox();

    // Order: +X, -X, +Y, -Y, +Z, -Z. All faces must be square and the same size.
    bool load_faces(const std::array<std::shared_ptr<ImageData>, 6>& faces);
    bool load_face_files(const std::array<std::string, 6>& paths);
    // Equirectangular panorama (2:1). face_size 0 = pick from the panorama height.
    bool load_equirect(const ImageData& panorama, int face_size = 0);
    bool load_equirect_file(const std::string& path, int face_size = 0);

    bool valid() const { return m_size > 0; }
    int face_size() const { return m_size; }
    const uint8_t* face_data(int i) const { return m_faces[i].data(); }

    // CPU lookup (nearest) following the OpenGL cube-map face selection rules.
    glm::vec3 sample(const glm::vec3& dir) const;

    // GL backend: cube texture handle (created on first use). 0 on the software backend.
    GLuint gl_texture();

private:
    int m_size = 0;
    std::array<std::vector<uint8_t>, 6> m_faces;
    GLuint m_gl_tex = 0;
};

} // namespace crayon
