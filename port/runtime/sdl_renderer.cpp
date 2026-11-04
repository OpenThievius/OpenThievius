/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "openrac/native_gs.h"
#include "openrac/native_window.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {
SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
}

extern "C" int openrac_native_window_create(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0;
    if (!SDL_CreateWindowAndRenderer("OpenThievius - Sly 1", 960, 336,
                                     SDL_WINDOW_RESIZABLE, &g_window, &g_renderer)) {
        SDL_Quit();
        return 0;
    }
    SDL_SetRenderLogicalPresentation(g_renderer, 640, 224,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
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
    if (g_renderer == nullptr) return;
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    const size_t count = openrac_guest_gs_published_vertex_count();
    std::vector<openrac_gs_vertex> source(count);
    const size_t read_count = openrac_guest_gs_read_vertices(source.data(), source.size());
    std::vector<SDL_Vertex> vertices(read_count);
    for (size_t i = 0; i < read_count; ++i) {
        const uint32_t rgba = source[i].rgba;
        vertices[i].position = {source[i].x, source[i].y};
        vertices[i].color = {
            static_cast<float>(rgba & 0xffu) / 255.0f,
            static_cast<float>((rgba >> 8u) & 0xffu) / 255.0f,
            static_cast<float>((rgba >> 16u) & 0xffu) / 255.0f,
            static_cast<float>((rgba >> 24u) & 0xffu) / 255.0f,
        };
        vertices[i].tex_coord = {0.0f, 0.0f};
    }
    if (!vertices.empty()) {
        SDL_RenderGeometry(g_renderer, nullptr, vertices.data(),
                           static_cast<int>(vertices.size()), nullptr, 0);
    }
    SDL_RenderPresent(g_renderer);
}

extern "C" void openrac_native_window_destroy(void) {
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    g_renderer = nullptr;
    g_window = nullptr;
    SDL_Quit();
}
