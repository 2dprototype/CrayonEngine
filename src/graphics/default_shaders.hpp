#pragma once

namespace crayon {

// 2D Batch Shader
inline const char* SHADER_2D_VS = R"(#version 330 core
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;

uniform mat4 u_proj;

out vec2 v_uv;
out vec4 v_color;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)";

inline const char* SHADER_2D_FS = R"(#version 330 core
in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    vec4 tex = texture(u_texture, v_uv);
    frag_color = tex * v_color;
}
)";

// 3D Retro Mesh Shader
inline const char* SHADER_3D_VS = R"(#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in vec4 a_color;
layout (location = 4) in uvec4 a_joints;
layout (location = 5) in vec4 a_weights;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

uniform int u_is_skinned;
const int MAX_BONES = 128;
uniform mat4 u_bone_matrices[MAX_BONES];

uniform int u_jitter_enabled;
uniform vec2 u_jitter_res;

out vec2 v_uv_persp;
out vec2 v_uv_affine;
out vec4 v_color;
out vec3 v_normal;
out vec3 v_world_pos;
out vec3 v_view_pos;

void main() {
    vec4 local_pos = vec4(a_pos, 1.0);
    vec3 local_normal = a_normal;

    if (u_is_skinned != 0) {
        mat4 skin_mat = u_bone_matrices[a_joints.x] * a_weights.x +
                        u_bone_matrices[a_joints.y] * a_weights.y +
                        u_bone_matrices[a_joints.z] * a_weights.z +
                        u_bone_matrices[a_joints.w] * a_weights.w;
        local_pos = skin_mat * local_pos;
        local_normal = mat3(skin_mat) * local_normal;
    }

    vec4 world_pos = u_model * local_pos;
    v_world_pos = world_pos.xyz;

    vec4 view_pos = u_view * world_pos;
    v_view_pos = view_pos.xyz;

    mat3 normal_matrix = transpose(inverse(mat3(u_model)));
    v_normal = normalize(normal_matrix * local_normal);

    v_color = a_color;
    v_uv_persp = a_uv;

    vec4 clip_pos = u_proj * view_pos;

    // By multiplying UV by clip_pos.w, the hardware perspective division cancels out,
    // yielding affine screen-space linear interpolation!
    v_uv_affine = a_uv * clip_pos.w;

    if (u_jitter_enabled != 0 && u_jitter_res.x > 0.0 && u_jitter_res.y > 0.0) {
        vec3 ndc = clip_pos.xyz / clip_pos.w;
        vec2 snapped = floor(ndc.xy * u_jitter_res * 0.5 + 0.5) / (u_jitter_res * 0.5);
        clip_pos = vec4(snapped * clip_pos.w, clip_pos.z, clip_pos.w);
    }

    gl_Position = clip_pos;
}
)";

inline const char* SHADER_3D_FS = R"(#version 330 core
in vec2 v_uv_persp;
in vec2 v_uv_affine;
in vec4 v_color;
in vec3 v_normal;
in vec3 v_world_pos;
in vec3 v_view_pos;

uniform sampler2D u_texture;
uniform float u_affine_blend; // 0.0 = perspective correct, 1.0 = affine

// Directional Light
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient_color;

// Shading Mode: 0 = Gouraud, 1 = Flat, 2 = Unlit
uniform int u_shading_mode;

// Point Lights (up to 4)
struct PointLight {
    vec3 pos;
    vec3 color;
    float radius;
    float intensity;
};
uniform int u_num_point_lights;
uniform PointLight u_point_lights[4];

// Distance Fog
uniform int u_fog_enabled;
uniform float u_fog_start;
uniform float u_fog_end;
uniform vec3 u_fog_color;

out vec4 frag_color;

void main() {
    vec2 uv = mix(v_uv_persp, v_uv_affine, clamp(u_affine_blend, 0.0, 1.0));
    vec4 tex = texture(u_texture, uv);

    vec3 N = normalize(v_normal);
    if (u_shading_mode == 1) {
        // Flat shading via screen derivatives
        vec3 fdx = dFdx(v_world_pos);
        vec3 fdy = dFdy(v_world_pos);
        N = normalize(cross(fdx, fdy));
    }

    vec3 lighting;
    if (u_shading_mode == 2) {
        lighting = vec3(1.0); // Unlit
    } else {
        vec3 L = normalize(-u_light_dir);
        float diff = max(dot(N, L), 0.0);
        lighting = u_ambient_color + u_light_color * diff;

        // Add Point Lights
        for (int i = 0; i < u_num_point_lights && i < 4; ++i) {
            vec3 light_dir = u_point_lights[i].pos - v_world_pos;
            float dist = length(light_dir);
            if (dist < u_point_lights[i].radius && dist > 0.0001) {
                vec3 pL = light_dir / dist;
                float pDiff = max(dot(N, pL), 0.0);
                float atten = clamp(1.0 - (dist / u_point_lights[i].radius), 0.0, 1.0);
                atten = atten * atten * u_point_lights[i].intensity;
                lighting += u_point_lights[i].color * (pDiff * atten);
            }
        }
    }

    vec4 lit_color = vec4(tex.rgb * v_color.rgb * lighting, tex.a * v_color.a);

    if (u_fog_enabled != 0) {
        float dist = length(v_view_pos);
        float fog_factor = clamp((u_fog_end - dist) / (u_fog_end - u_fog_start), 0.0, 1.0);
        lit_color.rgb = mix(u_fog_color, lit_color.rgb, fog_factor);
    }

    if (lit_color.a < 0.05) {
        discard;
    }

    frag_color = lit_color;
}
)";

// Fullscreen Blit / Post-Processing Shader
inline const char* SHADER_POST_VS = R"(#version 330 core
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_uv;

out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

inline const char* SHADER_POST_FS = R"(#version 330 core
in vec2 v_uv;

uniform sampler2D u_screen_texture;
uniform int u_dither_enabled;
uniform float u_dither_levels; // 32.0 for 15-bit, 8.0 for 8-bit
uniform vec2 u_virtual_res;

// CRT Effects
uniform int u_crt_scanlines;
uniform float u_scanline_strength;
uniform int u_crt_curvature;
uniform float u_curvature_distort;
uniform int u_vignette;
uniform float u_vignette_strength;

out vec4 frag_color;

// 4x4 Bayer Dithering Matrix
const float bayer4x4[16] = float[16](
     0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0,
    12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0,
     3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0,
    15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0
);

vec2 apply_curvature(vec2 uv, float distort) {
    vec2 cc = uv - 0.5;
    float dist = dot(cc, cc);
    return uv + cc * dist * distort;
}

void main() {
    vec2 uv = v_uv;
    if (u_crt_curvature != 0) {
        uv = apply_curvature(uv, u_curvature_distort);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            frag_color = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec4 color = texture(u_screen_texture, uv);

    if (u_dither_enabled != 0) {
        ivec2 pixel_coord = ivec2(uv * u_virtual_res);
        int bx = pixel_coord.x % 4;
        int by = pixel_coord.y % 4;
        float bayer = bayer4x4[by * 4 + bx];

        float levels = u_dither_levels > 1.0 ? u_dither_levels : 32.0;
        vec3 dithered = floor(color.rgb * levels + (bayer - 0.5)) / (levels - 1.0);
        color.rgb = clamp(dithered, 0.0, 1.0);
    }

    // CRT Scanlines
    if (u_crt_scanlines != 0) {
        float scanline = sin(uv.y * u_virtual_res.y * 3.14159265);
        color.rgb -= color.rgb * (scanline * scanline * u_scanline_strength);
    }

    // Vignette
    if (u_vignette != 0) {
        float vig = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
        color.rgb *= clamp(pow(vig, u_vignette_strength), 0.0, 1.0);
    }

    frag_color = color;
}
)";

} // namespace crayon
