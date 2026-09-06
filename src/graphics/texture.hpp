#pragma once

#include <string>
#include <glad/glad.h>
#include <memory>

namespace crayon {

class Texture {
public:
    Texture();
    ~Texture();

    bool load_from_file(const std::string& filepath, bool nearest = true);
    bool load_from_memory(const unsigned char* data, int width, int height, int channels, bool nearest = true);

    void bind(unsigned int slot = 0) const;
    void unbind(unsigned int slot = 0) const;

    GLuint get_id() const { return m_texture_id; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    bool is_valid() const { return m_texture_id != 0; }

    static std::shared_ptr<Texture> create_white();
    static std::shared_ptr<Texture> create_checker();

private:
    GLuint m_texture_id = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace crayon
