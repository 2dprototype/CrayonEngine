#include "shader.hpp"
#include "../core/log.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <vector>

namespace crayon {

Shader::Shader() = default;

Shader::~Shader() {
    if (m_program_id != 0) {
        glDeleteProgram(m_program_id);
        m_program_id = 0;
    }
}

GLuint Shader::compile_stage(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src_ptr = source.c_str();
    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 1024);
        glGetShaderInfoLog(shader, static_cast<GLsizei>(info_log.size()), nullptr, info_log.data());
        CRAYON_LOG_ERROR("Shader compilation error (type 0x{:X}):\n{}", type, info_log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::load_from_memory(const std::string& vertex_source, const std::string& fragment_source) {
    if (m_program_id != 0) {
        glDeleteProgram(m_program_id);
        m_program_id = 0;
        m_uniform_cache.clear();
    }

    GLuint vs = compile_stage(GL_VERTEX_SHADER, vertex_source);
    if (!vs) return false;

    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fragment_source);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> info_log(log_length > 0 ? log_length : 1024);
        glGetProgramInfoLog(program, static_cast<GLsizei>(info_log.size()), nullptr, info_log.data());
        CRAYON_LOG_ERROR("Shader program link error:\n{}", info_log.data());
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program);
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    m_program_id = program;
    return true;
}

bool Shader::load_from_file(const std::string& vertex_path, const std::string& fragment_path) {
    std::ifstream v_file(vertex_path);
    if (!v_file.is_open()) {
        CRAYON_LOG_ERROR("Failed to open vertex shader file: {}", vertex_path);
        return false;
    }
    std::stringstream v_stream;
    v_stream << v_file.rdbuf();

    std::ifstream f_file(fragment_path);
    if (!f_file.is_open()) {
        CRAYON_LOG_ERROR("Failed to open fragment shader file: {}", fragment_path);
        return false;
    }
    std::stringstream f_stream;
    f_stream << f_file.rdbuf();

    return load_from_memory(v_stream.str(), f_stream.str());
}

void Shader::bind() const {
    if (m_program_id != 0) {
        glUseProgram(m_program_id);
    }
}

void Shader::unbind() const {
    glUseProgram(0);
}

GLint Shader::get_uniform_location(const std::string& name) {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(m_program_id, name.c_str());
    m_uniform_cache[name] = loc;
    return loc;
}

void Shader::set_mat4(const std::string& name, const glm::mat4& mat) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
    }
}

void Shader::set_mat4_array(const std::string& name, const glm::mat4* mats, GLsizei count) {
    GLint loc = get_uniform_location(name);
    if (loc != -1 && mats && count > 0) {
        glUniformMatrix4fv(loc, count, GL_FALSE, glm::value_ptr(mats[0]));
    }
}

void Shader::set_vec4(const std::string& name, const glm::vec4& vec) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform4fv(loc, 1, glm::value_ptr(vec));
    }
}

void Shader::set_vec3(const std::string& name, const glm::vec3& vec) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform3fv(loc, 1, glm::value_ptr(vec));
    }
}

void Shader::set_vec2(const std::string& name, const glm::vec2& vec) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform2fv(loc, 1, glm::value_ptr(vec));
    }
}

void Shader::set_float(const std::string& name, float val) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1f(loc, val);
    }
}

void Shader::set_int(const std::string& name, int val) {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1i(loc, val);
    }
}

} // namespace crayon
