#include "editor_theme.hpp"
#include <imgui.h>

namespace crayon::editor {

void apply_modern_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Window & Frame styling
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;

    // Modern Graphite & Deep Slate Palette
    const ImVec4 bg_darkest   = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    const ImVec4 bg_dark      = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
    const ImVec4 bg_medium    = ImVec4(0.18f, 0.19f, 0.23f, 1.0f);
    const ImVec4 bg_light     = ImVec4(0.24f, 0.25f, 0.30f, 1.0f);

    const ImVec4 accent       = ImVec4(0.20f, 0.45f, 0.85f, 1.0f);
    const ImVec4 accent_hover = ImVec4(0.26f, 0.54f, 0.95f, 1.0f);
    const ImVec4 accent_active= ImVec4(0.15f, 0.38f, 0.75f, 1.0f);

    const ImVec4 text_primary = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
    const ImVec4 text_muted   = ImVec4(0.60f, 0.62f, 0.68f, 1.0f);

    colors[ImGuiCol_Text]                  = text_primary;
    colors[ImGuiCol_TextDisabled]          = text_muted;
    colors[ImGuiCol_WindowBg]              = bg_dark;
    colors[ImGuiCol_ChildBg]               = bg_darkest;
    colors[ImGuiCol_PopupBg]               = bg_dark;
    colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.23f, 0.27f, 0.8f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg]               = bg_medium;
    colors[ImGuiCol_FrameBgHovered]        = bg_light;
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.30f, 0.36f, 1.0f);

    colors[ImGuiCol_TitleBg]               = bg_darkest;
    colors[ImGuiCol_TitleBgActive]         = bg_medium;
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.08f, 0.10f, 0.6f);

    colors[ImGuiCol_MenuBarBg]             = bg_darkest;
    colors[ImGuiCol_ScrollbarBg]           = bg_darkest;
    colors[ImGuiCol_ScrollbarGrab]         = bg_light;
    colors[ImGuiCol_ScrollbarGrabHovered]  = accent;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_active;

    colors[ImGuiCol_CheckMark]             = accent;
    colors[ImGuiCol_SliderGrab]            = accent;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;

    colors[ImGuiCol_Button]                = bg_medium;
    colors[ImGuiCol_ButtonHovered]         = accent;
    colors[ImGuiCol_ButtonActive]          = accent_active;

    colors[ImGuiCol_Header]                = bg_medium;
    colors[ImGuiCol_HeaderHovered]         = accent;
    colors[ImGuiCol_HeaderActive]          = accent_active;

    colors[ImGuiCol_Separator]             = ImVec4(0.24f, 0.25f, 0.29f, 0.8f);
    colors[ImGuiCol_SeparatorHovered]      = accent;
    colors[ImGuiCol_SeparatorActive]       = accent_active;

    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.25f, 0.25f, 0.30f, 0.5f);
    colors[ImGuiCol_ResizeGripHovered]     = accent;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;

    colors[ImGuiCol_Tab]                   = bg_darkest;
    colors[ImGuiCol_TabHovered]            = bg_light;
    colors[ImGuiCol_TabActive]             = bg_medium;
    colors[ImGuiCol_TabUnfocused]          = bg_darkest;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_dark;

    colors[ImGuiCol_PlotLines]             = accent_hover;
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.0f, 0.43f, 0.35f, 1.0f);
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = accent_hover;

    colors[ImGuiCol_TableHeaderBg]         = bg_medium;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.25f, 0.26f, 0.31f, 1.0f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.21f, 0.25f, 1.0f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);

    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.20f, 0.45f, 0.85f, 0.4f);
    colors[ImGuiCol_DragDropTarget]        = accent_hover;
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);
}

} // namespace crayon::editor
