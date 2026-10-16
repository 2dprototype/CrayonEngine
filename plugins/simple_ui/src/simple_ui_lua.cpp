#include "simple_ui_lua.hpp"
#include "simple_ui_core.hpp"
#include <lua.hpp>
#include <imgui.h>
#include <cstring>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Helper
// -----------------------------------------------------------------------------
static ImTextureID get_texture_id_from_lua(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        lua_getfield(L, idx, "getId");
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, idx);
            lua_call(L, 1, 1);
            ImTextureID id = (ImTextureID)(intptr_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            return id;
        }
        lua_pop(L, 1);
    }
    return (ImTextureID)(intptr_t)luaL_checkinteger(L, idx);
}

// -----------------------------------------------------------------------------
// Windows & Layout
// -----------------------------------------------------------------------------
static int l_begin_window(lua_State* L) {
    SimpleUI::instance().ensure_frame_started();
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* title = luaL_checkstring(L, 1);
    bool has_open = !lua_isnoneornil(L, 2);
    bool is_open = has_open ? lua_toboolean(L, 2) : true;
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 3, 0);

    bool should_draw = false;
    if (has_open) {
        should_draw = ImGui::Begin(title, &is_open, flags);
    } else {
        should_draw = ImGui::Begin(title, nullptr, flags);
    }

    lua_pushboolean(L, is_open);
    lua_pushboolean(L, should_draw);
    return 2;
}

static int l_end_window(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::End();
    }
    return 0;
}

static int l_begin_child(lua_State* L) {
    SimpleUI::instance().ensure_frame_started();
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    float w = (float)luaL_optnumber(L, 2, 0.0);
    float h = (float)luaL_optnumber(L, 3, 0.0);
    bool border = lua_isboolean(L, 4) ? lua_toboolean(L, 4) : false;
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 5, 0);

    bool should_draw = ImGui::BeginChild(str_id, ImVec2(w, h), border, flags);
    lua_pushboolean(L, should_draw);
    return 1;
}

static int l_end_child(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndChild();
    }
    return 0;
}

static int l_set_next_window_pos(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    ImGuiCond cond = (ImGuiCond)luaL_optinteger(L, 3, 0);
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::SetNextWindowPos(ImVec2(x, y), cond);
    }
    return 0;
}

static int l_set_next_window_size(lua_State* L) {
    float w = (float)luaL_checknumber(L, 1);
    float h = (float)luaL_checknumber(L, 2);
    ImGuiCond cond = (ImGuiCond)luaL_optinteger(L, 3, 0);
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::SetNextWindowSize(ImVec2(w, h), cond);
    }
    return 0;
}

static int l_set_next_window_bg_alpha(lua_State* L) {
    float alpha = (float)luaL_checknumber(L, 1);
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::SetNextWindowBgAlpha(alpha);
    }
    return 0;
}

static int l_set_next_window_focus(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::SetNextWindowFocus();
    }
    return 0;
}

static int l_is_window_hovered(lua_State* L) {
    bool hovered = SimpleUI::instance().has_active_frame() ? ImGui::IsWindowHovered() : false;
    lua_pushboolean(L, hovered);
    return 1;
}

static int l_is_window_focused(lua_State* L) {
    bool focused = SimpleUI::instance().has_active_frame() ? ImGui::IsWindowFocused() : false;
    lua_pushboolean(L, focused);
    return 1;
}

static int l_get_window_pos(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 p = ImGui::GetWindowPos();
        lua_pushnumber(L, p.x);
        lua_pushnumber(L, p.y);
        return 2;
    }
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    return 2;
}

static int l_get_window_size(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 s = ImGui::GetWindowSize();
        lua_pushnumber(L, s.x);
        lua_pushnumber(L, s.y);
        return 2;
    }
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    return 2;
}

// -----------------------------------------------------------------------------
// Tabs & Menus
// -----------------------------------------------------------------------------
static int l_begin_tab_bar(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    ImGuiTabBarFlags flags = (ImGuiTabBarFlags)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, ImGui::BeginTabBar(str_id, flags));
    return 1;
}

static int l_end_tab_bar(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndTabBar();
    }
    return 0;
}

static int l_begin_tab_item(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    bool has_open = !lua_isnoneornil(L, 2);
    bool open = has_open ? lua_toboolean(L, 2) : true;
    ImGuiTabItemFlags flags = (ImGuiTabItemFlags)luaL_optinteger(L, 3, 0);

    bool selected = has_open
        ? ImGui::BeginTabItem(label, &open, flags)
        : ImGui::BeginTabItem(label, nullptr, flags);

    lua_pushboolean(L, selected);
    lua_pushboolean(L, open);
    return 2;
}

static int l_end_tab_item(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndTabItem();
    }
    return 0;
}

static int l_begin_menu_bar(lua_State* L) {
    bool ok = SimpleUI::instance().has_active_frame() ? ImGui::BeginMenuBar() : false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_end_menu_bar(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndMenuBar();
    }
    return 0;
}

static int l_begin_menu(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    bool enabled = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    lua_pushboolean(L, ImGui::BeginMenu(label, enabled));
    return 1;
}

static int l_end_menu(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndMenu();
    }
    return 0;
}

static int l_menu_item(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    const char* shortcut = luaL_optstring(L, 2, nullptr);
    bool has_selected = !lua_isnoneornil(L, 3);
    bool selected = has_selected ? lua_toboolean(L, 3) : false;
    bool enabled = lua_isboolean(L, 4) ? lua_toboolean(L, 4) : true;

    bool clicked = has_selected
        ? ImGui::MenuItem(label, shortcut, &selected, enabled)
        : ImGui::MenuItem(label, shortcut, false, enabled);

    lua_pushboolean(L, clicked);
    lua_pushboolean(L, selected);
    return 2;
}

// -----------------------------------------------------------------------------
// Widgets & Inputs
// -----------------------------------------------------------------------------
static int l_button(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    float w = (float)luaL_optnumber(L, 2, 0.0);
    float h = (float)luaL_optnumber(L, 3, 0.0);
    bool clicked = ImGui::Button(label, ImVec2(w, h));
    lua_pushboolean(L, clicked);
    return 1;
}

static int l_small_button(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    bool clicked = ImGui::SmallButton(label);
    lua_pushboolean(L, clicked);
    return 1;
}

static int l_invisible_button(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    float w = (float)luaL_checknumber(L, 2);
    float h = (float)luaL_checknumber(L, 3);
    lua_pushboolean(L, ImGui::InvisibleButton(str_id, ImVec2(w, h)));
    return 1;
}

static int l_text(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* txt = luaL_checkstring(L, 1);
    ImGui::TextUnformatted(txt);
    return 0;
}

static int l_text_colored(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    float a = (float)luaL_optnumber(L, 4, 1.0);
    const char* txt = luaL_checkstring(L, 5);
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", txt);
    return 0;
}

static int l_text_disabled(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* txt = luaL_checkstring(L, 1);
    ImGui::TextDisabled("%s", txt);
    return 0;
}

static int l_text_wrapped(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* txt = luaL_checkstring(L, 1);
    ImGui::TextWrapped("%s", txt);
    return 0;
}

static int l_label_text(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* label = luaL_checkstring(L, 1);
    const char* txt = luaL_checkstring(L, 2);
    ImGui::LabelText(label, "%s", txt);
    return 0;
}

static int l_checkbox(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    bool val = lua_toboolean(L, 2);
    bool changed = ImGui::Checkbox(label, &val);
    lua_pushboolean(L, val);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_radio_button(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    bool active = lua_toboolean(L, 2);
    bool clicked = ImGui::RadioButton(label, active);
    lua_pushboolean(L, clicked);
    return 1;
}

static int l_combo(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    int current_idx = (int)luaL_checkinteger(L, 2) - 1;
    luaL_checktype(L, 3, LUA_TTABLE);

    int len = (int)lua_objlen(L, 3);
    std::vector<std::string> items;
    items.reserve(len);
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 3, i);
        items.push_back(lua_tostring(L, -1) ? lua_tostring(L, -1) : "");
        lua_pop(L, 1);
    }

    std::vector<const char*> item_ptrs;
    item_ptrs.reserve(items.size());
    for (const auto& it : items) item_ptrs.push_back(it.c_str());

    bool changed = ImGui::Combo(label, &current_idx, item_ptrs.data(), (int)item_ptrs.size());
    lua_pushinteger(L, current_idx + 1);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_slider_float(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    float min_val = (float)luaL_checknumber(L, 3);
    float max_val = (float)luaL_checknumber(L, 4);
    const char* format = luaL_optstring(L, 5, "%.3f");

    bool changed = ImGui::SliderFloat(label, &val, min_val, max_val, format);
    lua_pushnumber(L, val);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_slider_int(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    int val = (int)luaL_checkinteger(L, 2);
    int min_val = (int)luaL_checkinteger(L, 3);
    int max_val = (int)luaL_checkinteger(L, 4);
    const char* format = luaL_optstring(L, 5, "%d");

    bool changed = ImGui::SliderInt(label, &val, min_val, max_val, format);
    lua_pushinteger(L, val);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_drag_float(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    float speed = (float)luaL_optnumber(L, 3, 1.0f);
    float min_val = (float)luaL_optnumber(L, 4, 0.0f);
    float max_val = (float)luaL_optnumber(L, 5, 0.0f);
    const char* format = luaL_optstring(L, 6, "%.3f");

    bool changed = ImGui::DragFloat(label, &val, speed, min_val, max_val, format);
    lua_pushnumber(L, val);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_drag_int(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    int val = (int)luaL_checkinteger(L, 2);
    float speed = (float)luaL_optnumber(L, 3, 1.0f);
    int min_val = (int)luaL_optinteger(L, 4, 0);
    int max_val = (int)luaL_optinteger(L, 5, 0);
    const char* format = luaL_optstring(L, 6, "%d");

    bool changed = ImGui::DragInt(label, &val, speed, min_val, max_val, format);
    lua_pushinteger(L, val);
    lua_pushboolean(L, changed);
    return 2;
}

static int l_input_text(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    const char* initial_text = luaL_optstring(L, 2, "");
    int max_len = (int)luaL_optinteger(L, 3, 256);
    if (max_len < 16) max_len = 16;
    if (max_len > 4096) max_len = 4096;

    std::vector<char> buffer(max_len, 0);
    strncpy(buffer.data(), initial_text, max_len - 1);
    buffer[max_len - 1] = '\0';

    bool changed = ImGui::InputText(label, buffer.data(), max_len);
    lua_pushstring(L, buffer.data());
    lua_pushboolean(L, changed);
    return 2;
}

static int l_input_text_multiline(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* label = luaL_checkstring(L, 1);
    const char* initial_text = luaL_optstring(L, 2, "");
    float w = (float)luaL_optnumber(L, 3, 0.0);
    float h = (float)luaL_optnumber(L, 4, 0.0);
    int max_len = (int)luaL_optinteger(L, 5, 2048);
    if (max_len < 64) max_len = 64;

    std::vector<char> buffer(max_len, 0);
    strncpy(buffer.data(), initial_text, max_len - 1);
    buffer[max_len - 1] = '\0';

    bool changed = ImGui::InputTextMultiline(label, buffer.data(), max_len, ImVec2(w, h));
    lua_pushstring(L, buffer.data());
    lua_pushboolean(L, changed);
    return 2;
}

static int l_color_edit3(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2); lua_pushvalue(L, 3); lua_pushvalue(L, 4);
        lua_pushboolean(L, false);
        return 4;
    }
    const char* label = luaL_checkstring(L, 1);
    float col[3] = {
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4)
    };
    bool changed = ImGui::ColorEdit3(label, col);
    lua_pushnumber(L, col[0]);
    lua_pushnumber(L, col[1]);
    lua_pushnumber(L, col[2]);
    lua_pushboolean(L, changed);
    return 4;
}

static int l_color_edit4(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushvalue(L, 2); lua_pushvalue(L, 3); lua_pushvalue(L, 4); lua_pushvalue(L, 5);
        lua_pushboolean(L, false);
        return 5;
    }
    const char* label = luaL_checkstring(L, 1);
    float col[4] = {
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4),
        (float)luaL_optnumber(L, 5, 1.0)
    };
    bool changed = ImGui::ColorEdit4(label, col);
    lua_pushnumber(L, col[0]);
    lua_pushnumber(L, col[1]);
    lua_pushnumber(L, col[2]);
    lua_pushnumber(L, col[3]);
    lua_pushboolean(L, changed);
    return 5;
}

static int l_progress_bar(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float fraction = (float)luaL_checknumber(L, 1);
    float w = (float)luaL_optnumber(L, 2, -1.0);
    float h = (float)luaL_optnumber(L, 3, 0.0);
    const char* overlay = luaL_optstring(L, 4, nullptr);
    ImGui::ProgressBar(fraction, ImVec2(w, h), overlay);
    return 0;
}

// -----------------------------------------------------------------------------
// Images & Textures
// -----------------------------------------------------------------------------
static int l_image(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    ImTextureID tex = get_texture_id_from_lua(L, 1);
    float w = (float)luaL_checknumber(L, 2);
    float h = (float)luaL_checknumber(L, 3);

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    if (!lua_isnoneornil(L, 4)) uv0.x = (float)luaL_checknumber(L, 4);
    if (!lua_isnoneornil(L, 5)) uv0.y = (float)luaL_checknumber(L, 5);
    if (!lua_isnoneornil(L, 6)) uv1.x = (float)luaL_checknumber(L, 6);
    if (!lua_isnoneornil(L, 7)) uv1.y = (float)luaL_checknumber(L, 7);

    ImVec4 tint(1, 1, 1, 1);
    if (!lua_isnoneornil(L, 8)) {
        tint.x = (float)luaL_checknumber(L, 8);
        tint.y = (float)luaL_checknumber(L, 9);
        tint.z = (float)luaL_checknumber(L, 10);
        tint.w = (float)luaL_optnumber(L, 11, 1.0);
    }

    ImVec4 border(0, 0, 0, 0);
    if (!lua_isnoneornil(L, 12)) {
        border.x = (float)luaL_checknumber(L, 12);
        border.y = (float)luaL_checknumber(L, 13);
        border.z = (float)luaL_checknumber(L, 14);
        border.w = (float)luaL_optnumber(L, 15, 1.0);
    }

    ImGui::Image(tex, ImVec2(w, h), uv0, uv1, tint, border);
    return 0;
}

static int l_image_button(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    ImTextureID tex = get_texture_id_from_lua(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);

    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    if (!lua_isnoneornil(L, 5)) uv0.x = (float)luaL_checknumber(L, 5);
    if (!lua_isnoneornil(L, 6)) uv0.y = (float)luaL_checknumber(L, 6);
    if (!lua_isnoneornil(L, 7)) uv1.x = (float)luaL_checknumber(L, 7);
    if (!lua_isnoneornil(L, 8)) uv1.y = (float)luaL_checknumber(L, 8);

    ImVec4 bg(0, 0, 0, 0);
    if (!lua_isnoneornil(L, 9)) {
        bg.x = (float)luaL_checknumber(L, 9);
        bg.y = (float)luaL_checknumber(L, 10);
        bg.z = (float)luaL_checknumber(L, 11);
        bg.w = (float)luaL_optnumber(L, 12, 1.0);
    }

    ImVec4 tint(1, 1, 1, 1);
    if (!lua_isnoneornil(L, 13)) {
        tint.x = (float)luaL_checknumber(L, 13);
        tint.y = (float)luaL_checknumber(L, 14);
        tint.z = (float)luaL_checknumber(L, 15);
        tint.w = (float)luaL_optnumber(L, 16, 1.0);
    }

    bool clicked = ImGui::ImageButton(str_id, tex, ImVec2(w, h), uv0, uv1, bg, tint);
    lua_pushboolean(L, clicked);
    return 1;
}

// -----------------------------------------------------------------------------
// Tooltips
// -----------------------------------------------------------------------------
static int l_set_tooltip(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* txt = luaL_checkstring(L, 1);
    ImGui::SetTooltip("%s", txt);
    return 0;
}

static int l_begin_tooltip(lua_State* L) {
    bool ok = SimpleUI::instance().has_active_frame() ? ImGui::BeginTooltip() : false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_end_tooltip(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndTooltip();
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Popups & Modals
// -----------------------------------------------------------------------------
static int l_open_popup(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* str_id = luaL_checkstring(L, 1);
    ImGuiPopupFlags flags = (ImGuiPopupFlags)luaL_optinteger(L, 2, 0);
    ImGui::OpenPopup(str_id, flags);
    return 0;
}

static int l_begin_popup(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, ImGui::BeginPopup(str_id, flags));
    return 1;
}

static int l_begin_popup_modal(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        lua_pushboolean(L, false);
        return 2;
    }
    const char* name = luaL_checkstring(L, 1);
    bool has_open = !lua_isnoneornil(L, 2);
    bool is_open = has_open ? lua_toboolean(L, 2) : true;
    ImGuiWindowFlags flags = (ImGuiWindowFlags)luaL_optinteger(L, 3, 0);

    bool should_draw = has_open
        ? ImGui::BeginPopupModal(name, &is_open, flags)
        : ImGui::BeginPopupModal(name, nullptr, flags);

    lua_pushboolean(L, is_open);
    lua_pushboolean(L, should_draw);
    return 2;
}

static int l_end_popup(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndPopup();
    }
    return 0;
}

static int l_close_current_popup(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::CloseCurrentPopup();
    }
    return 0;
}

static int l_is_popup_open(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    lua_pushboolean(L, ImGui::IsPopupOpen(str_id));
    return 1;
}

// -----------------------------------------------------------------------------
// Tables & Grids
// -----------------------------------------------------------------------------
static int l_begin_table(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* str_id = luaL_checkstring(L, 1);
    int column_count = (int)luaL_checkinteger(L, 2);
    ImGuiTableFlags flags = (ImGuiTableFlags)luaL_optinteger(L, 3, 0);
    float outer_w = (float)luaL_optnumber(L, 4, 0.0);
    float outer_h = (float)luaL_optnumber(L, 5, 0.0);

    lua_pushboolean(L, ImGui::BeginTable(str_id, column_count, flags, ImVec2(outer_w, outer_h)));
    return 1;
}

static int l_end_table(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::EndTable();
    }
    return 0;
}

static int l_table_next_row(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    ImGuiTableRowFlags flags = (ImGuiTableRowFlags)luaL_optinteger(L, 1, 0);
    float min_row_height = (float)luaL_optnumber(L, 2, 0.0);
    ImGui::TableNextRow(flags, min_row_height);
    return 0;
}

static int l_table_next_column(lua_State* L) {
    bool ok = SimpleUI::instance().has_active_frame() ? ImGui::TableNextColumn() : false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_table_set_column_index(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    int col_idx = (int)luaL_checkinteger(L, 1) - 1; // 1-indexed in Lua
    lua_pushboolean(L, ImGui::TableSetColumnIndex(col_idx));
    return 1;
}

static int l_table_setup_column(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* label = luaL_checkstring(L, 1);
    ImGuiTableColumnFlags flags = (ImGuiTableColumnFlags)luaL_optinteger(L, 2, 0);
    float init_width = (float)luaL_optnumber(L, 3, 0.0);
    ImGui::TableSetupColumn(label, flags, init_width);
    return 0;
}

static int l_table_headers_row(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::TableHeadersRow();
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Layout & Styling
// -----------------------------------------------------------------------------
static int l_same_line(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float offset_x = (float)luaL_optnumber(L, 1, 0.0);
    float spacing = (float)luaL_optnumber(L, 2, -1.0);
    ImGui::SameLine(offset_x, spacing);
    return 0;
}

static int l_separator(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::Separator();
    }
    return 0;
}

static int l_separator_text(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    const char* label = luaL_checkstring(L, 1);
    ImGui::SeparatorText(label);
    return 0;
}

static int l_spacing(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::Spacing();
    }
    return 0;
}

static int l_dummy(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float w = (float)luaL_checknumber(L, 1);
    float h = (float)luaL_checknumber(L, 2);
    ImGui::Dummy(ImVec2(w, h));
    return 0;
}

static int l_indent(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float w = (float)luaL_optnumber(L, 1, 0.0);
    ImGui::Indent(w);
    return 0;
}

static int l_unindent(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float w = (float)luaL_optnumber(L, 1, 0.0);
    ImGui::Unindent(w);
    return 0;
}

static int l_push_item_width(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float w = (float)luaL_checknumber(L, 1);
    ImGui::PushItemWidth(w);
    return 0;
}

static int l_pop_item_width(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::PopItemWidth();
    }
    return 0;
}

static int l_push_style_color(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    int idx = (int)luaL_checkinteger(L, 1);
    float r = (float)luaL_checknumber(L, 2);
    float g = (float)luaL_checknumber(L, 3);
    float b = (float)luaL_checknumber(L, 4);
    float a = (float)luaL_optnumber(L, 5, 1.0);
    ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
    return 0;
}

static int l_pop_style_color(lua_State* L) {
    int count = (int)luaL_optinteger(L, 1, 1);
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::PopStyleColor(count);
    }
    return 0;
}

static int l_push_style_var(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    int idx = (int)luaL_checkinteger(L, 1);
    float val = (float)luaL_checknumber(L, 2);
    ImGui::PushStyleVar(idx, val);
    return 0;
}

static int l_push_style_var2(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    int idx = (int)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    ImGui::PushStyleVar(idx, ImVec2(x, y));
    return 0;
}

static int l_pop_style_var(lua_State* L) {
    int count = (int)luaL_optinteger(L, 1, 1);
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::PopStyleVar(count);
    }
    return 0;
}

static int l_collapsing_header(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    ImGuiTreeNodeFlags flags = 0;
    if (lua_toboolean(L, 2)) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    bool open = ImGui::CollapsingHeader(label, flags);
    lua_pushboolean(L, open);
    return 1;
}

static int l_tree_node(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    const char* label = luaL_checkstring(L, 1);
    bool open = ImGui::TreeNode(label);
    lua_pushboolean(L, open);
    return 1;
}

static int l_tree_pop(lua_State* L) {
    (void)L;
    if (SimpleUI::instance().has_active_frame()) {
        ImGui::TreePop();
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Queries & Status
// -----------------------------------------------------------------------------
static int l_is_item_hovered(lua_State* L) {
    bool val = SimpleUI::instance().has_active_frame() ? ImGui::IsItemHovered() : false;
    lua_pushboolean(L, val);
    return 1;
}

static int l_is_item_active(lua_State* L) {
    bool val = SimpleUI::instance().has_active_frame() ? ImGui::IsItemActive() : false;
    lua_pushboolean(L, val);
    return 1;
}

static int l_is_item_focused(lua_State* L) {
    bool val = SimpleUI::instance().has_active_frame() ? ImGui::IsItemFocused() : false;
    lua_pushboolean(L, val);
    return 1;
}

static int l_is_item_clicked(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    int btn = (int)luaL_optinteger(L, 1, 0);
    lua_pushboolean(L, ImGui::IsItemClicked(btn));
    return 1;
}

static int l_is_item_edited(lua_State* L) {
    bool val = SimpleUI::instance().has_active_frame() ? ImGui::IsItemEdited() : false;
    lua_pushboolean(L, val);
    return 1;
}

static int l_is_item_deactivated_after_edit(lua_State* L) {
    bool val = SimpleUI::instance().has_active_frame() ? ImGui::IsItemDeactivatedAfterEdit() : false;
    lua_pushboolean(L, val);
    return 1;
}

static int l_get_item_rect_min(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 p = ImGui::GetItemRectMin();
        lua_pushnumber(L, p.x);
        lua_pushnumber(L, p.y);
        return 2;
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 2;
}

static int l_get_item_rect_max(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 p = ImGui::GetItemRectMax();
        lua_pushnumber(L, p.x);
        lua_pushnumber(L, p.y);
        return 2;
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 2;
}

static int l_get_item_rect_size(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 s = ImGui::GetItemRectSize();
        lua_pushnumber(L, s.x);
        lua_pushnumber(L, s.y);
        return 2;
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 2;
}

static int l_get_cursor_pos(lua_State* L) {
    if (SimpleUI::instance().has_active_frame()) {
        ImVec2 p = ImGui::GetCursorPos();
        lua_pushnumber(L, p.x);
        lua_pushnumber(L, p.y);
        return 2;
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 2;
}

static int l_set_cursor_pos(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) return 0;
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    ImGui::SetCursorPos(ImVec2(x, y));
    return 0;
}

static int l_want_capture_mouse(lua_State* L) {
    lua_pushboolean(L, SimpleUI::instance().want_capture_mouse());
    return 1;
}

static int l_want_capture_keyboard(lua_State* L) {
    lua_pushboolean(L, SimpleUI::instance().want_capture_keyboard());
    return 1;
}

static int l_get_display_size(lua_State* L) {
    if (SimpleUI::instance().is_initialized() || SimpleUI::instance().has_active_frame()) {
        ImVec2 size = ImGui::GetIO().DisplaySize;
        lua_pushnumber(L, size.x);
        lua_pushnumber(L, size.y);
        return 2;
    }
    lua_pushnumber(L, 0.0);
    lua_pushnumber(L, 0.0);
    return 2;
}

static int l_game_viewport(lua_State* L) {
    if (!SimpleUI::instance().has_active_frame()) {
        lua_pushboolean(L, false);
        return 1;
    }
    float w = (float)luaL_optnumber(L, 1, 0.0);
    float h = (float)luaL_optnumber(L, 2, 0.0);
    bool border = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : true;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    bool should_draw = ImGui::BeginChild("GameScreenViewport", ImVec2(w, h), border, flags);
    lua_pushboolean(L, should_draw);
    return 1;
}

// -----------------------------------------------------------------------------
// Registration & Constants
// -----------------------------------------------------------------------------
static const luaL_Reg simple_ui_lib[] = {
    // Windows & Layout
    {"beginWindow",                 l_begin_window},
    {"endWindow",                   l_end_window},
    {"beginChild",                  l_begin_child},
    {"endChild",                    l_end_child},
    {"setNextWindowPos",            l_set_next_window_pos},
    {"setNextWindowSize",           l_set_next_window_size},
    {"setNextWindowBgAlpha",        l_set_next_window_bg_alpha},
    {"setNextWindowFocus",          l_set_next_window_focus},
    {"isWindowHovered",             l_is_window_hovered},
    {"isWindowFocused",             l_is_window_focused},
    {"getWindowPos",                l_get_window_pos},
    {"getWindowSize",               l_get_window_size},

    // Tabs & Menus
    {"beginTabBar",                 l_begin_tab_bar},
    {"endTabBar",                   l_end_tab_bar},
    {"beginTabItem",                l_begin_tab_item},
    {"endTabItem",                  l_end_tab_item},
    {"beginMenuBar",                l_begin_menu_bar},
    {"endMenuBar",                  l_end_menu_bar},
    {"beginMenu",                   l_begin_menu},
    {"endMenu",                     l_end_menu},
    {"menuItem",                    l_menu_item},

    // Widgets & Inputs
    {"button",                      l_button},
    {"smallButton",                 l_small_button},
    {"invisibleButton",             l_invisible_button},
    {"text",                        l_text},
    {"textColored",                 l_text_colored},
    {"textDisabled",                l_text_disabled},
    {"textWrapped",                 l_text_wrapped},
    {"labelText",                   l_label_text},
    {"checkbox",                    l_checkbox},
    {"radioButton",                 l_radio_button},
    {"combo",                       l_combo},
    {"sliderFloat",                 l_slider_float},
    {"sliderInt",                   l_slider_int},
    {"dragFloat",                   l_drag_float},
    {"dragInt",                     l_drag_int},
    {"inputText",                   l_input_text},
    {"inputTextMultiline",          l_input_text_multiline},
    {"colorEdit3",                  l_color_edit3},
    {"colorEdit4",                  l_color_edit4},
    {"progressBar",                 l_progress_bar},

    // Images & Icons
    {"image",                       l_image},
    {"imageButton",                 l_image_button},

    // Tooltips
    {"setTooltip",                  l_set_tooltip},
    {"beginTooltip",                l_begin_tooltip},
    {"endTooltip",                  l_end_tooltip},

    // Popups & Modals
    {"openPopup",                   l_open_popup},
    {"beginPopup",                  l_begin_popup},
    {"beginPopupModal",             l_begin_popup_modal},
    {"endPopup",                    l_end_popup},
    {"closeCurrentPopup",           l_close_current_popup},
    {"isPopupOpen",                 l_is_popup_open},

    // Tables & Grids
    {"beginTable",                  l_begin_table},
    {"endTable",                    l_end_table},
    {"tableNextRow",                l_table_next_row},
    {"tableNextColumn",             l_table_next_column},
    {"tableSetColumnIndex",         l_table_set_column_index},
    {"tableSetupColumn",            l_table_setup_column},
    {"tableHeadersRow",             l_table_headers_row},

    // Formatting & Stacking
    {"sameLine",                    l_same_line},
    {"separator",                   l_separator},
    {"separatorText",               l_separator_text},
    {"spacing",                     l_spacing},
    {"dummy",                       l_dummy},
    {"indent",                      l_indent},
    {"unindent",                    l_unindent},
    {"pushItemWidth",               l_push_item_width},
    {"popItemWidth",                l_pop_item_width},
    {"pushStyleColor",              l_push_style_color},
    {"popStyleColor",               l_pop_style_color},
    {"pushStyleVar",                l_push_style_var},
    {"pushStyleVar2",               l_push_style_var2},
    {"popStyleVar",                 l_pop_style_var},
    {"collapsingHeader",            l_collapsing_header},
    {"treeNode",                    l_tree_node},
    {"treePop",                     l_tree_pop},

    // Status Queries
    {"isItemHovered",               l_is_item_hovered},
    {"isItemActive",                l_is_item_active},
    {"isItemFocused",               l_is_item_focused},
    {"isItemClicked",               l_is_item_clicked},
    {"isItemEdited",                l_is_item_edited},
    {"isItemDeactivatedAfterEdit",  l_is_item_deactivated_after_edit},
    {"getItemRectMin",              l_get_item_rect_min},
    {"getItemRectMax",              l_get_item_rect_max},
    {"getItemRectSize",             l_get_item_rect_size},
    {"getCursorPos",                l_get_cursor_pos},
    {"setCursorPos",                l_set_cursor_pos},

    // Engine Integration
    {"wantCaptureMouse",            l_want_capture_mouse},
    {"wantCaptureKeyboard",         l_want_capture_keyboard},
    {"getDisplaySize",              l_get_display_size},
    {"gameViewport",                l_game_viewport},

    {nullptr, nullptr}
};

static void set_table_int(lua_State* L, const char* key, int val) {
    lua_pushinteger(L, val);
    lua_setfield(L, -2, key);
}

int simple_ui_lua_register(lua_State* L) {
    // Register strictly on crayon.simpleUI
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "crayon");
    }

    lua_newtable(L);
    for (const luaL_Reg* l = simple_ui_lib; l->name; ++l) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }

    // WindowFlags constant table
    lua_newtable(L);
    set_table_int(L, "None",                ImGuiWindowFlags_None);
    set_table_int(L, "NoTitleBar",          ImGuiWindowFlags_NoTitleBar);
    set_table_int(L, "NoResize",            ImGuiWindowFlags_NoResize);
    set_table_int(L, "NoMove",              ImGuiWindowFlags_NoMove);
    set_table_int(L, "NoScrollbar",         ImGuiWindowFlags_NoScrollbar);
    set_table_int(L, "NoScrollWithMouse",   ImGuiWindowFlags_NoScrollWithMouse);
    set_table_int(L, "NoCollapse",          ImGuiWindowFlags_NoCollapse);
    set_table_int(L, "AlwaysAutoResize",    ImGuiWindowFlags_AlwaysAutoResize);
    set_table_int(L, "NoBackground",        ImGuiWindowFlags_NoBackground);
    set_table_int(L, "NoSavedSettings",     ImGuiWindowFlags_NoSavedSettings);
    set_table_int(L, "NoMouseInputs",       ImGuiWindowFlags_NoMouseInputs);
    set_table_int(L, "MenuBar",             ImGuiWindowFlags_MenuBar);
    set_table_int(L, "NoFocusOnAppearing",  ImGuiWindowFlags_NoFocusOnAppearing);
    lua_setfield(L, -2, "WindowFlags");

    // TableFlags constant table
    lua_newtable(L);
    set_table_int(L, "None",                ImGuiTableFlags_None);
    set_table_int(L, "Resizable",           ImGuiTableFlags_Resizable);
    set_table_int(L, "Reorderable",         ImGuiTableFlags_Reorderable);
    set_table_int(L, "Hideable",            ImGuiTableFlags_Hideable);
    set_table_int(L, "Sortable",            ImGuiTableFlags_Sortable);
    set_table_int(L, "RowBg",               ImGuiTableFlags_RowBg);
    set_table_int(L, "BordersInnerH",       ImGuiTableFlags_BordersInnerH);
    set_table_int(L, "BordersOuterH",       ImGuiTableFlags_BordersOuterH);
    set_table_int(L, "BordersInnerV",       ImGuiTableFlags_BordersInnerV);
    set_table_int(L, "BordersOuterV",       ImGuiTableFlags_BordersOuterV);
    set_table_int(L, "BordersH",            ImGuiTableFlags_BordersH);
    set_table_int(L, "BordersV",            ImGuiTableFlags_BordersV);
    set_table_int(L, "Borders",             ImGuiTableFlags_Borders);
    lua_setfield(L, -2, "TableFlags");

    lua_setfield(L, -2, "simpleUI");

    lua_pop(L, 1); // pop crayon table
    return 0;
}
