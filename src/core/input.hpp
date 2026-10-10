#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <string_view>
#include <SDL3/SDL.h>

namespace crayon {

class Window;

class Input {
public:
    Input();
    ~Input();

    void begin_frame();
    void handle_event(const SDL_Event& event, const Window& window);

    // Keyboard
    bool is_key_down(const std::string& key) const;
    bool is_key_pressed(const std::string& key) const;
    bool is_key_released(const std::string& key) const;
    bool is_scancode_down(const std::string& scancode) const;

    bool any_key_pressed() const { return !m_keys_pressed.empty(); }
    bool any_key_down() const { return !m_keys_down.empty(); }
    const std::unordered_set<std::string>& get_pressed_keys() const { return m_keys_pressed; }
    const std::unordered_set<std::string>& get_down_keys() const { return m_keys_down; }

    // Key Modifiers
    bool is_shift_down() const;
    bool is_ctrl_down() const;
    bool is_alt_down() const;
    bool is_gui_down() const;
    bool is_caps_lock() const;

    // Mouse coordinates (Virtual Canvas space)
    void get_mouse_pos(float& x, float& y) const { x = m_mouse_virt_x; y = m_mouse_virt_y; }
    float get_mouse_x() const { return m_mouse_virt_x; }
    float get_mouse_y() const { return m_mouse_virt_y; }
    void get_mouse_delta(float& dx, float& dy) const { dx = m_mouse_delta_virt_x; dy = m_mouse_delta_virt_y; }
    float get_mouse_delta_x() const { return m_mouse_delta_virt_x; }
    float get_mouse_delta_y() const { return m_mouse_delta_virt_y; }

    // Mouse coordinates (Physical Window space)
    void get_mouse_window_pos(float& x, float& y) const { x = m_mouse_win_x; y = m_mouse_win_y; }
    float get_mouse_window_x() const { return m_mouse_win_x; }
    float get_mouse_window_y() const { return m_mouse_win_y; }
    void get_mouse_window_delta(float& dx, float& dy) const { dx = m_mouse_delta_win_x; dy = m_mouse_delta_win_y; }

    // Mouse buttons: 1=Left, 2=Right, 3=Middle, 4=X1, 5=X2
    bool is_mouse_down(int button) const;
    bool is_mouse_pressed(int button) const;
    bool is_mouse_released(int button) const;

    bool is_mouse_down(const std::string& name) const;
    bool is_mouse_pressed(const std::string& name) const;
    bool is_mouse_released(const std::string& name) const;

    float get_mouse_wheel_x() const { return m_mouse_wheel_x; }
    float get_mouse_wheel_y() const { return m_mouse_wheel_y; }

    void set_mouse_position(const Window& window, float virt_x, float virt_y);
    void set_mouse_window_position(const Window& window, float win_x, float win_y);

    // Touch (multi-touch). Coordinates are in Virtual Canvas space.
    // SDL also synthesises mouse events from the first finger, so mouse-only
    // games already work on a touchscreen; this exposes the real fingers.
    struct Touch {
        int64_t id = 0;       // stable for the lifetime of the finger
        float x = 0.0f;       // virtual-canvas coordinates
        float y = 0.0f;
        float pressure = 1.0f;
    };
    const std::vector<Touch>& get_touches() const { return m_touches; }
    int get_touch_count() const { return static_cast<int>(m_touches.size()); }

    // Text Input & Clipboard
    void start_text_input(const Window& window);
    void stop_text_input(const Window& window);
    bool is_text_input_active(const Window& window) const;
    const std::string& get_text_input() const { return m_text_input; }

    std::string get_clipboard_text() const;
    void set_clipboard_text(const std::string& text);

    static int parse_mouse_button(const std::string& name);
    static std::string mouse_button_to_string(int button);

    // Gamepad
    bool gamepad_is_down(int button) const;
    bool gamepad_is_pressed(int button) const;
    bool gamepad_is_released(int button) const;
    bool gamepad_is_down(const std::string& name) const;
    bool gamepad_is_pressed(const std::string& name) const;
    bool gamepad_is_released(const std::string& name) const;

    float gamepad_axis(int axis) const;
    float gamepad_axis(const std::string& name) const;

    bool gamepad_is_connected(int index) const;
    std::string gamepad_get_name(int index) const;
    int gamepad_get_count() const;

    static int parse_gamepad_button(const std::string& name);
    static int parse_gamepad_axis(const std::string& name);

    std::string normalize_key(const std::string& name) const;

private:
    // Memoised normalize_key(): script code polls the same few key names every
    // frame, so after the first call a lookup is one hash and zero allocations.
    const std::string& normalized(const std::string& name) const;

    struct NameHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };
    mutable std::unordered_map<std::string, std::string, NameHash, std::equal_to<>> m_norm_cache;

    std::unordered_set<std::string> m_keys_down;
    std::unordered_set<std::string> m_keys_pressed;
    std::unordered_set<std::string> m_keys_released;

    std::string m_text_input;
    std::vector<Touch> m_touches;   // currently-down fingers (persist across frames)

    float m_mouse_win_x = 0.0f;
    float m_mouse_win_y = 0.0f;
    float m_mouse_virt_x = 0.0f;
    float m_mouse_virt_y = 0.0f;
    float m_mouse_delta_win_x = 0.0f;
    float m_mouse_delta_win_y = 0.0f;
    float m_mouse_delta_virt_x = 0.0f;
    float m_mouse_delta_virt_y = 0.0f;
    float m_mouse_wheel_x = 0.0f;
    float m_mouse_wheel_y = 0.0f;

    bool m_mouse_down[8] = {false};
    bool m_mouse_pressed[8] = {false};
    bool m_mouse_released[8] = {false};

    SDL_Gamepad* m_gamepad = nullptr;
    bool m_gamepad_buttons_down[32] = {false};
    bool m_gamepad_buttons_pressed[32] = {false};
    bool m_gamepad_buttons_released[32] = {false};
    float m_gamepad_axes[8] = {0.0f};
};

} // namespace crayon
