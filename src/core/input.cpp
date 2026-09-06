#include "input.hpp"
#include "window.hpp"
#include "log.hpp"
#include <algorithm>
#include <cctype>

namespace crayon {

Input::Input() {
    int num_joysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetGamepads(&num_joysticks);
    if (joysticks && num_joysticks > 0) {
        m_gamepad = SDL_OpenGamepad(joysticks[0]);
        if (m_gamepad) {
            CRAYON_LOG_INFO("Opened gamepad: {}", SDL_GetGamepadName(m_gamepad));
        }
        SDL_free(joysticks);
    }
}

Input::~Input() {
    if (m_gamepad) {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
}

void Input::begin_frame() {
    m_keys_pressed.clear();
    m_keys_released.clear();
    for (int i = 0; i < 8; ++i) {
        m_mouse_pressed[i] = false;
        m_mouse_released[i] = false;
    }
    m_mouse_wheel = 0.0f;
    m_mouse_delta_x = 0.0f;
    m_mouse_delta_y = 0.0f;
}

std::string Input::normalize_key(const std::string& name) const {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (c == ' ') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (result == "return") result = "enter";
    return result;
}

void Input::handle_event(const SDL_Event& event, const Window& window) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (!event.key.repeat) {
            const char* name = SDL_GetKeyName(event.key.key);
            if (name && name[0] != '\0') {
                std::string k = normalize_key(name);
                m_keys_down.insert(k);
                m_keys_pressed.insert(k);
            }
        }
    } else if (event.type == SDL_EVENT_KEY_UP) {
        const char* name = SDL_GetKeyName(event.key.key);
        if (name && name[0] != '\0') {
            std::string k = normalize_key(name);
            m_keys_down.erase(k);
            m_keys_released.insert(k);
        }
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        m_mouse_win_x = event.motion.x;
        m_mouse_win_y = event.motion.y;
        m_mouse_delta_x += event.motion.xrel;
        m_mouse_delta_y += event.motion.yrel;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        uint8_t btn = event.button.button;
        if (btn < 8) {
            m_mouse_down[btn] = true;
            m_mouse_pressed[btn] = true;
        }
        m_mouse_win_x = event.button.x;
        m_mouse_win_y = event.button.y;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        uint8_t btn = event.button.button;
        if (btn < 8) {
            m_mouse_down[btn] = false;
            m_mouse_released[btn] = true;
        }
        m_mouse_win_x = event.button.x;
        m_mouse_win_y = event.button.y;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        m_mouse_wheel = event.wheel.y;
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (!m_gamepad) {
            m_gamepad = SDL_OpenGamepad(event.gdevice.which);
            if (m_gamepad) {
                CRAYON_LOG_INFO("Gamepad connected: {}", SDL_GetGamepadName(m_gamepad));
            }
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (m_gamepad && SDL_GetGamepadID(m_gamepad) == event.gdevice.which) {
            CRAYON_LOG_INFO("Gamepad disconnected");
            SDL_CloseGamepad(m_gamepad);
            m_gamepad = nullptr;
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (event.gbutton.button < 32) {
            m_gamepad_buttons[event.gbutton.button] = true;
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (event.gbutton.button < 32) {
            m_gamepad_buttons[event.gbutton.button] = false;
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        if (event.gaxis.axis < 8) {
            m_gamepad_axes[event.gaxis.axis] = static_cast<float>(event.gaxis.value) / 32767.0f;
        }
    }
}

bool Input::is_key_down(const std::string& key) const {
    return m_keys_down.contains(normalize_key(key));
}

bool Input::is_key_pressed(const std::string& key) const {
    return m_keys_pressed.contains(normalize_key(key));
}

bool Input::is_key_released(const std::string& key) const {
    return m_keys_released.contains(normalize_key(key));
}

bool Input::is_mouse_down(int button) const {
    if (button >= 0 && button < 8) {
        return m_mouse_down[button];
    }
    return false;
}

bool Input::is_mouse_pressed(int button) const {
    if (button >= 0 && button < 8) {
        return m_mouse_pressed[button];
    }
    return false;
}

bool Input::is_mouse_released(int button) const {
    if (button >= 0 && button < 8) {
        return m_mouse_released[button];
    }
    return false;
}

bool Input::gamepad_is_down(int button) const {
    if (button >= 0 && button < 32) {
        return m_gamepad_buttons[button];
    }
    return false;
}

float Input::gamepad_axis(int axis) const {
    if (axis >= 0 && axis < 8) {
        return m_gamepad_axes[axis];
    }
    return 0.0f;
}

} // namespace crayon
