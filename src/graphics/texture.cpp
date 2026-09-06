#include "texture.hpp"
#include "../core/log.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace crayon {

Texture::Texture() = default;

Texture::~Texture() {
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }
}

bool Texture::load_from_file(const std::string& filepath, bool nearest) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 4);
    if (!data) {
        CRAYON_LOG_ERROR("Failed to load texture file: {}", filepath);
        return false;
    }

    bool success = load_from_memory(data, w, h, 4, nearest);
    stbi_image_free(data);
    return success;
}

bool Texture::load_from_memory(const unsigned char* data, int width, int height, int channels, bool nearest) {
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }

    m_width = width;
    m_height = height;

    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    GLenum format = GL_RGBA;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, format, GL_UNSIGNED_BYTE, data);

    GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
}

void Texture::unbind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::shared_ptr<Texture> Texture::create_white() {
    auto tex = std::make_shared<Texture>();
    unsigned char white_pixel[4] = {255, 255, 255, 255};
    tex->load_from_memory(white_pixel, 1, 1, 4, true);
    return tex;
}

std::shared_ptr<Texture> Texture::create_checker() {
    auto tex = std::make_shared<Texture>();
    unsigned char checker_pixels[4 * 4] = {
        220, 220, 220, 255,   80,  80,  80, 255,
         80,  80,  80, 255,  220, 220, 220, 255
    };
    tex->load_from_memory(checker_pixels, 2, 2, 4, true);
    return tex;
}

} // namespace crayon
