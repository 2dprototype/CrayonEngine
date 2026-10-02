#include "post_process_chain.hpp"
#include "../core/log.hpp"
#include <algorithm>

namespace crayon {

static const char* EFFECT_VS = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* CHROMATIC_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform float u_strength;
uniform float u_angle;
void main() {
    float str = (u_strength != 0.0) ? u_strength : 0.006;
    vec2 dir = vec2(cos(u_angle), sin(u_angle)) * str;
    float r = texture(u_texture, v_uv + dir).r;
    float g = texture(u_texture, v_uv).g;
    float b = texture(u_texture, v_uv - dir).b;
    float a = texture(u_texture, v_uv).a;
    FragColor = vec4(r, g, b, a);
}
)";

static const char* VIGNETTE_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform float u_radius;
uniform float u_softness;
uniform float u_intensity;
uniform vec4 u_color;
void main() {
    vec4 col = texture(u_texture, v_uv);
    float rad = (u_radius != 0.0) ? u_radius : 0.75;
    float soft = (u_softness != 0.0) ? u_softness : 0.45;
    float inten = (u_intensity != 0.0) ? u_intensity : 1.0;
    vec4 vCol = (u_color.a != 0.0) ? u_color : vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center = v_uv - 0.5;
    float dist = length(center);
    float vignette = smoothstep(rad, rad - soft, dist);
    vignette = mix(1.0, vignette, inten);
    FragColor = mix(vCol, col, vignette);
}
)";

static const char* DISSOLVE_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform float u_threshold;
uniform vec4 u_burnColor;
uniform float u_burnSize;
uniform float u_noiseScale;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec4 col = texture(u_texture, v_uv);
    float scale = (u_noiseScale != 0.0) ? u_noiseScale : 15.0;
    float n = noise(v_uv * scale);
    float t = u_threshold;
    if (n < t) {
        discard;
    }
    float burn = (u_burnSize > 0.0) ? u_burnSize : 0.05;
    if (n < t + burn) {
        vec4 bc = (u_burnColor.a != 0.0) ? u_burnColor : vec4(1.0, 0.4, 0.1, 1.0);
        col = mix(bc, col, (n - t) / burn);
    }
    FragColor = col;
}
)";

static const char* VHS_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_jitter;
uniform float u_noise;
uniform float u_lines;

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = v_uv;
    float jitter = (u_jitter != 0.0) ? u_jitter : 0.003;
    float line_noise = sin(uv.y * 300.0 + u_time * 10.0);
    if (fract(sin(u_time * 5.0) * 10.0) > 0.85) {
        uv.x += (rand(vec2(u_time, uv.y)) - 0.5) * jitter * 3.0;
    }
    float r = texture(u_texture, uv + vec2(jitter, 0.0)).r;
    float g = texture(u_texture, uv).g;
    float b = texture(u_texture, uv - vec2(jitter, 0.0)).b;
    vec3 col = vec3(r, g, b);
    float n_strength = (u_noise != 0.0) ? u_noise : 0.08;
    float n = (rand(uv + vec2(u_time * 1.5, u_time * 0.7)) - 0.5) * n_strength;
    col += n;
    float scanline = sin(uv.y * 480.0) * 0.5 + 0.5;
    float l_strength = (u_lines != 0.0) ? u_lines : 0.1;
    col *= (1.0 - scanline * l_strength);
    FragColor = vec4(col, 1.0);
}
)";

static const char* BLOOM2D_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_threshold;
uniform float u_intensity;
void main() {
    vec4 base = texture(u_texture, v_uv);
    vec2 texel = 1.0 / u_resolution;
    vec3 bloom = vec3(0.0);
    float th = (u_threshold != 0.0) ? u_threshold : 0.6;
    float inten = (u_intensity != 0.0) ? u_intensity : 0.8;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec3 s = texture(u_texture, v_uv + vec2(float(x), float(y)) * texel * 2.0).rgb;
            float lum = dot(s, vec3(0.299, 0.587, 0.114));
            if (lum > th) {
                bloom += s * (lum - th);
            }
        }
    }
    bloom = (bloom / 25.0) * inten * 4.0;
    FragColor = vec4(base.rgb + bloom, base.a);
}
)";

static const char* PIXELATE_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_pixelSize;
void main() {
    float pSize = (u_pixelSize > 1.0) ? u_pixelSize : 4.0;
    vec2 pGrid = u_resolution / pSize;
    vec2 uv = floor(v_uv * pGrid) / pGrid;
    FragColor = texture(u_texture, uv);
}
)";

static const char* RADIAL_BLUR_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform vec2 u_center;
uniform float u_strength;
void main() {
    vec2 center = (u_center.x != 0.0 || u_center.y != 0.0) ? u_center : vec2(0.5);
    float str = (u_strength != 0.0) ? u_strength : 0.03;
    vec2 dir = (v_uv - center);
    vec4 col = vec4(0.0);
    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; ++i) {
        float scale = 1.0 - str * (float(i) / float(SAMPLES - 1));
        col += texture(u_texture, center + dir * scale);
    }
    FragColor = col / float(SAMPLES);
}
)";

static const char* FILM_GRAIN_FS = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_intensity;
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}
void main() {
    vec4 col = texture(u_texture, v_uv);
    float inten = (u_intensity != 0.0) ? u_intensity : 0.12;
    float n = (rand(v_uv * 2.0 + vec2(u_time * 3.14, -u_time * 1.618)) - 0.5) * inten;
    FragColor = vec4(col.rgb + n, col.a);
}
)";

PostProcessChain::PostProcessChain() = default;

PostProcessChain::~PostProcessChain() {
    shutdown();
}

static void createFboTex(GLuint& fbo, GLuint& tex, int w, int h) {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool PostProcessChain::init(int width, int height) {
    m_width = width;
    m_height = height;

    createFboTex(m_pingFbo, m_pingTex, m_width, m_height);
    createFboTex(m_pongFbo, m_pongTex, m_width, m_height);

    createQuad();
    initBuiltinShaders();

    m_initialized = true;
    CRAYON_LOG_INFO("PostProcessChain initialized at {}x{}", m_width, m_height);
    return true;
}

void PostProcessChain::shutdown() {
    if (m_pingFbo) { glDeleteFramebuffers(1, &m_pingFbo); m_pingFbo = 0; }
    if (m_pingTex) { glDeleteTextures(1, &m_pingTex); m_pingTex = 0; }
    if (m_pongFbo) { glDeleteFramebuffers(1, &m_pongFbo); m_pongFbo = 0; }
    if (m_pongTex) { glDeleteTextures(1, &m_pongTex); m_pongTex = 0; }

    if (m_quadVao) { glDeleteVertexArrays(1, &m_quadVao); m_quadVao = 0; }
    if (m_quadVbo) { glDeleteBuffers(1, &m_quadVbo); m_quadVbo = 0; }

    m_builtinShaders.clear();
    m_effects.clear();
    m_initialized = false;
}

void PostProcessChain::resize(int width, int height) {
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;

    if (m_pingTex) {
        glBindTexture(GL_TEXTURE_2D, m_pingTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    if (m_pongTex) {
        glBindTexture(GL_TEXTURE_2D, m_pongTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

void PostProcessChain::createQuad() {
    if (m_quadVao != 0) return;

    float vertices[] = {
        // pos (x, y)    uv (u, v)
        -1.0f,  1.0f,    0.0f, 1.0f,
        -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f,  1.0f,    1.0f, 1.0f,

         1.0f,  1.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,    1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);

    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void PostProcessChain::initBuiltinShaders() {
    auto loadBuiltin = [this](const std::string& name, const char* fsSource) {
        auto sh = std::make_shared<Shader>();
        if (sh->load_from_memory(EFFECT_VS, fsSource)) {
            m_builtinShaders[name] = sh;
        } else {
            CRAYON_LOG_ERROR("Failed to compile built-in post effect: {}", name);
        }
    };

    loadBuiltin("chromatic", CHROMATIC_FS);
    loadBuiltin("vignette", VIGNETTE_FS);
    loadBuiltin("dissolve", DISSOLVE_FS);
    loadBuiltin("vhs", VHS_FS);
    loadBuiltin("bloom2d", BLOOM2D_FS);
    loadBuiltin("pixelate", PIXELATE_FS);
    loadBuiltin("radialBlur", RADIAL_BLUR_FS);
    loadBuiltin("filmGrain", FILM_GRAIN_FS);
}

std::shared_ptr<Shader> PostProcessChain::getOrCreateShader(const std::string& name) {
    auto it = m_builtinShaders.find(name);
    if (it != m_builtinShaders.end()) {
        return it->second;
    }
    return nullptr;
}

bool PostProcessChain::pushEffect(const std::string& nameOrBuiltin) {
    auto shader = getOrCreateShader(nameOrBuiltin);
    if (!shader) {
        CRAYON_LOG_WARN("PostProcessChain: Unknown built-in effect '{}'", nameOrBuiltin);
        return false;
    }
    PostEffectPass pass;
    pass.name = nameOrBuiltin;
    pass.shader = shader;
    m_effects.push_back(pass);
    return true;
}

bool PostProcessChain::pushEffect(std::shared_ptr<Shader> customShader, const std::string& name) {
    if (!customShader) return false;
    PostEffectPass pass;
    pass.name = name;
    pass.shader = customShader;
    m_effects.push_back(pass);
    return true;
}

bool PostProcessChain::popEffect() {
    if (m_effects.empty()) return false;
    m_effects.pop_back();
    return true;
}

void PostProcessChain::clearEffects() {
    m_effects.clear();
}

void PostProcessChain::setEffectUniform(const std::string& name, float val) {
    if (!m_effects.empty()) {
        m_effects.back().uniforms[name] = val;
    }
}

void PostProcessChain::setEffectUniform(const std::string& name, float x, float y) {
    if (!m_effects.empty()) {
        m_effects.back().uniforms[name] = glm::vec2(x, y);
    }
}

void PostProcessChain::setEffectUniform(const std::string& name, float x, float y, float z) {
    if (!m_effects.empty()) {
        m_effects.back().uniforms[name] = glm::vec3(x, y, z);
    }
}

void PostProcessChain::setEffectUniform(const std::string& name, float x, float y, float z, float w) {
    if (!m_effects.empty()) {
        m_effects.back().uniforms[name] = glm::vec4(x, y, z, w);
    }
}

void PostProcessChain::setEffectUniform(const std::string& name, int val) {
    if (!m_effects.empty()) {
        m_effects.back().uniforms[name] = val;
    }
}

GLuint PostProcessChain::process(GLuint inputTexture, int width, int height, float time) {
    if (m_effects.empty()) {
        return inputTexture;
    }

    resize(width, height);

    GLuint currentInput = inputTexture;
    GLuint currentFbo = m_pingFbo;
    GLuint currentTex = m_pingTex;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glBindVertexArray(m_quadVao);

    for (size_t i = 0; i < m_effects.size(); ++i) {
        auto& pass = m_effects[i];
        if (!pass.shader) continue;

        glBindFramebuffer(GL_FRAMEBUFFER, currentFbo);
        glViewport(0, 0, m_width, m_height);
        glClear(GL_COLOR_BUFFER_BIT);

        pass.shader->bind();

        // Default uniforms
        pass.shader->set_int("u_texture", 0);
        pass.shader->set_float("u_time", time);
        pass.shader->set_vec2("u_resolution", glm::vec2(static_cast<float>(m_width), static_cast<float>(m_height)));

        // Pass-specific uniforms
        for (const auto& [uName, uVal] : pass.uniforms) {
            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, float>) {
                    pass.shader->set_float(uName, val);
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    pass.shader->set_vec2(uName, val);
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    pass.shader->set_vec3(uName, val);
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    pass.shader->set_vec4(uName, val);
                } else if constexpr (std::is_same_v<T, int>) {
                    pass.shader->set_int(uName, val);
                }
            }, uVal);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentInput);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Ping-pong for next pass
        currentInput = currentTex;
        if (currentFbo == m_pingFbo) {
            currentFbo = m_pongFbo;
            currentTex = m_pongTex;
        } else {
            currentFbo = m_pingFbo;
            currentTex = m_pingTex;
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return currentInput;
}

} // namespace crayon
