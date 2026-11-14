// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/sly_input.h"

namespace sly1::platform {
namespace {
using openrac::platform::Button;
}

Input::Input() = default;

Input::~Input() {
    for (SDL_Gamepad* gamepad : m_gamepads) {
        if (gamepad != nullptr) SDL_CloseGamepad(gamepad);
    }
}

void Input::handle_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        for (SDL_Gamepad*& slot : m_gamepads) {
            if (slot == nullptr) {
                slot = SDL_OpenGamepad(event.gdevice.which);
                if (slot != nullptr) break;
            }
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        for (std::size_t port = 0; port < m_gamepads.size(); ++port) {
            SDL_Gamepad* gamepad = m_gamepads[port];
            if (gamepad != nullptr && SDL_GetGamepadID(gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad);
                m_gamepads[port] = nullptr;
                m_pads[port] = {};
            }
        }
    }
}

void Input::update() {
    SDL_UpdateGamepads();
    for (int port = 0; port < 2; ++port) refresh_pad(port);
}

void Input::test_press_cross_once() {
    m_test_cross_pending = true;
}

const openrac::platform::Pad& Input::pad(int port) const {
    static const openrac::platform::Pad disconnected{};
    return port >= 0 && port < 2 ? m_pads[static_cast<std::size_t>(port)] : disconnected;
}

void Input::refresh_pad(int port) {
    using namespace openrac::platform;
    Pad& pad = m_pads[static_cast<std::size_t>(port)];
    pad = {};
    SDL_Gamepad* gamepad = m_gamepads[static_cast<std::size_t>(port)];
    pad.connected = port == 0 || gamepad != nullptr;
    if (gamepad != nullptr) {
        constexpr SDL_GamepadButton buttons[] = {
            SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_SOUTH,
            SDL_GAMEPAD_BUTTON_WEST,
        };
        constexpr Button mapped[] = {
            Button::Select, Button::L3, Button::R3, Button::Start, Button::Up, Button::Right,
            Button::Down, Button::Left, Button::L1, Button::R1, Button::Triangle, Button::Circle,
            Button::Cross, Button::Square,
        };
        for (std::size_t i = 0; i < std::size(buttons); ++i)
            pad.set(mapped[i], SDL_GetGamepadButton(gamepad, buttons[i]));
        const auto lx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        const auto ly = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        const auto rx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
        const auto ry = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
        pad.left_x = stick_byte(lx); pad.left_y = stick_byte(ly);
        pad.right_x = stick_byte(rx); pad.right_y = stick_byte(ry);
        pad.set(Button::L2, SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 8000);
        pad.set(Button::R2, SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000);
        if (port == 0 && m_test_cross_pending) {
            pad.set(Button::Cross, true);
            m_test_cross_pending = false;
        }
        return;
    }

    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    const auto down = [&](SDL_Scancode key) {
        return SDL_GetKeyboardFocus() != nullptr && static_cast<int>(key) < count && keys[key];
    };
    pad.set(Button::Up, down(SDL_SCANCODE_UP));
    pad.set(Button::Right, down(SDL_SCANCODE_RIGHT));
    pad.set(Button::Down, down(SDL_SCANCODE_DOWN));
    pad.set(Button::Left, down(SDL_SCANCODE_LEFT));
    pad.set(Button::Cross, down(SDL_SCANCODE_SPACE));
    pad.set(Button::Square, down(SDL_SCANCODE_F));
    pad.set(Button::Circle, down(SDL_SCANCODE_E));
    pad.set(Button::Triangle, down(SDL_SCANCODE_R));
    pad.set(Button::L1, down(SDL_SCANCODE_LSHIFT));
    pad.set(Button::R1, down(SDL_SCANCODE_Q));
    pad.set(Button::L2, down(SDL_SCANCODE_1));
    pad.set(Button::R2, down(SDL_SCANCODE_3));
    pad.set(Button::L3, down(SDL_SCANCODE_C));
    pad.set(Button::R3, down(SDL_SCANCODE_V));
    pad.set(Button::Start, down(SDL_SCANCODE_RETURN));
    pad.set(Button::Select, down(SDL_SCANCODE_BACKSPACE));
    pad.left_x = static_cast<std::uint8_t>(down(SDL_SCANCODE_D) ? 0xFF : down(SDL_SCANCODE_A) ? 0 : 0x80);
    pad.left_y = static_cast<std::uint8_t>(down(SDL_SCANCODE_S) ? 0xFF : down(SDL_SCANCODE_W) ? 0 : 0x80);
    pad.right_x = static_cast<std::uint8_t>(down(SDL_SCANCODE_L) ? 0xFF : down(SDL_SCANCODE_J) ? 0 : 0x80);
    pad.right_y = static_cast<std::uint8_t>(down(SDL_SCANCODE_K) ? 0xFF : down(SDL_SCANCODE_I) ? 0 : 0x80);
    if (port == 0 && m_test_cross_pending) {
        pad.set(Button::Cross, true);
        m_test_cross_pending = false;
    }
}

}  // namespace sly1::platform
