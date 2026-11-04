// SPDX-License-Identifier: AGPL-3.0-or-later
// SDL input adapter for Sly's native launcher; DS2 report encoding is shared with OpenRAC.
#pragma once

#include "platform/pad.h"

#include <SDL3/SDL.h>

#include <array>

namespace sly1::platform {

class Input {
public:
    Input();
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    void handle_event(const SDL_Event& event);
    void update();
    const openrac::platform::Pad& pad(int port) const;

private:
    void refresh_pad(int port);
    std::array<SDL_Gamepad*, 2> m_gamepads{};
    std::array<openrac::platform::Pad, 2> m_pads{};
};

}  // namespace sly1::platform
