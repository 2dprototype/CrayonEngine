#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace crayon {

class Shader {
public:
    Shader();
    ~Shader();

    // A Shader owns a GL program; copying would double-delete it.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool load_from_memory(const std::string& vertex_source, const std::string& fragment_source);
    bool load_from_file(const std::string& vertex_path, const std::string& fragment_path);

    void bind() const;
    void unbind() const;

    void set_mat4(std::string_view name, const glm::mat4& mat);
    void set_mat4_array(std::string_view name, const glm::mat4* mats, GLsizei count);
    void set_vec4(std::string_view name, const glm::vec4& vec);
    void set_vec3(std::string_view name, const glm::vec3& vec);
    void set_vec2(std::string_view name, const glm::vec2& vec);
    void set_float(std::string_view name, float val);
    void set_int(std::string_view name, int val);

    GLuint get_program_id() const { return m_program_id; }
    bool is_valid() const { return m_program_id != 0; }

    // Resolve a uniform location. Public so hot paths (mesh renderer, batch)
    // can resolve once and cache the GLint instead of hashing a name per draw.
    GLint get_uniform_location(std::string_view name);

private:
    GLuint compile_stage(GLenum type, const std::string& source);

    // Transparent hash/equal so lookups by string_view / const char* never
    // allocate a temporary std::string.
    struct NameHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };

    GLuint m_program_id = 0;
    std::unordered_map<std::string, GLint, NameHash, std::equal_to<>> m_uniform_cache;
};

} // namespace crayon
