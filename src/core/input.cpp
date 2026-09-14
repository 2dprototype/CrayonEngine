#include "input.hpp"
#include "window.hpp"
#include "log.hpp"
#include <algorithm>
#include <cctype>

namespace crayon {

static int sdl_button_to_love(uint8_t btn) {
    if (btn == SDL_BUTTON_LEFT) return 1;
    if (btn == SDL_BUTTON_RIGHT) return 2;
    if (btn == SDL_BUTTON_MIDDLE) return 3;
    if (btn == SDL_BUTTON_X1) return 4;
    if (btn == SDL_BUTTON_X2) return 5;
    return static_cast<int>(btn);
}

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
    for (int i = 0; i < 32; ++i) {
        m_gamepad_buttons_pressed[i] = false;
        m_gamepad_buttons_released[i] = false;
    }
    m_mouse_wheel_x = 0.0f;
    m_mouse_wheel_y = 0.0f;
    m_mouse_delta_win_x = 0.0f;
    m_mouse_delta_win_y = 0.0f;
    m_mouse_delta_virt_x = 0.0f;
    m_mouse_delta_virt_y = 0.0f;
    m_text_input.clear();
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
    if (result == "esc") result = "escape";
    if (result == "left_shift" || result == "lshift") result = "lshift";
    if (result == "right_shift" || result == "rshift") result = "rshift";
    if (result == "left_ctrl" || result == "lctrl" || result == "left_control") result = "lctrl";
    if (result == "right_ctrl" || result == "rctrl" || result == "right_control") result = "rctrl";
    if (result == "left_alt" || result == "lalt") result = "lalt";
    if (result == "right_alt" || result == "ralt") result = "ralt";
    if (result == "left_gui" || result == "lgui" || result == "left_windows" || result == "lsuper") result = "lgui";
    if (result == "right_gui" || result == "rgui" || result == "right_windows" || result == "rsuper") result = "rgui";
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
                if (k == "lshift" || k == "rshift") {
                    m_keys_down.insert("shift");
                    m_keys_pressed.insert("shift");
                } else if (k == "lctrl" || k == "rctrl") {
                    m_keys_down.insert("ctrl");
                    m_keys_pressed.insert("ctrl");
                } else if (k == "lalt" || k == "ralt") {
                    m_keys_down.insert("alt");
                    m_keys_pressed.insert("alt");
                } else if (k == "lgui" || k == "rgui") {
                    m_keys_down.insert("gui");
                    m_keys_pressed.insert("gui");
                }
            }
        }
    } else if (event.type == SDL_EVENT_KEY_UP) {
        const char* name = SDL_GetKeyName(event.key.key);
        if (name && name[0] != '\0') {
            std::string k = normalize_key(name);
            m_keys_down.erase(k);
            m_keys_released.insert(k);
            if (k == "lshift" || k == "rshift") {
                if (!m_keys_down.contains("lshift") && !m_keys_down.contains("rshift")) {
                    m_keys_down.erase("shift");
                }
                m_keys_released.insert("shift");
            } else if (k == "lctrl" || k == "rctrl") {
                if (!m_keys_down.contains("lctrl") && !m_keys_down.contains("rctrl")) {
                    m_keys_down.erase("ctrl");
                }
                m_keys_released.insert("ctrl");
            } else if (k == "lalt" || k == "ralt") {
                if (!m_keys_down.contains("lalt") && !m_keys_down.contains("ralt")) {
                    m_keys_down.erase("alt");
                }
                m_keys_released.insert("alt");
            } else if (k == "lgui" || k == "rgui") {
                if (!m_keys_down.contains("lgui") && !m_keys_down.contains("rgui")) {
                    m_keys_down.erase("gui");
                }
                m_keys_released.insert("gui");
            }
        }
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
        float prev_virt_x = m_mouse_virt_x;
        float prev_virt_y = m_mouse_virt_y;
        m_mouse_win_x = event.motion.x;
        m_mouse_win_y = event.motion.y;
        m_mouse_delta_win_x += event.motion.xrel;
        m_mouse_delta_win_y += event.motion.yrel;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
        m_mouse_delta_virt_x += (m_mouse_virt_x - prev_virt_x);
        m_mouse_delta_virt_y += (m_mouse_virt_y - prev_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int btn = sdl_button_to_love(event.button.button);
        if (btn >= 0 && btn < 8) {
            m_mouse_down[btn] = true;
            m_mouse_pressed[btn] = true;
        }
        m_mouse_win_x = event.button.x;
        m_mouse_win_y = event.button.y;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        int btn = sdl_button_to_love(event.button.button);
        if (btn >= 0 && btn < 8) {
            m_mouse_down[btn] = false;
            m_mouse_released[btn] = true;
        }
        m_mouse_win_x = event.button.x;
        m_mouse_win_y = event.button.y;
        window.window_to_virtual(m_mouse_win_x, m_mouse_win_y, m_mouse_virt_x, m_mouse_virt_y);
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        m_mouse_wheel_x = event.wheel.x;
        m_mouse_wheel_y = event.wheel.y;
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            m_mouse_wheel_x = -m_mouse_wheel_x;
            m_mouse_wheel_y = -m_mouse_wheel_y;
        }
    } else if (event.type == SDL_EVENT_TEXT_INPUT) {
        if (event.text.text) {
            m_text_input += event.text.text;
        }
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
            m_gamepad_buttons_down[event.gbutton.button] = true;
            m_gamepad_buttons_pressed[event.gbutton.button] = true;
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (event.gbutton.button < 32) {
            m_gamepad_buttons_down[event.gbutton.button] = false;
            m_gamepad_buttons_released[event.gbutton.button] = true;
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

bool Input::is_scancode_down(const std::string& scancode) const {
    SDL_Scancode code = SDL_GetScancodeFromName(scancode.c_str());
    if (code == SDL_SCANCODE_UNKNOWN) return false;
    int numkeys = 0;
    const bool* state = SDL_GetKeyboardState(&numkeys);
    if (state && code < numkeys) {
        return state[code];
    }
    return false;
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

int Input::parse_mouse_button(const std::string& name) {
    std::string s = name;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s == "1" || s == "left" || s == "l" || s == "primary") return 1;
    if (s == "2" || s == "right" || s == "r" || s == "secondary") return 2;
    if (s == "3" || s == "middle" || s == "mid" || s == "m") return 3;
    if (s == "4" || s == "x1" || s == "mouse4") return 4;
    if (s == "5" || s == "x2" || s == "mouse5") return 5;
    return 1;
}

std::string Input::mouse_button_to_string(int button) {
    switch (button) {
        case 1: return "left";
        case 2: return "right";
        case 3: return "middle";
        case 4: return "x1";
        case 5: return "x2";
        default: return "unknown";
    }
}

bool Input::is_mouse_down(const std::string& name) const {
    return is_mouse_down(parse_mouse_button(name));
}

bool Input::is_mouse_pressed(const std::string& name) const {
    return is_mouse_pressed(parse_mouse_button(name));
}

bool Input::is_mouse_released(const std::string& name) const {
    return is_mouse_released(parse_mouse_button(name));
}

void Input::set_mouse_position(const Window& window, float virt_x, float virt_y) {
    if (window.get_sdl_window()) {
        float win_x = 0.0f, win_y = 0.0f;
        window.virtual_to_window(virt_x, virt_y, win_x, win_y);
        SDL_WarpMouseInWindow(window.get_sdl_window(), win_x, win_y);
    }
}

void Input::set_mouse_window_position(const Window& window, float win_x, float win_y) {
    if (window.get_sdl_window()) {
        SDL_WarpMouseInWindow(window.get_sdl_window(), win_x, win_y);
    }
}

bool Input::is_shift_down() const {
    return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
}

bool Input::is_ctrl_down() const {
    return (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
}

bool Input::is_alt_down() const {
    return (SDL_GetModState() & SDL_KMOD_ALT) != 0;
}

bool Input::is_gui_down() const {
    return (SDL_GetModState() & SDL_KMOD_GUI) != 0;
}

bool Input::is_caps_lock() const {
    return (SDL_GetModState() & SDL_KMOD_CAPS) != 0;
}

void Input::start_text_input(const Window& window) {
    if (window.get_sdl_window()) {
        SDL_StartTextInput(window.get_sdl_window());
    }
}

void Input::stop_text_input(const Window& window) {
    if (window.get_sdl_window()) {
        SDL_StopTextInput(window.get_sdl_window());
    }
}

bool Input::is_text_input_active(const Window& window) const {
    if (window.get_sdl_window()) {
        return SDL_TextInputActive(window.get_sdl_window());
    }
    return false;
}

std::string Input::get_clipboard_text() const {
    char* text = SDL_GetClipboardText();
    if (text) {
        std::string str(text);
        SDL_free(text);
        return str;
    }
    return "";
}

void Input::set_clipboard_text(const std::string& text) {
    SDL_SetClipboardText(text.c_str());
}

bool Input::gamepad_is_down(int button) const {
    if (button >= 0 && button < 32) {
        return m_gamepad_buttons_down[button];
    }
    return false;
}

bool Input::gamepad_is_pressed(int button) const {
    if (button >= 0 && button < 32) {
        return m_gamepad_buttons_pressed[button];
    }
    return false;
}

bool Input::gamepad_is_released(int button) const {
    if (button >= 0 && button < 32) {
        return m_gamepad_buttons_released[button];
    }
    return false;
}

int Input::parse_gamepad_button(const std::string& name) {
    if (name.empty()) return -1;
    if (std::isdigit(static_cast<unsigned char>(name[0]))) {
        return std::stoi(name);
    }
    return SDL_GetGamepadButtonFromString(name.c_str());
}

int Input::parse_gamepad_axis(const std::string& name) {
    if (name.empty()) return -1;
    if (std::isdigit(static_cast<unsigned char>(name[0]))) {
        return std::stoi(name);
    }
    return SDL_GetGamepadAxisFromString(name.c_str());
}

bool Input::gamepad_is_down(const std::string& name) const {
    int btn = parse_gamepad_button(name);
    return gamepad_is_down(btn);
}

bool Input::gamepad_is_pressed(const std::string& name) const {
    int btn = parse_gamepad_button(name);
    return gamepad_is_pressed(btn);
}

bool Input::gamepad_is_released(const std::string& name) const {
    int btn = parse_gamepad_button(name);
    return gamepad_is_released(btn);
}

float Input::gamepad_axis(int axis) const {
    if (axis >= 0 && axis < 8) {
        return m_gamepad_axes[axis];
    }
    return 0.0f;
}

float Input::gamepad_axis(const std::string& name) const {
    int ax = parse_gamepad_axis(name);
    return gamepad_axis(ax);
}

bool Input::gamepad_is_connected(int index) const {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    bool connected = (ids != nullptr && index >= 0 && index < count);
    if (ids) SDL_free(ids);
    return connected;
}

std::string Input::gamepad_get_name(int index) const {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::string name = "";
    if (ids && index >= 0 && index < count) {
        const char* n = SDL_GetGamepadNameForID(ids[index]);
        if (n) name = n;
    }
    if (ids) SDL_free(ids);
    return name;
}

int Input::gamepad_get_count() const {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids) SDL_free(ids);
    return count;
}

} // namespace crayon
