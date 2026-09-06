#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace crayon {

class Shader {
public:
    Shader();
    ~Shader();

    bool load_from_memory(const std::string& vertex_source, const std::string& fragment_source);
    bool load_from_file(const std::string& vertex_path, const std::string& fragment_path);

    void bind() const;
    void unbind() const;

    void set_mat4(const std::string& name, const glm::mat4& mat);
    void set_vec4(const std::string& name, const glm::vec4& vec);
    void set_vec3(const std::string& name, const glm::vec3& vec);
    void set_vec2(const std::string& name, const glm::vec2& vec);
    void set_float(const std::string& name, float val);
    void set_int(const std::string& name, int val);

    GLuint get_program_id() const { return m_program_id; }
    bool is_valid() const { return m_program_id != 0; }

private:
    GLint get_uniform_location(const std::string& name);
    GLuint compile_stage(GLenum type, const std::string& source);

    GLuint m_program_id = 0;
    std::unordered_map<std::string, GLint> m_uniform_cache;
};

} // namespace crayon
