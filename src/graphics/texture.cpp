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
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(0);

    // Load with the file's natural channel count (1/2/3/4) instead of always
    // expanding to RGBA: greyscale and RGB images use 25-75% less memory.
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &channels, 0);
    if (!data) {
        CRAYON_LOG_ERROR("Failed to load texture file: {}", filepath);
        return false;
    }

    bool success = load_from_memory(data, w, h, channels, nearest);
    stbi_image_free(data);
    return success;
}

bool Texture::load_from_memory(const unsigned char* data, int width, int height, int channels, bool nearest) {
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }

    if (!data || width <= 0 || height <= 0) {
        m_width = 0;
        m_height = 0;
        return false;
    }

    m_width = width;
    m_height = height;

    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    // Tightly packed rows. The GL default (4-byte row alignment) corrupts or
    // over-reads 1- and 3-channel images whose width*channels is not a multiple of 4.
    GLint prev_align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum format = GL_RGBA;
    GLint internal_format = GL_RGBA8;
    GLint swizzle[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    bool needs_swizzle = false;

    switch (channels) {
        case 1: // greyscale -> (g, g, g, 1)
            format = GL_RED;  internal_format = GL_R8;
            swizzle[0] = GL_RED; swizzle[1] = GL_RED; swizzle[2] = GL_RED; swizzle[3] = GL_ONE;
            needs_swizzle = true;
            break;
        case 2: // grey + alpha -> (g, g, g, a)
            format = GL_RG;   internal_format = GL_RG8;
            swizzle[0] = GL_RED; swizzle[1] = GL_RED; swizzle[2] = GL_RED; swizzle[3] = GL_GREEN;
            needs_swizzle = true;
            break;
        case 3:
            format = GL_RGB;  internal_format = GL_RGB8;
            break;
        default:
            format = GL_RGBA; internal_format = GL_RGBA8;
            break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, m_width, m_height, 0, format, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);

    if (needs_swizzle) {
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }

    // Mipmaps: without these, textures on anything that recedes into the
    // distance (floors, terrain, most real game props) alias and shimmer
    // instead of smoothly minifying. Skip them for "nearest" textures, since
    // that mode is normally chosen deliberately for crisp pixel-art look and
    // GL_NEAREST_MIPMAP filtering would soften it.
    GLint min_filter = GL_LINEAR;
    GLint mag_filter = GL_LINEAR;
    if (nearest) {
        min_filter = GL_NEAREST;
        mag_filter = GL_NEAREST;
    } else {
        glGenerateMipmap(GL_TEXTURE_2D);
        min_filter = GL_LINEAR_MIPMAP_LINEAR;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
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
