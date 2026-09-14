#include "retro_fx_panel.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/graphics/mesh3d.hpp"
#include <imgui.h>
#include <format>

namespace crayon::editor {

RetroFXPanel::RetroFXPanel(crayon::Engine& engine)
    : EditorPanel("Retro FX Tuner"), m_engine(engine) {}

void RetroFXPanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void RetroFXPanel::render_content() {
    auto& retro = m_engine.get_mesh_renderer().get_retro_effects();

        // 1. Dithering & Color Quantization
        if (ImGui::CollapsingHeader("Bayer Matrix Dithering", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Dithering Enabled", &retro.dither_enabled);
            if (retro.dither_enabled) {
                ImGui::SliderFloat("Color Levels", &retro.dither_levels, 2.0f, 64.0f, "%.0f");
                ImGui::TextDisabled("32.0 = 15-bit RGB555 (PS1), 8.0 = 8-bit retro");
            }
        }

        // 2. CRT & Scanlines Post-Processing
        if (ImGui::CollapsingHeader("CRT & Display FX", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Scanlines", &retro.crt_scanlines);
            if (retro.crt_scanlines) {
                ImGui::SliderFloat("Scanline Strength", &retro.scanline_strength, 0.05f, 1.0f, "%.2f");
            }

            ImGui::Checkbox("CRT Curvature", &retro.crt_curvature);
            if (retro.crt_curvature) {
                ImGui::SliderFloat("Curvature Distortion", &retro.curvature_distort, 0.01f, 0.3f, "%.3f");
            }

            ImGui::Checkbox("Vignette", &retro.vignette);
            if (retro.vignette) {
                ImGui::SliderFloat("Vignette Strength", &retro.vignette_strength, 0.05f, 1.0f, "%.2f");
            }
        }

        // 3. PS1 Geometric Wobble & Affine Texturing
        if (ImGui::CollapsingHeader("PS1 Retro 3D Emulation", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Vertex Jitter / Snapping", &retro.jitter_enabled);
            if (retro.jitter_enabled) {
                ImGui::DragFloat2("Snap Res (W x H)", &retro.jitter_resolution.x, 1.0f, 40.0f, 640.0f, "%.0f");
            }

            ImGui::SliderFloat("Affine Warp Blend", &retro.affine_blend, 0.0f, 1.0f, "%.2f");
            ImGui::TextDisabled("0.0 = Perspective Correct, 1.0 = Full Affine Warping");
        }

        // 4. Distance Fog
        if (ImGui::CollapsingHeader("Distance Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Fog Enabled", &retro.fog_enabled);
            if (retro.fog_enabled) {
                ImGui::DragFloat("Fog Start", &retro.fog_start, 0.5f, 0.0f, 500.0f, "%.1f");
                ImGui::DragFloat("Fog End", &retro.fog_end, 0.5f, retro.fog_start, 1000.0f, "%.1f");
                ImGui::ColorEdit3("Fog Color", &retro.fog_color.r);
            }
        }

        ImGui::Separator();

        // Preset buttons
        ImGui::Text("Presets:");
        if (ImGui::Button("PlayStation 1 (1995)")) {
            retro.jitter_enabled = true;
            retro.jitter_resolution = glm::vec2(160.0f, 120.0f);
            retro.affine_blend = 1.0f;
            retro.dither_enabled = true;
            retro.dither_levels = 32.0f;
            retro.crt_scanlines = true;
            retro.scanline_strength = 0.25f;
            retro.crt_curvature = false;
            retro.vignette = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Arcade CRT (1992)")) {
            retro.jitter_enabled = false;
            retro.affine_blend = 0.0f;
            retro.dither_enabled = true;
            retro.dither_levels = 16.0f;
            retro.crt_scanlines = true;
            retro.scanline_strength = 0.40f;
            retro.crt_curvature = true;
            retro.curvature_distort = 0.06f;
            retro.vignette = true;
            retro.vignette_strength = 0.35f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clean Modern")) {
            retro.jitter_enabled = false;
            retro.affine_blend = 0.0f;
            retro.dither_enabled = false;
            retro.crt_scanlines = false;
            retro.crt_curvature = false;
            retro.vignette = false;
        }

        ImGui::Separator();

        // Copy Lua code
        if (ImGui::Button("Copy to Lua Code", ImVec2(-1, 0))) {
            std::string code = std::format(
                "crayon.graphics.set_retro_effects({{\n"
                "    jitter_resolution = {}, -- {{ {}, {} }}\n"
                "    affine = {:.2f},\n"
                "    dither = {},\n"
                "    dither_levels = {:.1f},\n"
                "    scanlines = {},\n"
                "    scanline_strength = {:.2f},\n"
                "    crt_curvature = {},\n"
                "    curvature_distort = {:.3f},\n"
                "    vignette = {},\n"
                "    vignette_strength = {:.2f},\n"
                "    fog = {{ start = {:.1f}, [\"end\"] = {:.1f}, color = {{ {:.2f}, {:.2f}, {:.2f} }} }}\n"
                "}})",
                retro.jitter_enabled ? std::format("{{ {}, {} }}", (int)retro.jitter_resolution.x, (int)retro.jitter_resolution.y) : "nil",
                (int)retro.jitter_resolution.x, (int)retro.jitter_resolution.y,
                retro.affine_blend,
                retro.dither_enabled ? "true" : "false",
                retro.dither_levels,
                retro.crt_scanlines ? "true" : "false",
                retro.scanline_strength,
                retro.crt_curvature ? "true" : "false",
                retro.curvature_distort,
                retro.vignette ? "true" : "false",
                retro.vignette_strength,
                retro.fog_start, retro.fog_end,
                retro.fog_color.r, retro.fog_color.g, retro.fog_color.b
            );
            ImGui::SetClipboardText(code.c_str());
        }
}

} // namespace crayon::editor
