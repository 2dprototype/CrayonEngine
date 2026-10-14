#pragma once

#include <string>
#include <glad/glad.h>
#include <memory>

namespace crayon {

enum class TextureWrap { Repeat, Clamp, Mirror };

class Texture {
public:
    Texture();
    ~Texture();

    // Owns a GL texture; copying would double-delete it.
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool load_from_file(const std::string& filepath, bool nearest = true);
    bool load_from_memory(const unsigned char* data, int width, int height, int channels, bool nearest = true);

    void bind(unsigned int slot = 0) const;
    void unbind(unsigned int slot = 0) const;

    GLuint get_id() const { return m_texture_id; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    bool is_valid() const { return m_texture_id != 0; }

    // ---- sampling state, changeable after load (works on both renderer backends) ----
    // nearest = crisp pixels; otherwise bilinear, optionally with a mip chain (smooth minification).
    void set_filter(bool nearest, bool mipmaps = true);
    void set_wrap(TextureWrap wrap);
    bool is_nearest() const { return m_nearest; }
    bool has_mipmaps() const { return m_has_mips; }
    TextureWrap get_wrap() const { return m_wrap; }
    // Replaces the pixels of an RGBA8 texture in place (same size). Used by ImageData textures.
    bool update_rgba(const unsigned char* rgba, int width, int height);
    // Looks up a live texture by its id (GL name, or registry handle on the software backend).
    static Texture* find(GLuint id);

    static std::shared_ptr<Texture> create_white();
    static std::shared_ptr<Texture> create_checker();

private:
    GLuint m_texture_id = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 4;
    bool m_nearest = true;
    bool m_has_mips = false;
    TextureWrap m_wrap = TextureWrap::Repeat;
};

} // namespace crayon
