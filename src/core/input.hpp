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

    void get_mouse_pos(float& x, float& y) const { x = m_mouse_virt_x; y = m_mouse_virt_y; }
    void get_mouse_delta(float& dx, float& dy) const { dx = m_mouse_delta_x; dy = m_mouse_delta_y; }
    bool is_mouse_down(int button) const;
    bool is_mouse_pressed(int button) const;
    bool is_mouse_released(int button) const;
    float get_mouse_wheel() const { return m_mouse_wheel; }

    bool gamepad_is_down(int button) const;
    float gamepad_axis(int axis) const;

private:
    std::string normalize_key(const std::string& name) const;

    std::unordered_set<std::string> m_keys_down;
    std::unordered_set<std::string> m_keys_pressed;
    std::unordered_set<std::string> m_keys_released;

    float m_mouse_win_x = 0.0f;
    float m_mouse_win_y = 0.0f;
    float m_mouse_virt_x = 0.0f;
    float m_mouse_virt_y = 0.0f;
    float m_mouse_delta_x = 0.0f;
    float m_mouse_delta_y = 0.0f;
    float m_mouse_wheel = 0.0f;

    bool m_mouse_down[8] = {false};
    bool m_mouse_pressed[8] = {false};
    bool m_mouse_released[8] = {false};

    SDL_Gamepad* m_gamepad = nullptr;
    bool m_gamepad_buttons[32] = {false};
    float m_gamepad_axes[8] = {0.0f};
};

} // namespace crayon
