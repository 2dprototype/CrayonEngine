#include "editor_theme.hpp"
#include <imgui.h>

namespace crayon::editor {

void apply_modern_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 7.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 7.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(9.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;

    const ImVec4 bg_darkest   = ImVec4(0.085f, 0.088f, 0.105f, 1.00f);
    const ImVec4 bg_dark      = ImVec4(0.115f, 0.120f, 0.140f, 1.00f);
    const ImVec4 bg_medium    = ImVec4(0.155f, 0.165f, 0.195f, 1.00f);
    const ImVec4 bg_light     = ImVec4(0.215f, 0.230f, 0.275f, 1.00f);

    const ImVec4 accent       = ImVec4(0.245f, 0.510f, 0.925f, 1.00f);
    const ImVec4 accent_hover = ImVec4(0.310f, 0.590f, 0.980f, 1.00f);
    const ImVec4 accent_active= ImVec4(0.175f, 0.400f, 0.780f, 1.00f);

    const ImVec4 text_primary = ImVec4(0.930f, 0.940f, 0.965f, 1.00f);
    const ImVec4 text_muted   = ImVec4(0.560f, 0.590f, 0.650f, 1.00f);

    colors[ImGuiCol_Text]                  = text_primary;
    colors[ImGuiCol_TextDisabled]          = text_muted;
    colors[ImGuiCol_WindowBg]              = bg_dark;
    colors[ImGuiCol_ChildBg]               = bg_darkest;
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.105f, 0.125f, 0.985f);
    colors[ImGuiCol_Border]                = ImVec4(0.190f, 0.205f, 0.240f, 0.90f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg]               = bg_medium;
    colors[ImGuiCol_FrameBgHovered]        = bg_light;
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.265f, 0.290f, 0.345f, 1.00f);

    colors[ImGuiCol_TitleBg]               = bg_darkest;
    colors[ImGuiCol_TitleBgActive]         = bg_medium;
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.070f, 0.075f, 0.090f, 0.75f);

    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.095f, 0.098f, 0.115f, 1.00f);
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

    colors[ImGuiCol_Separator]             = ImVec4(0.220f, 0.235f, 0.270f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]      = accent;
    colors[ImGuiCol_SeparatorActive]       = accent_active;

    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.240f, 0.255f, 0.300f, 0.55f);
    colors[ImGuiCol_ResizeGripHovered]     = accent;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;

    colors[ImGuiCol_Tab]                   = ImVec4(0.100f, 0.105f, 0.125f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.190f, 0.215f, 0.275f, 1.00f);
    colors[ImGuiCol_TabActive]             = bg_medium;
    colors[ImGuiCol_TabUnfocused]          = bg_darkest;
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.135f, 0.145f, 0.170f, 1.00f);

    colors[ImGuiCol_PlotLines]             = accent_hover;
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = accent_hover;

    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.135f, 0.145f, 0.170f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.230f, 0.245f, 0.285f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.180f, 0.195f, 0.225f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.028f);

    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.245f, 0.510f, 0.925f, 0.42f);
    colors[ImGuiCol_DragDropTarget]        = accent_hover;
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.62f);
}

} // namespace crayon::editor