#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include "shader.hpp"

namespace crayon {

using UniformValue = std::variant<float, glm::vec2, glm::vec3, glm::vec4, int>;

struct PostEffectPass {
    std::string name;
    std::shared_ptr<Shader> shader;
    std::unordered_map<std::string, UniformValue> uniforms;
};

class PostProcessChain {
public:
    PostProcessChain();
    ~PostProcessChain();

    bool init(int width, int height);
    void shutdown();
    void resize(int width, int height);

    // Stack operations (camelCase)
    bool pushEffect(const std::string& nameOrBuiltin);
    bool pushEffect(std::shared_ptr<Shader> customShader, const std::string& name = "custom");
    bool popEffect();
    void clearEffects();
    size_t getEffectCount() const { return m_effects.size(); }
    bool hasActiveEffects() const { return !m_effects.empty(); }
    std::vector<PostEffectPass>& getEffects() { return m_effects; }
    const std::vector<PostEffectPass>& getEffects() const { return m_effects; }

    // Uniform setters for top-most effect (or all effects matching name)
    void setEffectUniform(const std::string& name, float val);
    void setEffectUniform(const std::string& name, float x, float y);
    void setEffectUniform(const std::string& name, float x, float y, float z);
    void setEffectUniform(const std::string& name, float x, float y, float z, float w);
    void setEffectUniform(const std::string& name, int val);

    // Execute the effect chain over input texture
    // Returns final texture ID after all passes
    GLuint process(GLuint inputTexture, int width, int height, float time);

private:
    void initBuiltinShaders();
    void createQuad();
    std::shared_ptr<Shader> getOrCreateShader(const std::string& name);

    int m_width = 320;
    int m_height = 240;

    // Ping-pong FBOs
    GLuint m_pingFbo = 0;
    GLuint m_pingTex = 0;
    GLuint m_pongFbo = 0;
    GLuint m_pongTex = 0;

    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;

    std::vector<PostEffectPass> m_effects;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_builtinShaders;
    bool m_initialized = false;
};

} // namespace crayon
