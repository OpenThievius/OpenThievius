// SPDX-License-Identifier: AGPL-3.0-or-later
// Sly launcher adapter around OpenRAC's reusable SDL/OpenGL window platform.
#include "openrac/native_window.h"

#include "../platform/window.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace {
std::unique_ptr<openrac::platform::Platform> g_platform;
std::unique_ptr<openrac::platform::Window> g_window;
using ClearColor = void (*)(float, float, float, float);
using Clear = void (*)(unsigned int);
using Viewport = void (*)(int, int, int, int);
ClearColor g_clear_color = nullptr;
Clear g_clear = nullptr;
Viewport g_viewport = nullptr;
}

extern "C" int openrac_native_window_create(void) {
    openrac::platform::set_app_metadata("OpenThievius - Sly 1", "open-thievius-sly1");
    g_platform = std::make_unique<openrac::platform::Platform>(openrac::platform::kVideo);
    if (!g_platform->ok()) {
        g_platform.reset();
        return 0;
    }

    openrac::platform::WindowConfig config;
    config.title = "OpenThievius - Sly 1";
    config.width = 960;
    config.height = 336;
    config.vsync = false;  // The native loop uses OpenRAC's exact NTSC frame pacer.
    config.software = openrac::platform::SoftwareRenderer::Allow;
    std::string error;
    g_window = openrac::platform::Window::open(config, error);
    if (!g_window) {
        g_platform.reset();
        return 0;
    }

    const auto load = openrac::platform::Window::gl_loader();
    g_clear_color = reinterpret_cast<ClearColor>(load("glClearColor"));
    g_clear = reinterpret_cast<Clear>(load("glClear"));
    g_viewport = reinterpret_cast<Viewport>(load("glViewport"));
    if (!g_clear_color || !g_clear || !g_viewport) {
        g_window.reset();
        g_platform.reset();
        return 0;
    }
    return 1;
}

extern "C" int openrac_native_window_poll_quit(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return 1;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) return 1;
    }
    return 0;
}

extern "C" void openrac_native_window_present(void) {
    if (!g_window) return;
    int width = 0;
    int height = 0;
    g_window->drawable_size(width, height);
    g_viewport(0, 0, width, height);
    g_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    g_clear(0x00004000u);  // GL_COLOR_BUFFER_BIT
    g_window->swap();
}

extern "C" void openrac_native_window_destroy(void) {
    g_window.reset();
    g_platform.reset();
    g_clear_color = nullptr;
    g_clear = nullptr;
    g_viewport = nullptr;
}
