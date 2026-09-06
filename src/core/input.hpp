#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <SDL3/SDL.h>

namespace crayon {

class Window;

class Input {
public:
    Input();
    ~Input();

    void begin_frame();
    void handle_event(const SDL_Event& event, const Window& window);

    bool is_key_down(const std::string& key) const;
    bool is_key_pressed(const std::string& key) const;
    bool is_key_released(const std::string& key) const;

    bool any_key_pressed() const { return !m_keys_pressed.empty(); }
    const std::unordered_set<std::string>& get_pressed_keys() const { return m_keys_pressed; }

    // Key Modifiers
    bool is_shift_down() const;
    bool is_ctrl_down() const;
    bool is_alt_down() const;
    bool is_gui_down() const;
    bool is_caps_lock() const;

    // Mouse coordinates & events
    void get_mouse_pos(float& x, float& y) const { x = m_mouse_virt_x; y = m_mouse_virt_y; }
    void get_mouse_window_pos(float& x, float& y) const { x = m_mouse_win_x; y = m_mouse_win_y; }
    void get_mouse_delta(float& dx, float& dy) const { dx = m_mouse_delta_x; dy = m_mouse_delta_y; }

    bool is_mouse_down(int button) const;
    bool is_mouse_pressed(int button) const;
    bool is_mouse_released(int button) const;

    bool is_mouse_down(const std::string& name) const;
    bool is_mouse_pressed(const std::string& name) const;
    bool is_mouse_released(const std::string& name) const;

    float get_mouse_wheel() const { return m_mouse_wheel_y; }
    float get_mouse_wheel_x() const { return m_mouse_wheel_x; }
    float get_mouse_wheel_y() const { return m_mouse_wheel_y; }

    void set_mouse_position(const Window& window, float win_x, float win_y);

    // Text Input & Clipboard
    void start_text_input(const Window& window);
    void stop_text_input(const Window& window);
    bool is_text_input_active(const Window& window) const;
    const std::string& get_text_input() const { return m_text_input; }

    std::string get_clipboard_text() const;
    void set_clipboard_text(const std::string& text);

    static int parse_mouse_button(const std::string& name);

    bool gamepad_is_down(int button) const;
    float gamepad_axis(int axis) const;

private:
    std::string normalize_key(const std::string& name) const;

    std::unordered_set<std::string> m_keys_down;
    std::unordered_set<std::string> m_keys_pressed;
    std::unordered_set<std::string> m_keys_released;

    std::string m_text_input;

    float m_mouse_win_x = 0.0f;
    float m_mouse_win_y = 0.0f;
    float m_mouse_virt_x = 0.0f;
    float m_mouse_virt_y = 0.0f;
    float m_mouse_delta_x = 0.0f;
    float m_mouse_delta_y = 0.0f;
    float m_mouse_wheel_x = 0.0f;
    float m_mouse_wheel_y = 0.0f;

    bool m_mouse_down[8] = {false};
    bool m_mouse_pressed[8] = {false};
    bool m_mouse_released[8] = {false};

    SDL_Gamepad* m_gamepad = nullptr;
    bool m_gamepad_buttons[32] = {false};
    float m_gamepad_axes[8] = {0.0f};
};

} // namespace crayon
