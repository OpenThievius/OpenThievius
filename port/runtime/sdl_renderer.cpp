// SPDX-License-Identifier: AGPL-3.0-or-later
// Sly's completed DMA packets feed OpenRAC's reusable GS direct renderer.
#include "openrac/native_window.h"
#include "openrac/native_gs.h"
#include "openrac/guest.h"

#include "../platform/window.h"
#include "../platform/sly_input.h"
#include "renderer/direct.h"
#include "renderer/framebuffer.h"
#include "renderer/gl.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture_pool.h"
#include "common/log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>
#include <vector>

// Defined in vu1_native.cpp; the same read-only accepted-append summary for the PATH3 chain.
extern "C" void openrac_trace_accepted_kick_xyz(const void* data, size_t size,
                                                 const char* origin);

// The standalone Sly window renders straight to framebuffer 0 without MSAA.
// OpenRAC's DirectRenderer uses this adapter only when a packet samples the
// current frame back into itself.
namespace openrac::renderer {
unsigned readable(const RenderTarget& target) { return target.framebuffer; }
}

namespace {
using ClearColor = void (*)(float, float, float, float);
using Clear = void (*)(unsigned int);
using Viewport = void (*)(int, int, int, int);

struct NativeWorldMesh {
    std::uint64_t instance = 0;
    std::uint32_t request = 0;
    std::uint32_t source_mesh = 0;
    std::uint32_t target_tbp = 0;
    int target_width = 0;
    int target_height = 0;
    bool render_target = false;
    std::vector<float> vertices;
    std::vector<std::uint8_t> vertex_indices;
    std::array<float, 16> mvp{};
    std::array<float, 16> model{};
    std::array<float, 16> view_projection{};
    bool model_is_identity = false;
    std::uint64_t tex0 = 0;
    std::uint64_t tex1 = 0;
    std::uint64_t clamp = 0;
    std::uint64_t alpha = 0;
    bool alpha_valid = false;
    std::uint64_t test = 0;
};

std::unique_ptr<openrac::platform::Platform> g_platform;
std::unique_ptr<sly1::platform::Input> g_input;
std::unique_ptr<openrac::platform::Window> g_window;
std::unique_ptr<openrac::renderer::TexturePool> g_textures;
std::unique_ptr<openrac::renderer::DirectRenderer> g_direct;
std::unique_ptr<openrac::renderer::Shader> g_world_shader;
std::unique_ptr<openrac::renderer::Shader> g_movie_shader;
std::mutex g_movie_frame_mutex;
std::shared_ptr<const std::vector<std::uint8_t>> g_movie_frame;
std::uint64_t g_movie_frame_serial = 0;
std::uint64_t g_movie_uploaded_serial = 0;
int g_movie_frame_width = 0;
int g_movie_frame_height = 0;
int g_movie_texture_width = 0;
int g_movie_texture_height = 0;
bool g_movie_frame_active = false;
openrac::gl::GLuint g_movie_vao = 0;
openrac::gl::GLuint g_movie_texture = 0;
openrac::gl::GLint g_movie_sampler_location = -1;
std::mutex g_world_mesh_mutex;
std::vector<NativeWorldMesh> g_world_meshes;
std::mutex g_render_frame_mutex;
std::shared_ptr<const std::vector<std::uint8_t>> g_ready_packets =
    std::make_shared<const std::vector<std::uint8_t>>();
std::shared_ptr<const std::vector<NativeWorldMesh>> g_ready_world_meshes =
    std::make_shared<const std::vector<NativeWorldMesh>>();
std::size_t g_ready_world_mesh_replacements = 0;
std::uint64_t g_ready_frame_serial = 0;
struct NativeTextureTarget {
    std::unique_ptr<openrac::renderer::FrameBuffer> framebuffer;
    openrac::renderer::TextureHandle texture = 0;
    int width = 0;
    int height = 0;
};
std::unordered_map<std::uint32_t, NativeTextureTarget> g_native_texture_targets;
std::size_t g_world_mesh_replacements = 0;
std::uint32_t g_world_mesh_world = 0;
openrac::gl::GLuint g_world_vao = 0;
openrac::gl::GLuint g_world_vbo = 0;
openrac::gl::GLuint g_present_framebuffer = 0;
openrac::gl::GLuint g_present_texture = 0;
int g_present_frame_width = 0;
int g_present_frame_height = 0;
bool g_present_frame_valid = false;
openrac::gl::GLint g_world_mvp_location = -1;
openrac::gl::GLint g_world_texture_location = -1;
openrac::gl::GLint g_world_textured_location = -1;
openrac::gl::GLint g_world_tcc_location = -1;
openrac::gl::GLint g_world_tfx_location = -1;
openrac::gl::GLint g_world_tex_alpha_scale_location = -1;
openrac::gl::GLint g_world_source_scale_location = -1;
openrac::gl::GLint g_world_source_fix_location = -1;
openrac::gl::GLint g_world_alpha_test_location = -1;
openrac::gl::GLint g_world_alpha_ref_location = -1;
openrac::gl::GLint g_world_alpha_keep_failing_location = -1;
openrac::gl::GLint g_world_debug_geometry_location = -1;
openrac::gl::GLint g_world_debug_color_location = -1;
ClearColor g_clear_color = nullptr;
Clear g_clear = nullptr;
Viewport g_viewport = nullptr;
std::mutex g_packets_mutex;
std::mutex g_input_mutex;
std::vector<std::uint8_t> g_packets;
constexpr std::size_t kMaxQueuedGifBytes = 16u * 1024u * 1024u;
std::atomic<std::uint64_t> g_dma_submit_count{0};
std::atomic<std::uint64_t> g_dma_accept_count{0};
std::atomic<std::uint64_t> g_dma_reject_count{0};
std::atomic<std::uint64_t> g_dma_bytes{0};
std::atomic<unsigned> g_dma_reject_notices{0};

bool gif_queue_has_room_locked(std::size_t incoming_bytes,
                               std::size_t maximum_bytes) {
    const std::size_t queued_bytes = g_packets.size();
    return incoming_bytes <= maximum_bytes && queued_bytes <= maximum_bytes &&
           incoming_bytes <= maximum_bytes - queued_bytes;
}

void trace_gif_queue_append(const char* source, std::size_t incoming_bytes,
                            std::uint32_t guest_address = 0, const void* host_source = nullptr) {
    if (std::getenv("OPENRAC_TRACE_GIF_QUEUE") == nullptr) return;
    static std::atomic<unsigned> trace_count{0};
    const unsigned index = trace_count.fetch_add(1, std::memory_order_relaxed);
    unsigned limit = 512u;
    if (const char* setting = std::getenv("OPENRAC_TRACE_GIF_QUEUE_LIMIT")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 10);
        if (end != setting && *end == '\0' && parsed <= 100000u)
            limit = static_cast<unsigned>(parsed);
    }
    if (index < limit) {
        std::fprintf(stderr,
                     "[native GIF queue] append=%u source=%s guest_source=%#010x host_source=%p incoming=%zu queued=%zu\n",
                     index, source, guest_address, host_source, incoming_bytes, g_packets.size());
    }
}

bool ensure_present_frame(int width, int height) {
    using namespace openrac::gl;
    if (width <= 0 || height <= 0) return false;
    const bool resized = width != g_present_frame_width || height != g_present_frame_height;
    if (g_present_texture == 0) glGenTextures(1, &g_present_texture);
    if (g_present_framebuffer == 0) glGenFramebuffers(1, &g_present_framebuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_present_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (resized || g_present_frame_width == 0) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        g_present_frame_width = width;
        g_present_frame_height = height;
        g_present_frame_valid = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_present_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           g_present_texture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) {
        g_present_frame_valid = false;
        openrac::log::warn("native saved-frame framebuffer is incomplete");
    }
    return complete;
}

bool restore_present_frame(int width, int height) {
    using namespace openrac::gl;
    if (!g_present_frame_valid || width != g_present_frame_width ||
        height != g_present_frame_height || !ensure_present_frame(width, height)) {
        return false;
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_present_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void save_present_frame(int width, int height) {
    using namespace openrac::gl;
    if (!ensure_present_frame(width, height)) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_present_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_present_frame_valid = true;
}

std::uint64_t read_u64(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

void trace_gs_register_inputs(const std::uint8_t* bytes, std::size_t size,
                              std::uint32_t source, const char* path) {
    if (bytes == nullptr || std::getenv("OPENRAC_TRACE_GS_REGISTERS") == nullptr) return;
    static std::atomic<unsigned> traces{0};
    std::size_t at = 0;
    std::size_t tag_index = 0;
    while (at + 16u <= size) {
        const std::uint64_t low = read_u64(bytes + at);
        const std::uint64_t high = read_u64(bytes + at + 8u);
        const std::uint32_t loops = static_cast<std::uint32_t>(low & 0x7FFFu);
        const std::uint32_t format = static_cast<std::uint32_t>((low >> 58u) & 3u);
        std::uint32_t registers = static_cast<std::uint32_t>((low >> 60u) & 0xFu);
        if (registers == 0) registers = 16;
        at += 16u;
        if (format == 0u) {
            const std::size_t qwords = static_cast<std::size_t>(loops) * registers;
            if (qwords > (size - at) / 16u) return;
            for (std::uint32_t loop = 0; loop < loops; ++loop) {
                for (std::uint32_t reg = 0; reg < registers; ++reg) {
                    const std::uint64_t value = read_u64(bytes + at);
                    const std::uint64_t meta = read_u64(bytes + at + 8u);
                    const std::uint32_t descriptor = static_cast<std::uint32_t>(
                        (high >> (reg * 4u)) & 0xFu);
                    if (descriptor == 0xEu) {
                        const std::uint32_t address = static_cast<std::uint32_t>(meta & 0xFFu);
                        if ((address == 0x06u || address == 0x07u || address == 0x40u) &&
                            traces.fetch_add(1, std::memory_order_relaxed) < 256u) {
                            std::fprintf(stderr,
                                         "[gif source register] path=%s source=%#010x tag=%zu offset=%zu loop=%u reg=%u address=%#04x value=%#018llx\n",
                                         path, source, tag_index, at, loop, reg, address,
                                         static_cast<unsigned long long>(value));
                        }
                    }
                    at += 16u;
                }
            }
        } else if (format == 1u) {
            const std::size_t words = static_cast<std::size_t>(loops) * registers;
            const std::size_t qwords = (words + 1u) / 2u;
            if (qwords > (size - at) / 16u) return;
            at += qwords * 16u;
        } else {
            const std::size_t qwords = loops;
            if (qwords > (size - at) / 16u) return;
            at += qwords * 16u;
        }
        ++tag_index;
    }
}

bool init_world_renderer(std::string& error) {
    using namespace openrac::gl;
    constexpr std::string_view vertex = R"(#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_mvp;
out vec4 v_color;
out vec2 v_uv;
void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    gl_PointSize = 6.0;
    v_color = a_color;
    v_uv = a_uv;
}
)";
    constexpr std::string_view fragment = R"(#version 410 core
in vec4 v_color;
in vec2 v_uv;
uniform sampler2D u_texture;
uniform int u_textured;
uniform int u_tcc;
uniform int u_tfx;
uniform float u_tex_alpha_scale;
uniform int u_source_scale;
uniform float u_source_fix;
uniform int u_alpha_test;
uniform float u_alpha_ref;
uniform int u_alpha_keep_failing;
uniform int u_debug_geometry;
uniform vec4 u_debug_color;
out vec4 frag_color;
bool alpha_passes(float a) {
    float value = floor(a + 0.5);
    switch (u_alpha_test) {
        case 2: return value < u_alpha_ref;
        case 3: return value <= u_alpha_ref;
        case 4: return value == u_alpha_ref;
        case 5: return value >= u_alpha_ref;
        case 6: return value > u_alpha_ref;
        case 7: return value != u_alpha_ref;
        default: return true;
    }
}
void main() {
    if (u_debug_geometry != 0) {
        frag_color = u_debug_color;
        return;
    }
    vec4 color = v_color * 255.0;
    if (u_textured != 0) {
        vec4 texel = texture(u_texture, v_uv) * 255.0;
        texel.a *= u_tex_alpha_scale;
        if (u_tfx == 0) {
            color.rgb = color.rgb * texel.rgb / 128.0;
            if (u_tcc != 0) color.a = color.a * texel.a / 128.0;
        } else if (u_tfx == 1) {
            color.rgb = texel.rgb;
            if (u_tcc != 0) color.a = texel.a;
        } else if (u_tfx == 2) {
            color.rgb = color.rgb * texel.rgb / 128.0 + color.aaa;
            if (u_tcc != 0) color.a += texel.a;
        } else {
            color.rgb = color.rgb * texel.rgb / 128.0 + color.aaa;
            if (u_tcc != 0) color.a = texel.a;
        }
    }
    bool alpha_failed = u_alpha_test != 0 &&
        alpha_passes(color.a) == (u_alpha_keep_failing != 0);
    if (alpha_failed) discard;
    if (u_source_scale == 1)
        color.rgb = min(color.rgb * color.a / 128.0, 255.0);
    else if (u_source_scale == 2)
        color.rgb = min(color.rgb * u_source_fix, 255.0);
    frag_color = vec4(clamp(color.rgb, 0.0, 255.0) / 255.0,
                      clamp(color.a / 128.0, 0.0, 1.0));
}
)";
    g_world_shader = std::make_unique<openrac::renderer::Shader>();
    if (!g_world_shader->build("sly-world-kind6", vertex, fragment, error)) {
        g_world_shader.reset();
        return false;
    }
    g_world_mvp_location = g_world_shader->uniform("u_mvp");
    g_world_texture_location = g_world_shader->uniform("u_texture");
    g_world_textured_location = g_world_shader->uniform("u_textured");
    g_world_tcc_location = g_world_shader->uniform("u_tcc");
    g_world_tfx_location = g_world_shader->uniform("u_tfx");
    g_world_tex_alpha_scale_location = g_world_shader->uniform("u_tex_alpha_scale");
    g_world_source_scale_location = g_world_shader->uniform("u_source_scale");
    g_world_source_fix_location = g_world_shader->uniform("u_source_fix");
    g_world_alpha_test_location = g_world_shader->uniform("u_alpha_test");
    g_world_alpha_ref_location = g_world_shader->uniform("u_alpha_ref");
    g_world_alpha_keep_failing_location = g_world_shader->uniform("u_alpha_keep_failing");
    g_world_debug_geometry_location = g_world_shader->uniform("u_debug_geometry");
    g_world_debug_color_location = g_world_shader->uniform("u_debug_color");
    glGenVertexArrays(1, &g_world_vao);
    glGenBuffers(1, &g_world_vbo);
    if (g_world_vao == 0 || g_world_vbo == 0) {
        error = "could not allocate Sly world mesh buffers";
        return false;
    }
    glBindVertexArray(g_world_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_world_vbo);
    constexpr GLsizei stride = 9 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(7 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

bool init_movie_renderer(std::string& error) {
    using namespace openrac::gl;
    constexpr std::string_view vertex = R"(#version 410 core
out vec2 v_uv;
void main() {
    const vec2 positions[4] = vec2[4](
        vec2(-1.0, -1.0), vec2( 1.0, -1.0),
        vec2(-1.0,  1.0), vec2( 1.0,  1.0));
    const vec2 texcoords[4] = vec2[4](
        vec2(0.0, 1.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 0.0));
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_uv = texcoords[gl_VertexID];
}
)";
    constexpr std::string_view fragment = R"(#version 410 core
in vec2 v_uv;
uniform sampler2D u_movie;
out vec4 frag_color;
void main() { frag_color = texture(u_movie, v_uv); }
)";
    g_movie_shader = std::make_unique<openrac::renderer::Shader>();
    if (!g_movie_shader->build("sly-movie-frame", vertex, fragment, error)) {
        g_movie_shader.reset();
        return false;
    }
    g_movie_sampler_location = g_movie_shader->uniform("u_movie");
    glGenVertexArrays(1, &g_movie_vao);
    glGenTextures(1, &g_movie_texture);
    if (g_movie_vao == 0 || g_movie_texture == 0) {
        error = "could not allocate movie frame texture";
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, g_movie_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool draw_movie_frame(int width, int height) {
    using namespace openrac::gl;
    std::shared_ptr<const std::vector<std::uint8_t>> pixels;
    std::uint64_t serial = 0;
    int frame_width = 0;
    int frame_height = 0;
    {
        std::scoped_lock lock(g_movie_frame_mutex);
        if (!g_movie_frame_active || !g_movie_frame) return false;
        pixels = g_movie_frame;
        serial = g_movie_frame_serial;
        frame_width = g_movie_frame_width;
        frame_height = g_movie_frame_height;
    }
    if (!g_movie_shader || g_movie_texture == 0 || frame_width <= 0 ||
        frame_height <= 0 || pixels->size() !=
            static_cast<std::size_t>(frame_width) * frame_height * 4u)
        return false;

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    g_viewport(0, 0, width, height);
    g_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    g_clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_movie_texture);
    if (g_movie_texture_width != frame_width || g_movie_texture_height != frame_height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame_width, frame_height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels->data());
        g_movie_texture_width = frame_width;
        g_movie_texture_height = frame_height;
        g_movie_uploaded_serial = serial;
    } else if (g_movie_uploaded_serial != serial) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width, frame_height,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels->data());
        g_movie_uploaded_serial = serial;
    }
    g_movie_shader->use();
    glUniform1i(g_movie_sampler_location, 0);
    glBindVertexArray(g_movie_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    if (std::getenv("OPENRAC_DUMP_MOVIE_FRAMES") != nullptr) {
        static unsigned dumped = 0;
        static std::uint64_t last_dumped_serial = 0;
        unsigned stride = 30;
        if (const char* setting = std::getenv("OPENRAC_DUMP_MOVIE_FRAME_STRIDE")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed > 0 && parsed <= 100000u)
                stride = static_cast<unsigned>(parsed);
        }
        if (dumped < 8u && serial != last_dumped_serial && (serial - 1u) % stride == 0u) {
            last_dumped_serial = serial;
            const auto frame = openrac::renderer::read_rgba(0, width, height);
            char path[512];
            const char* directory = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_DIR");
            if (directory == nullptr || directory[0] == '\0') directory = "/tmp";
            const int length = std::snprintf(path, sizeof(path),
                "%s/sly1-movie-frame-%02u.ppm", directory, dumped++);
            if (length > 0 && static_cast<std::size_t>(length) < sizeof(path)) {
                if (FILE* file = std::fopen(path, "wb")) {
                    std::fprintf(file, "P6 %d %d 255\n", width, height);
                    for (std::size_t i = 0; i + 3 < frame.size(); i += 4)
                        std::fwrite(frame.data() + i, 1, 3, file);
                    std::fclose(file);
                    std::fprintf(stderr, "[native movie frame dump] path=%s serial=%llu\n",
                        path, static_cast<unsigned long long>(serial));
                }
            }
        }
    }
    return true;
}

void render_world_meshes(const std::vector<NativeWorldMesh>& meshes,
                         bool target_pass = false) {
    using namespace openrac::gl;
    if (!g_world_shader || meshes.empty()) return;
    bool debug_source_filter = false;
    std::uint32_t debug_source_mesh = 0;
    if (const char* setting = std::getenv("OPENRAC_DEBUG_SLY_WORLD_SOURCE")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            debug_source_mesh = static_cast<std::uint32_t>(parsed);
            debug_source_filter = true;
        }
    }
    bool debug_request_filter = false;
    std::uint32_t debug_request = 0;
    if (const char* setting = std::getenv("OPENRAC_DEBUG_SLY_WORLD_REQUEST")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            debug_request = static_cast<std::uint32_t>(parsed);
            debug_request_filter = true;
        }
    }
    std::vector<std::uint32_t> debug_skip_requests;
    if (const char* setting = std::getenv("OPENRAC_DEBUG_SLY_WORLD_SKIP_REQUEST")) {
        const char* next = setting;
        while (*next != '\0') {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(next, &end, 0);
            if (end == next || parsed > 0xFFFFFFFFul ||
                (*end != '\0' && *end != ',')) {
                debug_skip_requests.clear();
                break;
            }
            debug_skip_requests.push_back(static_cast<std::uint32_t>(parsed));
            if (*end == '\0') break;
            next = end + 1;
        }
    }
    const bool debug_skip_request_filter = !debug_skip_requests.empty();
    const bool debug_first_material_pass =
        std::getenv("OPENRAC_DEBUG_SLY_WORLD_FIRST_PASS") != nullptr;
    GLenum debug_primitive = GL_TRIANGLE_STRIP;
    if (const char* setting = std::getenv("OPENRAC_DEBUG_SLY_WORLD_PRIMITIVE")) {
        if (std::strcmp(setting, "triangles") == 0)
            debug_primitive = GL_TRIANGLES;
        else if (std::strcmp(setting, "points") == 0)
            debug_primitive = GL_POINTS;
    }
    static unsigned debug_selection_notice = 0;
    if ((debug_source_filter || debug_request_filter || debug_skip_request_filter ||
         debug_first_material_pass || debug_primitive != GL_TRIANGLE_STRIP) &&
        debug_selection_notice++ < 1u) {
        std::fprintf(stderr,
                     "[native sly debug selection] source=%s%#010x request=%s%#010x skipped_requests=%zu first_pass=%u primitive=%s\n",
                     debug_source_filter ? "" : "all/",
                     debug_source_mesh,
                     debug_request_filter ? "" : "all/",
                     debug_request,
                     debug_skip_requests.size(),
                     debug_first_material_pass ? 1u : 0u,
                     debug_primitive == GL_TRIANGLES ? "triangles" :
                     debug_primitive == GL_POINTS ? "points" : "triangle-strip");
    }
    const bool trace_projected_bounds =
        std::getenv("OPENRAC_TRACE_SLY_PRESENT_BOUNDS") != nullptr;
    const bool trace_native_per_mesh_bounds =
        std::getenv("OPENRAC_TRACE_SLY_PER_MESH_BOUNDS") != nullptr;
    static bool native_per_mesh_bounds_logged = false;
    const bool debug_solid_colors =
        std::getenv("OPENRAC_DEBUG_SLY_WORLD_SOLID_COLORS") != nullptr;
    const bool debug_normal_colors =
        std::getenv("OPENRAC_DEBUG_SLY_WORLD_NORMAL_COLORS") != nullptr;
    const bool debug_skip_full_cover =
        std::getenv("OPENRAC_DEBUG_SLY_SKIP_FULL_COVER") != nullptr;
    static unsigned present_bounds_sample = 0;
    unsigned projected_meshes = 0;
    unsigned intersecting_meshes = 0;
    unsigned long long projected_vertices = 0;
    unsigned long long inside_vertices = 0;
    std::unordered_set<std::uint64_t> present_instances;
    std::unordered_set<std::uint64_t> present_materials;
    std::unordered_set<std::uint64_t> present_uploaded_textures;
    std::unordered_set<std::uint64_t> present_missing_textures;
    unsigned duplicate_instances = 0;
    std::uint64_t present_signature = 1469598103934665603ull;
    float frame_min_x = 1.0e30f, frame_min_y = 1.0e30f, frame_min_z = 1.0e30f;
    float frame_max_x = -1.0e30f, frame_max_y = -1.0e30f, frame_max_z = -1.0e30f;
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static unsigned notices = 0;
        if (notices++ < 16u)
            std::fprintf(stderr, "[native sly world render] submissions=%zu\n", meshes.size());
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    g_world_shader->use();
    glBindVertexArray(g_world_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_world_vbo);
    for (const auto& mesh : meshes) {
        if (mesh.render_target != target_pass) continue;
        if (std::getenv("OPENRAC_TRACE_SLY_GL_MESH_BOUNDS") != nullptr) {
            static unsigned queued_mesh_notices = 0;
            if (queued_mesh_notices++ < 96u)
                std::fprintf(stderr,
                             "[native sly queued mesh] instance=%#llx request=%#010x source=%#010x vertices=%zu\n",
                             static_cast<unsigned long long>(mesh.instance),
                             mesh.request,
                             mesh.source_mesh, mesh.vertices.size() / 9u);
        }
        if (debug_source_filter && mesh.source_mesh != debug_source_mesh) continue;
        if (debug_request_filter &&
            mesh.request != debug_request)
            continue;
        if (debug_skip_request_filter &&
            std::find(debug_skip_requests.begin(), debug_skip_requests.end(),
                      mesh.request) != debug_skip_requests.end())
            continue;
        if (debug_first_material_pass && (mesh.instance & 0xFFu) != 0u) continue;
        if (trace_projected_bounds) {
            if (!present_instances.insert(mesh.instance).second) ++duplicate_instances;
            present_materials.insert(mesh.tex0);
            present_signature ^= mesh.instance;
            present_signature *= 1099511628211ull;
            present_signature ^= mesh.tex0;
            present_signature *= 1099511628211ull;
            present_signature ^= mesh.alpha;
            present_signature *= 1099511628211ull;
            present_signature ^= static_cast<std::uint64_t>(mesh.vertices.size() / 9u);
            present_signature *= 1099511628211ull;
        }
        if (mesh.vertices.size() < 3u * 9u || mesh.vertices.size() > 4096u * 9u)
            continue;
        bool projected_cover = false;
        if (trace_projected_bounds || debug_skip_full_cover ||
            (trace_native_per_mesh_bounds && !target_pass &&
             !native_per_mesh_bounds_logged)) {
            float min_x = 1.0e30f, min_y = 1.0e30f, min_z = 1.0e30f;
            float max_x = -1.0e30f, max_y = -1.0e30f, max_z = -1.0e30f;
            unsigned mesh_projected = 0;
            unsigned mesh_inside = 0;
            const std::size_t vertex_count = mesh.vertices.size() / 9u;
            for (std::size_t i = 0; i < vertex_count; ++i) {
                const float x = mesh.vertices[i * 9u + 0u];
                const float y = mesh.vertices[i * 9u + 1u];
                const float z = mesh.vertices[i * 9u + 2u];
                float clip[4]{};
                for (std::size_t row = 0; row < 4; ++row) {
                    clip[row] = mesh.mvp[row] * x + mesh.mvp[4u + row] * y +
                                mesh.mvp[8u + row] * z + mesh.mvp[12u + row];
                }
                if (!std::isfinite(clip[3]) || clip[3] <= 1.0e-6f) continue;
                const float nx = clip[0] / clip[3];
                const float ny = clip[1] / clip[3];
                const float nz = clip[2] / clip[3];
                if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) continue;
                ++mesh_projected;
                min_x = std::min(min_x, nx); max_x = std::max(max_x, nx);
                min_y = std::min(min_y, ny); max_y = std::max(max_y, ny);
                min_z = std::min(min_z, nz); max_z = std::max(max_z, nz);
                if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f &&
                    nz >= -1.0f && nz <= 1.0f)
                    ++mesh_inside;
            }
            if (mesh_projected != 0) {
                ++projected_meshes;
                projected_vertices += mesh_projected;
                inside_vertices += mesh_inside;
                frame_min_x = std::min(frame_min_x, min_x); frame_max_x = std::max(frame_max_x, max_x);
                frame_min_y = std::min(frame_min_y, min_y); frame_max_y = std::max(frame_max_y, max_y);
                frame_min_z = std::min(frame_min_z, min_z); frame_max_z = std::max(frame_max_z, max_z);
                if (max_x >= -1.0f && min_x <= 1.0f &&
                    max_y >= -1.0f && min_y <= 1.0f &&
                    max_z >= -1.0f && min_z <= 1.0f)
                    ++intersecting_meshes;
                if (debug_solid_colors && min_x <= -1.0f && max_x >= 1.0f &&
                    min_y <= -1.0f && max_y >= 1.0f &&
                    present_bounds_sample < 64u) {
                    std::fprintf(stderr,
                                 "[native sly solid cover] frame=%u instance=%#llx source=%#010x vertices=%zu tex0=%#llx test=%#llx ndc=%g,%g..%g,%g z=%g..%g\n",
                                 present_bounds_sample,
                                 static_cast<unsigned long long>(mesh.instance),
                                 mesh.source_mesh, vertex_count,
                                 static_cast<unsigned long long>(mesh.tex0),
                                 static_cast<unsigned long long>(mesh.test),
                                 min_x, min_y, max_x, max_y, min_z, max_z);
                }
                projected_cover = min_x <= -1.0f && max_x >= 1.0f &&
                                  min_y <= -1.0f && max_y >= 1.0f;
                if (trace_native_per_mesh_bounds && !target_pass &&
                    !native_per_mesh_bounds_logged) {
                    std::fprintf(stderr,
                                 "[native sly per-mesh bounds] instance=%#llx request=%#010x source=%#010x vertices=%zu projected=%u inside=%u tex0=%#llx ndc=%g,%g,%g..%g,%g,%g\n",
                                 static_cast<unsigned long long>(mesh.instance),
                                 mesh.request, mesh.source_mesh, vertex_count,
                                 mesh_projected, mesh_inside,
                                 static_cast<unsigned long long>(mesh.tex0),
                                 min_x, min_y, min_z, max_x, max_y, max_z);
                    std::fprintf(stderr,
                                 "[native sly per-mesh matrices] instance=%#llx mvp=",
                                 static_cast<unsigned long long>(mesh.instance));
                    for (std::size_t i = 0; i < mesh.mvp.size(); ++i)
                        std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", mesh.mvp[i]);
                    std::fprintf(stderr, " model=");
                    for (std::size_t i = 0; i < mesh.model.size(); ++i)
                        std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", mesh.model[i]);
                    std::fprintf(stderr, " view_projection=");
                    for (std::size_t i = 0; i < mesh.view_projection.size(); ++i)
                        std::fprintf(stderr, "%s%g", i == 0 ? "" : ",",
                                     mesh.view_projection[i]);
                    std::fputc('\n', stderr);
                }
            }
        }
        const auto tex0 = openrac::renderer::gs::Tex0::decode(mesh.tex0);
        const auto tex1 = openrac::renderer::gs::Tex1::decode(mesh.tex1);
        const auto clamp = openrac::renderer::gs::Clamp::decode(mesh.clamp);
        const auto test = openrac::renderer::gs::Test::decode(mesh.test);
        bool missing_texture_fallback = false;
        if (debug_skip_full_cover && projected_cover && test.zte &&
            test.ztst == openrac::renderer::gs::DepthTest::Always) {
            static unsigned skipped_cover_notices = 0;
            if (skipped_cover_notices++ < 32u)
                std::fprintf(stderr,
                             "[native sly debug skip] instance=%#llx source=%#010x vertices=%zu\n",
                             static_cast<unsigned long long>(mesh.instance),
                             mesh.source_mesh, mesh.vertices.size() / 9u);
            continue;
        }
        auto texture = openrac::renderer::TextureHandle{};
        if (!debug_solid_colors && !debug_normal_colors &&
            g_textures != nullptr && mesh.tex0 != 0) {
            const auto texa = openrac::renderer::gs::Texa{0, false, 0x80};
            texture = g_textures->resolve_native_sly_asset(tex0, texa);
            if (texture == 0) texture = g_textures->resolve(tex0, texa);
            if (texture == g_textures->placeholder()) {
                present_missing_textures.insert(mesh.tex0);
                // Keep native geometry visible while texture residency is
                // incomplete. The fallback is a stable per-instance flat
                // color; it is clearly diagnostic and never pretends to be
                // recovered game art. Set OPENRAC_SKIP_MISSING_TEXTURE_MESH
                // to restore the previous black-background behavior, or
                // OPENRAC_SHOW_PLACEHOLDER_MESH to draw the checker texture.
                if (getenv("OPENRAC_SHOW_PLACEHOLDER_MESH") == nullptr) {
                    if (getenv("OPENRAC_SKIP_MISSING_TEXTURE_MESH") != nullptr)
                        continue;
                    missing_texture_fallback = true;
                    texture = 0;
                }
            } else
                present_uploaded_textures.insert(mesh.tex0);
            if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
                static std::unordered_map<std::uint64_t, bool> texture_notices;
                if (texture_notices.emplace(mesh.tex0, true).second) {
                    const auto diagnostic_texture = texture != 0
                        ? texture : g_textures->placeholder();
                    const auto& image = g_textures->image(diagnostic_texture);
                    std::fprintf(stderr,
                                 "[native sly material] tbp=%#x cbp=%#x psm=%#x size=%dx%d handle=%u name=%s placeholder=%u test=%#llx alpha=%s%#llx tex1=%#llx clamp=%#llx\n",
                                 tex0.tbp0, tex0.cbp, tex0.psm, image.width, image.height,
                                 diagnostic_texture,
                                 g_textures->name(diagnostic_texture).c_str(),
                                 diagnostic_texture == g_textures->placeholder() ? 1u : 0u,
                                 static_cast<unsigned long long>(mesh.test),
                                 mesh.alpha_valid ? "" : "missing/",
                                 static_cast<unsigned long long>(mesh.alpha),
                                 static_cast<unsigned long long>(mesh.tex1),
                                 static_cast<unsigned long long>(mesh.clamp));
                }
            }
        }
        // The game's row-vector path uses model*viewProjection. The host's
        // column-major GLSL uniform interprets that row-major product as its
        // transpose, matching the source transform. A fresh paired capture
        // shows mode 0 preserves corridor-like terrain; mode 3 makes radial
        // fans. Keep the override for continued comparison.
        int mvp_mode = 0;
        if (std::getenv("OPENRAC_SLY_WORLD_MVP_MODE") != nullptr) {
            char* end = nullptr;
            const long parsed = std::strtol(std::getenv("OPENRAC_SLY_WORLD_MVP_MODE"), &end, 0);
            if (end != std::getenv("OPENRAC_SLY_WORLD_MVP_MODE") && *end == '\0' &&
                parsed >= 0 && parsed <= 3) mvp_mode = static_cast<int>(parsed);
        }
        if (std::getenv("OPENRAC_SLY_WORLD_AUTO_MATRIX") != nullptr)
            mvp_mode = mesh.model_is_identity ? 0 : 1;
        float position_scale = 1.0f;
        if (const char* setting = std::getenv("OPENRAC_SLY_WORLD_SCALE")) {
            char* end = nullptr;
            const float parsed = std::strtof(setting, &end);
            if (end != setting && parsed > 0.0f && parsed <= 1000.0f) position_scale = parsed;
        }
        float m[16];
        // Matrix conventions remain an explicit diagnostic choice until the
        // Sly model and camera inputs are validated for each draw category.
        int effective_mode = mvp_mode;
        if (effective_mode >= 2) {
            for (std::size_t row = 0; row < 4; ++row)
                for (std::size_t column = 0; column < 4; ++column) {
                    float value = 0.0f;
                    for (std::size_t k = 0; k < 4; ++k)
                        value += mesh.view_projection[row * 4u + k] * mesh.model[k * 4u + column];
                    m[row * 4u + column] = value;
                }
        } else {
            std::memcpy(m, mesh.mvp.data(), sizeof(m));
        }
        if (effective_mode & 1u) {
            const float t[16] = {
                m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13],
                m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
            std::memcpy(m, t, sizeof(m));
        }
        const bool screen_to_clip_all =
            std::getenv("OPENRAC_SLY_WORLD_SCREEN_TO_CLIP") != nullptr;
        const bool screen_to_clip_transformed =
            std::getenv("OPENRAC_SLY_WORLD_SCREEN_TO_CLIP_TRANSFORMED") != nullptr &&
            !mesh.model_is_identity;
        const bool raw_screen_matrix =
            std::getenv("OPENRAC_SLY_WORLD_RAW_SCREEN_MATRIX") != nullptr;
        // The retail projection is scaled for the PS2's 640x224 GS field.
        // Normalize those output axes for OpenGL by default; keep the raw
        // matrix as an explicit diagnostic comparison only.
        bool screen_to_clip = !raw_screen_matrix || screen_to_clip_all ||
                              screen_to_clip_transformed;
        std::uint32_t screen_to_clip_request = 0;
        bool screen_to_clip_request_valid = false;
        bool screen_to_clip_request_match = false;
        if (const char* setting =
                std::getenv("OPENRAC_SLY_WORLD_SCREEN_TO_CLIP_REQUEST")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 0);
            if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
                screen_to_clip_request = static_cast<std::uint32_t>(parsed);
                screen_to_clip_request_valid = true;
                screen_to_clip_request_match = mesh.request == screen_to_clip_request;
                screen_to_clip = screen_to_clip || screen_to_clip_request_match;
            }
        }
        if (!mesh.render_target && screen_to_clip) {
            // The retail camera matrix maps into the PS2's centered GS field
            // coordinates (640x224, with 12-bit fixed-point screen scale).
            // OpenGL instead clips x/y to [-w,+w]. The guest matrix is row-
            // major, and this array is interpreted by GLSL as its transpose;
            // clip x/y therefore use interleaved entries (0,4,8,12) and
            // (1,5,9,13). Scale those output components, not contiguous rows
            // of the guest matrix, while leaving depth and W untouched.
            constexpr float kPs2ScreenToClipX = 4096.0f / 640.0f;
            constexpr float kPs2ScreenToClipY = 4096.0f / 224.0f;
            for (std::size_t column = 0; column < 4; ++column) {
                m[column * 4u] *= kPs2ScreenToClipX;
                m[column * 4u + 1u] *= kPs2ScreenToClipY;
            }
            // The host draws a full frame, while the retail projection is
            // scaled for one 224-line field. Map it across both fields by
            // default; set the diagnostic scale to 1 to compare the former
            // single-field mapping without changing game-space inputs.
            float clip_y_scale = 0.5f;
            if (const char* setting = std::getenv(
                    "OPENRAC_SLY_WORLD_SCREEN_TO_CLIP_Y_SCALE")) {
                char* end = nullptr;
                const float parsed = std::strtof(setting, &end);
                if (end != setting && *end == '\0' && std::isfinite(parsed) &&
                    parsed > 0.0f && parsed <= 4.0f)
                    clip_y_scale = parsed;
            }
            if (clip_y_scale != 1.0f)
                for (std::size_t column = 0; column < 4; ++column)
                    m[column * 4u + 1u] *= clip_y_scale;
            static bool screen_to_clip_notice = false;
            if (!screen_to_clip_notice) {
                screen_to_clip_notice = true;
                const char* scope = raw_screen_matrix ?
                    (screen_to_clip_all ? "forced-all" : "diagnostic-scope") :
                    screen_to_clip_all ? "all" :
                    screen_to_clip_request_match ? "request" :
                    screen_to_clip_transformed ? "transformed" :
                    screen_to_clip_request_valid ? "request" : "default";
                std::fprintf(stderr,
                             "[native sly screen-to-clip] enabled scope=%s request=%#010x scale_x=%g scale_y=%g logical_field=640x224 clip_y_scale=%g\n",
                             scope,
                             screen_to_clip_request,
                             kPs2ScreenToClipX,
                             kPs2ScreenToClipY * clip_y_scale, clip_y_scale);
            }
        }
        if (std::getenv("OPENRAC_TRACE_SLY_GL_MESH_BOUNDS") != nullptr) {
            static unsigned uploaded_bounds_notices = 0;
            if (uploaded_bounds_notices++ < 256u) {
                float min_x = 1.0e30f, min_y = 1.0e30f, min_z = 1.0e30f;
                float max_x = -1.0e30f, max_y = -1.0e30f, max_z = -1.0e30f;
                unsigned projected = 0, inside = 0;
                const std::size_t vertex_count = mesh.vertices.size() / 9u;
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const float x = mesh.vertices[i * 9u + 0u];
                    const float y = mesh.vertices[i * 9u + 1u];
                    const float z = mesh.vertices[i * 9u + 2u];
                    float clip[4]{};
                    for (std::size_t row = 0; row < 4; ++row) {
                        clip[row] = m[row] * x + m[4u + row] * y +
                                    m[8u + row] * z + m[12u + row];
                    }
                    if (!std::isfinite(clip[3]) || clip[3] <= 1.0e-6f) continue;
                    const float nx = clip[0] / clip[3];
                    const float ny = clip[1] / clip[3];
                    const float nz = clip[2] / clip[3];
                    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) continue;
                    ++projected;
                    min_x = std::min(min_x, nx); max_x = std::max(max_x, nx);
                    min_y = std::min(min_y, ny); max_y = std::max(max_y, ny);
                    min_z = std::min(min_z, nz); max_z = std::max(max_z, nz);
                    if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f &&
                        nz >= -1.0f && nz <= 1.0f)
                        ++inside;
                }
                std::fprintf(stderr,
                             "[native sly uploaded bounds] instance=%#llx source=%#010x vertices=%zu positive_w=%u inside=%u ndc=%g,%g,%g..%g,%g,%g\n",
                             static_cast<unsigned long long>(mesh.instance), mesh.source_mesh,
                             vertex_count, projected, inside,
                             min_x, min_y, min_z, max_x, max_y, max_z);
            }
        }
        if (std::getenv("OPENRAC_TRACE_SLY_GL_OUTLIERS") != nullptr &&
            !mesh.render_target) {
            static std::unordered_map<std::uint64_t, std::uint32_t>
                logged_outlier_sources;
            const std::uint64_t draw_key = mesh.instance & ~0xFFull;
            const auto logged = logged_outlier_sources.find(draw_key);
            if (logged_outlier_sources.size() < 512u &&
                (logged == logged_outlier_sources.end() ||
                 logged->second != mesh.source_mesh)) {
                float min_x = 1.0e30f, min_y = 1.0e30f, min_z = 1.0e30f;
                float max_x = -1.0e30f, max_y = -1.0e30f, max_z = -1.0e30f;
                unsigned positive_w = 0, inside = 0;
                const std::size_t vertex_count = mesh.vertices.size() / 9u;
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const float x = mesh.vertices[i * 9u + 0u];
                    const float y = mesh.vertices[i * 9u + 1u];
                    const float z = mesh.vertices[i * 9u + 2u];
                    float clip[4]{};
                    for (std::size_t row = 0; row < 4; ++row)
                        clip[row] = m[row] * x + m[4u + row] * y +
                                    m[8u + row] * z + m[12u + row];
                    if (!std::isfinite(clip[3]) || clip[3] <= 1.0e-6f) continue;
                    const float nx = clip[0] / clip[3];
                    const float ny = clip[1] / clip[3];
                    const float nz = clip[2] / clip[3];
                    if (!std::isfinite(nx) || !std::isfinite(ny) ||
                        !std::isfinite(nz))
                        continue;
                    ++positive_w;
                    min_x = std::min(min_x, nx); max_x = std::max(max_x, nx);
                    min_y = std::min(min_y, ny); max_y = std::max(max_y, ny);
                    min_z = std::min(min_z, nz); max_z = std::max(max_z, nz);
                    if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f &&
                        nz >= -1.0f && nz <= 1.0f)
                        ++inside;
                }
                const bool outlier = positive_w != 0u &&
                    (min_x < -2.0f || max_x > 2.0f || min_y < -2.0f ||
                     max_y > 2.0f || min_z < -2.0f || max_z > 2.0f);
                if (outlier) {
                    logged_outlier_sources[draw_key] = mesh.source_mesh;
                    std::fprintf(stderr,
                                 "[native sly GL outlier] request=%#010x instance=%#llx source=%#010x vertices=%zu positive_w=%u inside=%u ndc=%g,%g,%g..%g,%g,%g mvp=",
                                 mesh.request,
                                 static_cast<unsigned long long>(mesh.instance),
                                 mesh.source_mesh, vertex_count, positive_w, inside,
                                 min_x, min_y, min_z, max_x, max_y, max_z);
                    for (std::size_t i = 0; i < 16u; ++i)
                        std::fprintf(stderr, "%s%g", i == 0u ? "" : ",", m[i]);
                    std::fputc('\n', stderr);
                    if (std::getenv("OPENRAC_TRACE_SLY_GL_OUTLIER_VERTICES") !=
                        nullptr) {
                        static bool outlier_vertices_logged = false;
                        if (!outlier_vertices_logged) {
                            outlier_vertices_logged = true;
                            for (std::size_t i = 0; i < vertex_count; ++i) {
                                const float* vertex = mesh.vertices.data() + i * 9u;
                                float clip[4]{};
                                for (std::size_t row = 0; row < 4u; ++row)
                                    clip[row] = m[row] * vertex[0] +
                                        m[4u + row] * vertex[1] +
                                        m[8u + row] * vertex[2] + m[12u + row];
                                std::fprintf(stderr,
                                             "[native sly GL outlier vertex] request=%#010x instance=%#llx source=%#010x i=%zu ref=%u xyz=%g,%g,%g clip=%g,%g,%g,%g rgba=%g,%g,%g,%g uv=%g,%g\n",
                                             mesh.request,
                                             static_cast<unsigned long long>(mesh.instance),
                                             mesh.source_mesh, i,
                                             i < mesh.vertex_indices.size()
                                                 ? static_cast<unsigned>(
                                                       mesh.vertex_indices[i])
                                                 : 0xFFu,
                                             vertex[0], vertex[1], vertex[2],
                                             clip[0], clip[1], clip[2], clip[3],
                                             vertex[3], vertex[4], vertex[5], vertex[6],
                                             vertex[7], vertex[8]);
                            }
                        }
                    }
                }
            }
        }
        if (std::getenv("OPENRAC_TRACE_SLY_GL_COVERAGE") != nullptr &&
            !mesh.render_target) {
            static std::unordered_map<std::uint64_t, std::uint32_t>
                traced_coverage_sources;
            const std::uint64_t draw_key = mesh.instance & ~0xFFull;
            const auto traced = traced_coverage_sources.find(draw_key);
            if (traced_coverage_sources.size() < 512u &&
                (traced == traced_coverage_sources.end() ||
                 traced->second != mesh.source_mesh)) {
                traced_coverage_sources[draw_key] = mesh.source_mesh;
                const std::size_t vertex_count = mesh.vertices.size() / 9u;
                std::vector<std::array<float, 4>> clips(vertex_count);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const float* vertex = mesh.vertices.data() + i * 9u;
                    for (std::size_t row = 0; row < 4u; ++row)
                        clips[i][row] = m[row] * vertex[0] +
                            m[4u + row] * vertex[1] + m[8u + row] * vertex[2] +
                            m[12u + row];
                }
                std::size_t visible_triangles = 0;
                double total_area = 0.0;
                float min_x = 1.0e30f, min_y = 1.0e30f;
                float max_x = -1.0e30f, max_y = -1.0e30f;
                std::vector<std::array<float, 4>> polygon;
                std::vector<std::array<float, 4>> clipped;
                polygon.reserve(12u);
                clipped.reserve(12u);
                for (std::size_t triangle = 0; triangle + 2u < vertex_count; ++triangle) {
                    polygon.clear();
                    polygon.push_back(clips[triangle]);
                    polygon.push_back(clips[triangle + 1u]);
                    polygon.push_back(clips[triangle + 2u]);
                    for (unsigned plane = 0; plane < 6u && !polygon.empty(); ++plane) {
                        clipped.clear();
                        const auto distance = [plane](const auto& v) {
                            switch (plane) {
                            case 0: return v[3] + v[0];
                            case 1: return v[3] - v[0];
                            case 2: return v[3] + v[1];
                            case 3: return v[3] - v[1];
                            case 4: return v[3] + v[2];
                            default: return v[3] - v[2];
                            }
                        };
                        auto previous = polygon.back();
                        float previous_distance = distance(previous);
                        for (const auto& current : polygon) {
                            const float current_distance = distance(current);
                            if ((previous_distance >= 0.0f) !=
                                (current_distance >= 0.0f)) {
                                const float t = previous_distance /
                                    (previous_distance - current_distance);
                                std::array<float, 4> intersection{};
                                for (std::size_t axis = 0; axis < 4u; ++axis)
                                    intersection[axis] = previous[axis] +
                                        (current[axis] - previous[axis]) * t;
                                clipped.push_back(intersection);
                            }
                            if (current_distance >= 0.0f) clipped.push_back(current);
                            previous = current;
                            previous_distance = current_distance;
                        }
                        polygon.swap(clipped);
                    }
                    if (polygon.size() < 3u) continue;
                    double twice_area = 0.0;
                    for (std::size_t i = 0; i < polygon.size(); ++i) {
                        const auto& a = polygon[i];
                        const auto& b = polygon[(i + 1u) % polygon.size()];
                        if (a[3] <= 0.0f || b[3] <= 0.0f) {
                            twice_area = 0.0;
                            break;
                        }
                        const float ax = a[0] / a[3], ay = a[1] / a[3];
                        const float bx = b[0] / b[3], by = b[1] / b[3];
                        twice_area += static_cast<double>(ax) * by -
                                      static_cast<double>(bx) * ay;
                        min_x = std::min(min_x, ax); max_x = std::max(max_x, ax);
                        min_y = std::min(min_y, ay); max_y = std::max(max_y, ay);
                    }
                    const double triangle_area = std::abs(twice_area) * 0.5;
                    if (triangle_area > 1.0e-10) {
                        ++visible_triangles;
                        total_area += triangle_area;
                    }
                }
                std::fprintf(stderr,
                             "[native sly GL coverage] request=%#010x instance=%#llx source=%#010x vertices=%zu strip_triangles=%zu visible_triangles=%zu ndc_area=%g bbox=",
                             mesh.request,
                             static_cast<unsigned long long>(mesh.instance),
                             mesh.source_mesh, vertex_count,
                             vertex_count > 2u ? vertex_count - 2u : 0u,
                             visible_triangles, total_area);
                if (visible_triangles != 0u)
                    std::fprintf(stderr, "%g,%g..%g,%g\n", min_x, min_y,
                                 max_x, max_y);
                else
                    std::fprintf(stderr, "none\n");
            }
        }
        if (std::getenv("OPENRAC_DUMP_WORLD_MATRIX") != nullptr) {
            static unsigned notices = 0;
            std::uint32_t dump_request = 0x005e5400u;
            if (const char* setting = std::getenv("OPENRAC_DUMP_WORLD_MATRIX")) {
                if (setting[0] != '\0' && std::strcmp(setting, "1") != 0) {
                    char* end = nullptr;
                    const unsigned long parsed = std::strtoul(setting, &end, 0);
                    if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul)
                        dump_request = static_cast<std::uint32_t>(parsed);
                }
            }
            if (mesh.request == dump_request &&
                notices++ < 32u) {
                float min_x = 1.0e30f, min_y = 1.0e30f, min_z = 1.0e30f;
                float max_x = -1.0e30f, max_y = -1.0e30f, max_z = -1.0e30f;
                const std::size_t vertex_count = mesh.vertices.size() / 9u;
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const float x = mesh.vertices[i * 9u + 0u];
                    const float y = mesh.vertices[i * 9u + 1u];
                    const float z = mesh.vertices[i * 9u + 2u];
                    min_x = std::min(min_x, x); max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y); max_y = std::max(max_y, y);
                    min_z = std::min(min_z, z); max_z = std::max(max_z, z);
                }
                std::fprintf(stderr,
                             "[native sly uploaded matrix] request=%#010x instance=%#llx source=%#010x vertices=%zu mode=%d local_bounds=%g,%g,%g..%g,%g,%g model=",
                             dump_request,
                             static_cast<unsigned long long>(mesh.instance),
                             mesh.source_mesh, vertex_count, mvp_mode,
                             min_x, min_y, min_z, max_x, max_y, max_z);
                for (std::size_t i = 0; i < 16; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", mesh.model[i]);
                std::fprintf(stderr, " view_projection=");
                for (std::size_t i = 0; i < 16; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",",
                                 mesh.view_projection[i]);
                std::fprintf(stderr, " uploaded=");
                for (std::size_t i = 0; i < 16; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", m[i]);
                std::fprintf(stderr, " v0=");
                for (std::size_t i = 0; i < 9; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", mesh.vertices[i]);
                std::fputc('\n', stderr);
            }
        }
        glUniformMatrix4fv(g_world_mvp_location, 1, GL_FALSE, m);
        glUniform1i(g_world_texture_location, 0);
        glUniform1i(g_world_textured_location,
                    !debug_solid_colors && !debug_normal_colors &&
                    texture != 0 && mesh.tex0 != 0 ? 1 : 0);
        glUniform1i(g_world_tcc_location, tex0.tcc ? 1 : 0);
        glUniform1i(g_world_tfx_location, static_cast<GLint>(tex0.tfx));
        const float texture_alpha_scale = texture != 0 && g_textures != nullptr &&
                g_textures->alpha_scale(texture) == openrac::renderer::AlphaScale::Full
            ? 0.5f : 1.0f;
        glUniform1f(g_world_tex_alpha_scale_location, texture_alpha_scale);
        const bool debug_geometry = debug_solid_colors || missing_texture_fallback;
        glUniform1i(g_world_debug_geometry_location, debug_geometry ? 1 : 0);
        if (debug_geometry) {
            const std::uint32_t color_hash = static_cast<std::uint32_t>(
                mesh.instance ^ (mesh.instance >> 32u));
            const float color_floor = missing_texture_fallback ? 0.35f : 0.25f;
            const float color_range = 1.0f - color_floor;
            const float red = color_floor +
                static_cast<float>(color_hash & 0xFFu) * (color_range / 255.0f);
            const float green = color_floor +
                static_cast<float>((color_hash >> 8u) & 0xFFu) * (color_range / 255.0f);
            const float blue = color_floor +
                static_cast<float>((color_hash >> 16u) & 0xFFu) * (color_range / 255.0f);
            glUniform4f(g_world_debug_color_location, red, green, blue, 1.0f);
        }
        if (texture != 0 && mesh.tex0 != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_textures->gl_texture(texture));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            tex1.mmag ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            clamp.wms == openrac::renderer::gs::Wrap::Clamp
                                ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            clamp.wmt == openrac::renderer::gs::Wrap::Clamp
                                ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        }
        if (mesh.alpha_valid && !debug_geometry && !debug_normal_colors) {
            const auto alpha = openrac::renderer::gs::Alpha::decode(mesh.alpha);
            const openrac::renderer::GlBlend blend =
                openrac::renderer::translate_blend(alpha);
            glEnable(GL_BLEND);
            glBlendEquation(blend.equation);
            glBlendFuncSeparate(blend.src, blend.dst, GL_ONE, GL_ZERO);
            glBlendColor(0.0f, 0.0f, 0.0f, blend.constant);
            glUniform1i(g_world_source_scale_location,
                        static_cast<GLint>(blend.source_scale));
            glUniform1f(g_world_source_fix_location, blend.fix);
            if (!blend.exact && std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
                static std::unordered_set<std::uint64_t> blend_notices;
                if (blend_notices.insert(mesh.alpha).second)
                    std::fprintf(stderr,
                                 "[native sly blend approximated] alpha=%#llx function=%u,%u,%u,%u fix=%u\n",
                                 static_cast<unsigned long long>(mesh.alpha), alpha.a,
                                 alpha.b, alpha.c, alpha.d, alpha.fix);
            }
        } else {
            glDisable(GL_BLEND);
            glUniform1i(g_world_source_scale_location, 0);
            glUniform1f(g_world_source_fix_location, 0.0f);
        }
        const bool alpha_test_enabled = !debug_geometry && test.ate &&
            test.atst != openrac::renderer::gs::AlphaTest::Always &&
            test.atst != openrac::renderer::gs::AlphaTest::Never;
        glUniform1i(g_world_alpha_test_location,
                    alpha_test_enabled ? static_cast<GLint>(test.atst) : 0);
        glUniform1f(g_world_alpha_ref_location, static_cast<float>(test.aref));
        glUniform1i(g_world_alpha_keep_failing_location, 0);
        if (std::getenv("OPENRAC_SLY_WORLD_NO_DEPTH") != nullptr) {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        } else if (test.zte) {
            glEnable(GL_DEPTH_TEST);
            switch (test.ztst) {
                case openrac::renderer::gs::DepthTest::Never:
                    glDepthFunc(GL_NEVER);
                    break;
                case openrac::renderer::gs::DepthTest::Always:
                    glDepthFunc(GL_ALWAYS);
                    break;
                case openrac::renderer::gs::DepthTest::Gequal:
                    glDepthFunc(GL_GEQUAL);
                    break;
                case openrac::renderer::gs::DepthTest::Greater:
                    glDepthFunc(GL_GREATER);
                    break;
            }
            glDepthMask(!(test.ate &&
                          test.atst == openrac::renderer::gs::AlphaTest::Never &&
                          test.afail == openrac::renderer::gs::AlphaFail::FbOnly));
        } else {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        }
        auto draw_geometry = [&]() {
        if (debug_primitive == GL_POINTS) {
            // Isolate vertex placement from the still-unverified kind-5
            // triangle grouping. This remains opt-in and is useful for
            // checking whether captured positions form a coherent silhouette.
            // Unlike the normal draw path below, points do not need expanded
            // strip data, but they still need this mesh's vertices in the VBO.
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                         mesh.vertices.data(), GL_STREAM_DRAW);
            glEnable(GL_PROGRAM_POINT_SIZE);
            glDrawArrays(GL_POINTS, 0,
                         static_cast<GLsizei>(mesh.vertices.size() / 9u));
            glDisable(GL_PROGRAM_POINT_SIZE);
        } else if (std::getenv("OPENRAC_SLY_WORLD_RAW_TRIANGLES") != nullptr) {
            // Diagnostic comparison only: the source topology is not yet
            // represented in NativeWorldMesh, so callers must opt in.
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                         mesh.vertices.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertices.size() / 9u));
        } else if (debug_primitive == GL_TRIANGLES) {
            // Expand the source sequence as a triangle strip. GL_TRIANGLES over
            // the original vertices groups unrelated corners and is not a valid
            // strip-to-list comparison.
            std::vector<float> triangles;
            const std::size_t vertex_count = mesh.vertices.size() / 9u;
            if (vertex_count >= 3u)
                triangles.reserve((vertex_count - 2u) * 3u * 9u);
            for (std::size_t vertex = 2u; vertex < vertex_count; ++vertex) {
                const std::size_t a = (vertex & 1u) != 0u ? vertex - 1u : vertex - 2u;
                const std::size_t b = (vertex & 1u) != 0u ? vertex - 2u : vertex - 1u;
                for (const std::size_t index : {a, b, vertex}) {
                    const auto begin = mesh.vertices.begin() +
                        static_cast<std::ptrdiff_t>(index * 9u);
                    triangles.insert(triangles.end(), begin, begin + 9);
                }
            }
            // Opt-in player-only vertex scale (kind-5 character). The retained
            // kind-5 vertices are character-scale but project far too small;
            // scale just this mesh to find the correct retail VU factor.
            float player_scale = 1.0f;
            if (const char* setting = std::getenv("OPENRAC_PLAYER_VERTEX_SCALE")) {
                char* end = nullptr;
                const float parsed = std::strtof(setting, &end);
                if (end != setting && parsed > 0.0f && parsed <= 1000.0f)
                    player_scale = parsed;
            }
            if (player_scale != 1.0f) {
                for (std::size_t i = 0; i < triangles.size(); i += 9u) {
                    triangles[i] *= player_scale;
                    triangles[i + 1u] *= player_scale;
                    triangles[i + 2u] *= player_scale;
                }
            }
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(triangles.size() * sizeof(float)),
                         triangles.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0,
                         static_cast<GLsizei>(triangles.size() / 9u));
        } else {
            std::vector<float> scaled_vertices;
            const float* vertex_data = mesh.vertices.data();
            if (position_scale != 1.0f) {
                scaled_vertices = mesh.vertices;
                for (std::size_t i = 0; i < scaled_vertices.size(); i += 9u) {
                    scaled_vertices[i] *= position_scale;
                    scaled_vertices[i + 1u] *= position_scale;
                    scaled_vertices[i + 2u] *= position_scale;
                }
                vertex_data = scaled_vertices.data();
            }
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                         vertex_data, GL_STREAM_DRAW);
            const std::size_t vertex_count = mesh.vertices.size() / 9u;
            const bool restart_split =
                std::getenv("OPENRAC_SLY_WORLD_SPLIT_RESTARTS") != nullptr ||
                std::getenv("OPENRAC_SLY_WORLD_SPLIT_STRIPS") != nullptr;
            if (!restart_split) {
                // VertexRefs.flags bit 7 selects fog bias in the matched mesh
                // builder; FrameSource records select VU data transfers, not
                // primitive boundaries. Preserve the ordered vertex stream
                // as one strip unless an explicit topology diagnostic is set.
                glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(vertex_count));
            } else {
                // Diagnostic hypothesis only: a strip restart may repeat one
                // of the current run's first two vertices. Compare source
                // references instead of coordinates because distinct vertices
                // may share a point. The retail boundary convention is unknown.
                std::size_t run_begin = 0;
                std::size_t split_notices = 0;
                for (std::size_t vertex = 2; vertex + 1 < vertex_count; ++vertex) {
                    bool same_a = false, same_b = false;
                    if (mesh.vertex_indices.size() == vertex_count) {
                        same_a = mesh.vertex_indices[vertex] == mesh.vertex_indices[run_begin];
                        same_b = mesh.vertex_indices[vertex] ==
                            mesh.vertex_indices[run_begin + 1u];
                    } else {
                        const float* a = &mesh.vertices[vertex * 9u];
                        const float* b = &mesh.vertices[run_begin * 9u];
                        same_a = true;
                        same_b = true;
                        for (int c = 0; c < 3; ++c) {
                            if (a[c] != b[c]) same_a = false;
                            if (a[c] != mesh.vertices[(run_begin + 1u) * 9u +
                                                      static_cast<std::size_t>(c)])
                                same_b = false;
                        }
                    }
                    if ((same_a || same_b) && vertex - run_begin >= 3u) {
                        glDrawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(run_begin),
                                     static_cast<GLsizei>(vertex - run_begin));
                        if (std::getenv("OPENRAC_TRACE_SLY_STRIP_SPLITS") != nullptr &&
                            split_notices++ < 16u) {
                            const auto ref_at = mesh.vertex_indices.size() == vertex_count
                                ? mesh.vertex_indices[vertex] : 0xFFu;
                            const auto ref_start = mesh.vertex_indices.size() == vertex_count
                                ? mesh.vertex_indices[run_begin] : 0xFFu;
                            const auto ref_second = mesh.vertex_indices.size() == vertex_count
                                ? mesh.vertex_indices[run_begin + 1u] : 0xFFu;
                            std::fprintf(stderr,
                                         "[native sly strip split] source=%#010x at=%zu run=%zu refs=%u,%u,%u match=%u\n",
                                         mesh.source_mesh, vertex, run_begin,
                                         static_cast<unsigned>(ref_at),
                                         static_cast<unsigned>(ref_start),
                                         static_cast<unsigned>(ref_second), same_a ? 0u : 1u);
                        }
                        run_begin = vertex;
                    }
                }
                if (vertex_count - run_begin >= 3u)
                    glDrawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(run_begin),
                                 static_cast<GLsizei>(vertex_count - run_begin));
            }
        }
        };
        const auto alpha_fail = test.afail;
        if (!alpha_test_enabled ||
            test.atst == openrac::renderer::gs::AlphaTest::Always) {
            glUniform1i(g_world_alpha_test_location, 0);
            glUniform1i(g_world_alpha_keep_failing_location, 0);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            draw_geometry();
        } else if (test.atst == openrac::renderer::gs::AlphaTest::Never) {
            if (alpha_fail != openrac::renderer::gs::AlphaFail::Keep) {
                glColorMask(alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail == openrac::renderer::gs::AlphaFail::FbOnly);
                glDepthMask(alpha_fail == openrac::renderer::gs::AlphaFail::ZbOnly);
                glUniform1i(g_world_alpha_test_location, 0);
                draw_geometry();
            }
        } else {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(test.zte ? GL_TRUE : GL_FALSE);
            glUniform1i(g_world_alpha_test_location, static_cast<GLint>(test.atst));
            glUniform1i(g_world_alpha_keep_failing_location, 0);
            draw_geometry();
            if (alpha_fail != openrac::renderer::gs::AlphaFail::Keep) {
                glColorMask(alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail != openrac::renderer::gs::AlphaFail::ZbOnly,
                            alpha_fail == openrac::renderer::gs::AlphaFail::FbOnly);
                glDepthMask(alpha_fail == openrac::renderer::gs::AlphaFail::ZbOnly);
                glUniform1i(g_world_alpha_keep_failing_location, 1);
                draw_geometry();
            }
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    if (trace_native_per_mesh_bounds && !target_pass && projected_meshes != 0u)
        native_per_mesh_bounds_logged = true;
    if (trace_projected_bounds && present_bounds_sample < 64u) {
        std::fprintf(stderr,
                     "[native sly present bounds] frame=%u meshes=%zu unique=%zu duplicate=%u materials=%zu uploaded_textures=%zu missing_textures=%zu signature=%016llx projected=%u intersects=%u vertices=%llu inside=%llu ndc=%g,%g,%g..%g,%g,%g\n",
                     present_bounds_sample++, meshes.size(),
                     present_instances.size(), duplicate_instances,
                     present_materials.size(), present_uploaded_textures.size(),
                     present_missing_textures.size(),
                     static_cast<unsigned long long>(present_signature),
                     projected_meshes, intersecting_meshes, projected_vertices,
                     inside_vertices,
                     frame_min_x, frame_min_y, frame_min_z,
                     frame_max_x, frame_max_y, frame_max_z);
    }
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}

std::uint32_t guest_address(std::uint32_t address) {
    if (address >= GUEST_PORT && address - GUEST_PORT < GUEST_PORT_SIZE) return address;
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    if (address >= 0x70000000u && address < 0x70004000u) return address;
    if (address >= 0x20000000u && address < 0x22000000u) return address - 0x20000000u;
    if (address >= 0x30000000u && address < 0x32000000u) return address - 0x30000000u;
    // The native C++ build keeps addressed locals on the Linux stack and in
    // its own static storage, rather than in a translated guest RAM arena.
    // The 32-bit Linux stack used by the launcher is commonly mapped in the
    // 0xB0000000-0xEFFFFFFF range; preserve these native pointers so DMA chains
    // built in local storage are read from their actual host address.
    if (address >= 0xB0000000u && address < 0xF0000000u) return address;
    if ((address >= 0x08000000u && address < 0x10000000u) ||
        address >= 0xf0000000u) return address;
    return address & 0x01ffffffu;
#else
    return openrac_guest_ram_address(address);
#endif
}

bool guest_span(std::uint32_t address, std::size_t size, const std::uint8_t*& out) {
#if defined(OPENRAC_SLY1_DIRECT_RETAIL_MEMORY)
    const std::uint32_t physical = guest_address(address);
    if (size == 0 && physical == 0) {
        out = nullptr;
        return true;
    }
    const bool in_ram = physical >= 0x00010000u && physical <= 0x02000000u &&
        size <= static_cast<std::size_t>(0x02000000u - physical);
    const bool in_scratchpad = physical >= 0x70000000u && physical <= 0x70004000u &&
        size <= static_cast<std::size_t>(0x70004000u - physical);
    const bool in_port = physical >= GUEST_PORT && physical <= GUEST_PORT + GUEST_PORT_SIZE &&
        size <= static_cast<std::size_t>(GUEST_PORT + GUEST_PORT_SIZE - physical);
    const bool native_address =
        (physical >= 0x08000000u && physical < 0x10000000u) ||
        (physical >= 0xB0000000u && physical < 0xF0000000u) ||
        physical >= 0xf0000000u;
    const bool in_native_storage = in_port ||
        (native_address &&
         size <= static_cast<std::size_t>(0x100000000ull - physical));
    if (!in_ram && !in_scratchpad && !in_native_storage) return false;
    if (in_native_storage && size != 0) {
        const long page_size = ::sysconf(_SC_PAGESIZE);
        if (page_size <= 0) return false;
        const std::uintptr_t begin = static_cast<std::uintptr_t>(physical);
        const std::uintptr_t page_begin = begin - begin % static_cast<std::uintptr_t>(page_size);
        const std::uintptr_t end = begin + size;
        const std::size_t page_count =
            (end - page_begin + static_cast<std::uintptr_t>(page_size) - 1) /
            static_cast<std::uintptr_t>(page_size);
        std::vector<unsigned char> resident(page_count);
        if (::mincore(reinterpret_cast<void*>(page_begin), page_count *
                          static_cast<std::size_t>(page_size), resident.data()) != 0)
            return false;
    }
    out = reinterpret_cast<const std::uint8_t*>(physical);
    return true;
#else
    if (openrac_guest_base != nullptr && address >= GUEST_PORT &&
        address - GUEST_PORT <= GUEST_PORT_SIZE &&
        size <= static_cast<std::size_t>(GUEST_PORT_SIZE - (address - GUEST_PORT))) {
        out = openrac_guest_base + address;
        return true;
    }
    const std::uint32_t physical = guest_address(address);
    if (!openrac_guest_base || physical > openrac_guest_ram_size
        || size > static_cast<std::size_t>(openrac_guest_ram_size - physical)) return false;
    out = openrac_guest_base + physical;
    return true;
#endif
}

}

extern "C" void openrac_native_renderer_submit_dma_chain(std::uint32_t source_address) {
    constexpr std::size_t kMaxTags = 65536;
    constexpr std::size_t kMaxBytes = kMaxQueuedGifBytes;
    std::vector<std::uint8_t> stream;
    std::array<std::uint32_t, 16> returns{};
    std::size_t return_count = 0;
    std::uint32_t tag_address = guest_address(source_address);
    std::uint32_t failed_tag_address = tag_address;
    std::uint64_t failed_tag_low = 0;
    std::uint32_t failed_payload_address = 0;
    std::size_t failed_payload_size = 0;
    std::size_t tags_consumed = 0;
    const char* failure = "tag limit";
    g_dma_submit_count.fetch_add(1, std::memory_order_relaxed);
    bool ok = false;
    for (std::size_t tags = 0; tags < kMaxTags; ++tags) {
        tags_consumed = tags + 1;
        const std::uint8_t* tag = nullptr;
        failed_tag_address = tag_address;
        if (!guest_span(tag_address, 16, tag)) { failure = "tag span"; break; }
        std::uint64_t low;
        std::memcpy(&low, tag, 8);
        failed_tag_low = low;
        const std::size_t qwc = low & 0xffffu;
        const unsigned id = (low >> 28) & 7u;
        const std::uint32_t inline_data = tag_address + 16;
        const std::uint32_t raw_reference = static_cast<std::uint32_t>(low >> 32);
        // Port pointers use bit 31, which overlaps the DMAtag SPR flag.
        const std::uint32_t referenced =
            raw_reference >= GUEST_PORT && raw_reference - GUEST_PORT < GUEST_PORT_SIZE
                ? raw_reference
                : guest_address(raw_reference & 0x7fffffffu);
        std::uint32_t payload_address = inline_data;
        std::uint32_t next = inline_data;
        const std::size_t payload_size = qwc * 16;
        bool stop = false;
        bool valid = true;
        switch (id) {
        case 0: // REFE
            payload_address = referenced;
            stop = true;
            break;
        case 1: // CNT
            next += static_cast<std::uint32_t>(payload_size);
            break;
        case 2: // NEXT
            next = referenced;
            break;
        case 3: // REF
        case 4: // REFS
            payload_address = referenced;
            break;
        case 5: // CALL
            if (return_count == returns.size()) { valid = false; break; }
            returns[return_count++] = inline_data + static_cast<std::uint32_t>(payload_size);
            next = referenced;
            break;
        case 6: // RET
            if (return_count == 0) stop = true;
            else next = returns[--return_count];
            break;
        case 7: // END
            stop = true;
            break;
        }
        if (!valid) { failure = "call depth"; break; }
        const std::uint8_t* payload = nullptr;
        failed_payload_address = payload_address;
        failed_payload_size = payload_size;
        if (payload_size > kMaxBytes - stream.size()) {
            failure = "byte limit";
            break;
        }
        if (!guest_span(payload_address, payload_size, payload)) {
            failure = "payload span";
            break;
        }
        static const std::uint32_t trace_target = [] {
            const char* value = std::getenv("OPENRAC_TRACE_DMA_TARGET");
            if (value == nullptr || *value == '\0') return 0u;
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(value, &end, 0);
            if (end == value || *end != '\0' || parsed > UINT32_MAX) return 0u;
            return static_cast<std::uint32_t>(parsed);
        }();
        if (trace_target != 0 && payload_size != 0 && trace_target >= payload_address &&
            trace_target - payload_address < payload_size) {
            const std::size_t target_offset = trace_target - payload_address;
            const std::uint8_t* target = payload + target_offset;
            const std::size_t preceding_size = std::min<std::size_t>(stream.size(), 80);
            std::array<char, 161> preceding{};
            for (std::size_t i = 0; i < preceding_size; ++i) {
                std::snprintf(preceding.data() + i * 2, preceding.size() - i * 2,
                              "%02x", stream[stream.size() - preceding_size + i]);
            }
            std::fprintf(stderr,
                         "[native dma target] chain=%#010x tag=%#010x id=%u qwc=%zu raw_ref=%#010x payload=%#010x bytes=%zu stream_offset=%zu target=%#010x target_offset=%zu bytes_at_target=%02x%02x%02x%02x preceding=%s\n",
                         source_address, tag_address, id, qwc, raw_reference,
                         payload_address, payload_size, stream.size(), trace_target,
                         target_offset, target[0], target_offset + 1 < payload_size ? target[1] : 0,
                         target_offset + 2 < payload_size ? target[2] : 0,
                         target_offset + 3 < payload_size ? target[3] : 0,
                         preceding.data());
        }
        if (payload_size >= 4096u && std::getenv("OPENRAC_TRACE_DMA_PAYLOAD") != nullptr) {
            static std::atomic<unsigned> payload_traces{0};
            const unsigned index = payload_traces.fetch_add(1, std::memory_order_relaxed);
            if (index < 64) {
                std::fprintf(stderr,
                             "[native dma payload] #%u tag=%#010x id=%u qwc=%zu raw_ref=%#010x source=%#010x bytes=%zu stream_offset=%zu first=%02x%02x%02x%02x\n",
                             index + 1, tag_address, id, qwc, raw_reference, payload_address,
                             payload_size, stream.size(), payload_size > 0 ? payload[0] : 0,
                             payload_size > 1 ? payload[1] : 0,
                             payload_size > 2 ? payload[2] : 0,
                             payload_size > 3 ? payload[3] : 0);
            }
        }
        if (payload_size != 0) stream.insert(stream.end(), payload, payload + payload_size);
        if (stop) {
            ok = true;
            break;
        }
        tag_address = next;
    }
    if (!ok) {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        const unsigned notice = g_dma_reject_notices.fetch_add(1, std::memory_order_relaxed);
        if (notice < 12) {
            const std::uint32_t first = stream.size() >= 4
                ? (static_cast<std::uint32_t>(stream[0]) |
                   (static_cast<std::uint32_t>(stream[1]) << 8) |
                   (static_cast<std::uint32_t>(stream[2]) << 16) |
                   (static_cast<std::uint32_t>(stream[3]) << 24))
                : 0;
            std::uint64_t adjacent = 0;
            std::uint64_t preceding = 0;
            const std::uint8_t* adjacent_bytes = nullptr;
            if (guest_span(failed_tag_address + 4, 8, adjacent_bytes)) {
                std::memcpy(&adjacent, adjacent_bytes, 8);
            }
            const std::uint8_t* preceding_bytes = nullptr;
            if (failed_tag_address >= 4 &&
                guest_span(failed_tag_address - 4, 8, preceding_bytes)) {
                std::memcpy(&preceding, preceding_bytes, 8);
            }
            openrac::log::warn(
                "native GIF chain rejected: source={:#010x} tag={:#010x} preceding={:#018x} low={:#018x} adjacent={:#018x} id={} qwc={} payload={:#010x}/{} reason={} bytes={} first_word={:#010x}",
                source_address, failed_tag_address, preceding, failed_tag_low,
                adjacent,
                static_cast<unsigned>((failed_tag_low >> 28) & 7u),
                static_cast<unsigned>(failed_tag_low & 0xffffu),
                failed_payload_address, failed_payload_size, failure, stream.size(), first);
        }
        return;
    }
    trace_gs_register_inputs(stream.data(), stream.size(), source_address, "dma-chain");
    {
        std::scoped_lock lock(g_packets_mutex);
        if (!gif_queue_has_room_locked(stream.size(), kMaxBytes)) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), stream.begin(), stream.end());
        openrac_trace_accepted_kick_xyz(stream.data(), stream.size(), "gif-dma-chain");
        trace_gif_queue_append("dma-chain", stream.size(), source_address);
    }
    if (std::getenv("OPENRAC_TRACE_DMA_CALL") != nullptr) {
        static std::atomic<unsigned> chain_trace_count{0};
        const unsigned index = chain_trace_count.fetch_add(1, std::memory_order_relaxed);
        if (index < 64) {
            std::uint64_t first = 0;
            std::uint64_t second = 0;
            if (stream.size() >= sizeof(first))
                std::memcpy(&first, stream.data(), sizeof(first));
            if (stream.size() >= sizeof(first) + sizeof(second))
                std::memcpy(&second, stream.data() + sizeof(first), sizeof(second));
            std::fprintf(stderr,
                         "[native dma chain] #%u source=%#010x tags=%zu bytes=%zu first=%#018llx second=%#018llx\n",
                         index + 1, source_address, tags_consumed, stream.size(),
                         static_cast<unsigned long long>(first),
                         static_cast<unsigned long long>(second));
        }
    }
    g_dma_accept_count.fetch_add(1, std::memory_order_relaxed);
    g_dma_bytes.fetch_add(stream.size(), std::memory_order_relaxed);
    // Native GIF DMA completes synchronously. Latch the GS SIGNAL interrupt
    // before dispatching it to the game's registered callback.
    openrac_guest_gs_signal_interrupt();
}

extern "C" void openrac_native_renderer_trace_dma_call(std::uint32_t channel_address,
                                                          std::uint32_t source_address) {
    if (std::getenv("OPENRAC_TRACE_DMA_CALL") == nullptr) return;
    static std::atomic<unsigned> trace_count{0};
    const unsigned index = trace_count.fetch_add(1, std::memory_order_relaxed);
    if (index < 64) {
        std::fprintf(stderr, "[native dma call] #%u channel=%#010x source=%#010x\n",
                     index + 1, channel_address, source_address);
    }
    if (channel_address == 0x10008000u && source_address != 0) {
        static std::atomic<unsigned> vif0_trace_count{0};
        const unsigned vif0_index = vif0_trace_count.fetch_add(1, std::memory_order_relaxed);
        if (vif0_index < 8) {
            std::uint32_t tag_address = guest_address(source_address);
            for (unsigned tag_index = 0; tag_index < 8; ++tag_index) {
                const std::uint8_t* tag = nullptr;
                std::uint64_t low = 0;
                if (!guest_span(tag_address, 16, tag)) break;
                std::memcpy(&low, tag, sizeof(low));
                const std::uint32_t qwc = static_cast<std::uint32_t>(low & 0xFFFFu);
                const unsigned id = static_cast<unsigned>((low >> 28) & 7u);
                const std::uint32_t reference = static_cast<std::uint32_t>(
                    (low >> 32) & 0x7FFFFFFFu);
                std::fprintf(stderr,
                             "[native vif0 chain] #%u tag=%u address=%#010x id=%u qwc=%u reference=%#010x\n",
                             vif0_index + 1, tag_index, tag_address, id, qwc,
                             reference);
                if (tag_index == 0 && id == 1u) {
                    const std::uint8_t* payload = nullptr;
                    const std::size_t payload_size = static_cast<std::size_t>(qwc) * 16u;
                    if (guest_span(tag_address + 16u, payload_size, payload)) {
                        for (std::size_t word = 0; word < 16u && word * 4u < payload_size; ++word) {
                            std::uint32_t value = 0;
                            std::memcpy(&value, payload + word * 4u, sizeof(value));
                            std::fprintf(stderr, "[native vif0 data] #%u word=%zu value=%#010x\n",
                                         vif0_index + 1, word, value);
                        }
                    }
                }
                if (id == 1u) {
                    tag_address += 16u + qwc * 16u;
                } else if (id == 2u || id == 5u) {
                    tag_address = guest_address(reference);
                } else if (id == 3u || id == 4u) {
                    tag_address += 16u;
                } else {
                    break;
                }
            }
        }
    }
}

/* Receive the GIF stream after the native VIF1/VU1 path has expanded the
 * display list. The direct renderer consumes ordered GIF bytes at present. */
extern "C" void openrac_native_renderer_append_gif_stream(const void* source,
                                                           std::size_t size,
                                                           const char* origin) {
    if (source == nullptr || size == 0) return;
    std::scoped_lock lock(g_packets_mutex);
    if (!gif_queue_has_room_locked(size, kMaxQueuedGifBytes)) {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        static std::atomic<unsigned> overflow_notices{0};
        if (overflow_notices.fetch_add(1, std::memory_order_relaxed) < 4u) {
            openrac::log::warn(
                "native GIF packet queue full: queued={} incoming={} limit={}",
                g_packets.size(), size, kMaxQueuedGifBytes);
        }
        return;
    }
    g_packets.insert(g_packets.end(), static_cast<const std::uint8_t*>(source),
                     static_cast<const std::uint8_t*>(source) + size);
    trace_gif_queue_append(origin != nullptr ? origin : "gif-stream-callback", size, 0,
                           source);
}

extern "C" void openrac_native_renderer_submit_gif_stream(std::uint32_t source_address) {
    constexpr std::size_t kMaxBytes = kMaxQueuedGifBytes;
    g_dma_submit_count.fetch_add(1, std::memory_order_relaxed);
    const char* failure = "unknown";
    const std::uint8_t* first_tag = nullptr;
    std::uint64_t first_lo = 0;
    std::uint64_t first_hi = 0;
    if (!guest_span(source_address, 16, first_tag)) {
        failure = "tag span";
    } else {
        std::memcpy(&first_lo, first_tag, 8);
        std::memcpy(&first_hi, first_tag + 8, 8);
    }
    const std::size_t qwc = static_cast<std::size_t>(first_lo & 0x7fffu) + 1;
    const std::size_t gif_size = qwc * 16;
    const std::uint8_t* gif = nullptr;
    if (failure == "unknown" && gif_size > kMaxBytes) failure = "byte limit";
    if (failure == "unknown" && !guest_span(source_address, gif_size, gif))
        failure = "packet span";
    if (failure != "unknown") {
        g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
        const unsigned notice = g_dma_reject_notices.fetch_add(1, std::memory_order_relaxed);
        if (notice < 12)
            openrac::log::warn("native GIF stream rejected: source={:#010x} tag={:#018x}/{:#018x} reason={}",
                               source_address, first_lo, first_hi, failure);
        return;
    }
    trace_gs_register_inputs(gif, gif_size, source_address, "gif-stream");
    {
        std::scoped_lock lock(g_packets_mutex);
        if (!gif_queue_has_room_locked(gif_size, kMaxBytes)) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), gif, gif + gif_size);
        trace_gif_queue_append("gif-stream-submit", gif_size, source_address);
    }
    g_dma_accept_count.fetch_add(1, std::memory_order_relaxed);
    g_dma_bytes.fetch_add(gif_size, std::memory_order_relaxed);
    openrac_guest_gs_signal_interrupt();
}

extern "C" std::uint64_t openrac_native_renderer_dma_submit_count(void) {
    return g_dma_submit_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_accept_count(void) {
    return g_dma_accept_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_reject_count(void) {
    return g_dma_reject_count.load(std::memory_order_relaxed);
}

extern "C" std::uint64_t openrac_native_renderer_dma_bytes(void) {
    return g_dma_bytes.load(std::memory_order_relaxed);
}

extern "C" void openrac_native_sly_world_mesh_submit(
    std::uint32_t world, std::uint64_t instance, const float* vertices,
    std::uint32_t vertex_count, std::uint32_t request, std::uint32_t source_mesh,
    std::uint32_t target_tbp, int target_width, int target_height, int render_target,
    const std::uint8_t* vertex_indices,
    const float* model, const float* view_projection,
    std::uint64_t tex0, std::uint64_t tex1, std::uint64_t clamp,
    std::uint64_t alpha, int alpha_valid, std::uint64_t test) {
    // Diagnostic filter for isolating one guest mesh from scene occlusion and
    // the experimental overlay's other approximations. Unset by default.
    if (const char* setting = std::getenv("OPENRAC_SLY_WORLD_ONLY_SOURCE")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul &&
            source_mesh != static_cast<std::uint32_t>(parsed))
            return;
    }
    if (world == 0 || vertices == nullptr || vertex_count < 3 || vertex_count > 4096 ||
        model == nullptr || view_projection == nullptr)
        return;
    NativeWorldMesh draw;
    draw.instance = instance;
    draw.request = request;
    draw.source_mesh = source_mesh;
    draw.target_tbp = target_tbp;
    draw.target_width = target_width;
    draw.target_height = target_height;
    draw.render_target = render_target != 0;
    draw.vertices.assign(vertices, vertices + static_cast<std::size_t>(vertex_count) * 9u);
    if (vertex_indices != nullptr)
        draw.vertex_indices.assign(vertex_indices, vertex_indices + vertex_count);
    draw.tex0 = tex0;
    draw.tex1 = tex1;
    draw.clamp = clamp;
    draw.alpha = alpha;
    draw.alpha_valid = alpha_valid != 0;
    draw.test = test;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            float value = 0.0f;
            for (std::size_t k = 0; k < 4; ++k)
                value += model[row * 4u + k] * view_projection[k * 4u + column];
            if (!std::isfinite(value)) return;
            draw.mvp[row * 4u + column] = value;
        }
    }
    draw.model_is_identity = true;
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            if (model[row * 4u + column] != (row == column ? 1.0f : 0.0f))
                draw.model_is_identity = false;
    std::memcpy(draw.model.data(), model, sizeof(draw.model));
    std::memcpy(draw.view_projection.data(), view_projection, sizeof(draw.view_projection));
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD_BOUNDS") != nullptr) {
        static unsigned samples = 0;
        const unsigned sample = samples++;
        if (sample < 2048u) {
            float min_x = 1.0e30f, min_y = 1.0e30f, min_z = 1.0e30f;
            float max_x = -1.0e30f, max_y = -1.0e30f, max_z = -1.0e30f;
            float min_local[3]{1.0e30f, 1.0e30f, 1.0e30f};
            float max_local[3]{-1.0e30f, -1.0e30f, -1.0e30f};
            unsigned projected = 0, inside = 0;
            for (std::uint32_t i = 0; i < vertex_count; ++i) {
                const float x = vertices[i * 9u + 0u];
                const float y = vertices[i * 9u + 1u];
                const float z = vertices[i * 9u + 2u];
                min_local[0] = std::min(min_local[0], x);
                min_local[1] = std::min(min_local[1], y);
                min_local[2] = std::min(min_local[2], z);
                max_local[0] = std::max(max_local[0], x);
                max_local[1] = std::max(max_local[1], y);
                max_local[2] = std::max(max_local[2], z);
                float clip[4]{};
                for (std::size_t row = 0; row < 4; ++row) {
                    clip[row] = draw.mvp[row] * x + draw.mvp[4u + row] * y +
                                draw.mvp[8u + row] * z + draw.mvp[12u + row];
                }
                if (!std::isfinite(clip[3]) || std::abs(clip[3]) < 1.0e-6f) continue;
                const float nx = clip[0] / clip[3];
                const float ny = clip[1] / clip[3];
                const float nz = clip[2] / clip[3];
                if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) continue;
                ++projected;
                if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f &&
                    nz >= -1.0f && nz <= 1.0f) ++inside;
                min_x = std::min(min_x, nx); max_x = std::max(max_x, nx);
                min_y = std::min(min_y, ny); max_y = std::max(max_y, ny);
                min_z = std::min(min_z, nz); max_z = std::max(max_z, nz);
            }
            std::fprintf(stderr,
                         "[native sly projected bounds] source=%#010x kind=%u instance=%#llx vertices=%u projected=%u inside=%u local=%g,%g,%g..%g,%g,%g ndc=%g,%g..%g,%g z=%g..%g\n",
                         draw.source_mesh,
                         draw.source_mesh >= 0x00010000u && draw.source_mesh < 0x02000000u
                             ? *reinterpret_cast<const volatile std::uint8_t*>(draw.source_mesh)
                             : 0xFFu,
                         static_cast<unsigned long long>(instance), vertex_count,
                         projected, inside, min_local[0], min_local[1], min_local[2],
                         max_local[0], max_local[1], max_local[2],
                         min_x, min_y, max_x, max_y, min_z, max_z);
            if (sample < 2048u) {
                std::fprintf(stderr, "[native sly transform] instance=%#llx model rows=",
                             static_cast<unsigned long long>(instance));
                for (std::size_t i = 0; i < 16; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", model[i]);
                std::fprintf(stderr, " view_projection rows=");
                for (std::size_t i = 0; i < 16; ++i)
                    std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", view_projection[i]);
                std::fputc('\n', stderr);
            }
        }
    }
    std::scoped_lock lock(g_world_mesh_mutex);
    if (g_world_mesh_world != world) {
        g_world_meshes.clear();
        g_world_mesh_replacements = 0;
        g_world_mesh_world = world;
    }
    const auto previous = std::find_if(
        g_world_meshes.begin(), g_world_meshes.end(),
        [&draw](const NativeWorldMesh& queued) { return queued.instance == draw.instance; });
    if (previous != g_world_meshes.end()) {
        g_world_meshes.erase(previous);
        ++g_world_mesh_replacements;
    }
    g_world_meshes.push_back(std::move(draw));
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static unsigned notices = 0;
        if (notices++ < 16u)
            std::fprintf(stderr, "[native sly world queued] world=%#010x instance=%#llx vertices=%u draws=%zu\n",
                         world, static_cast<unsigned long long>(instance), vertex_count, g_world_meshes.size());
    }
}

extern "C" int openrac_native_window_create(void) {
    openrac::platform::set_app_metadata("OpenThievius - Sly 1", "open-thievius-sly1");
    g_platform = std::make_unique<openrac::platform::Platform>(
        openrac::platform::kVideo | openrac::platform::kGamepad);
    if (!g_platform->ok()) { g_platform.reset(); return 0; }
    g_input = std::make_unique<sly1::platform::Input>();
    openrac::platform::WindowConfig config;
    config.title = "OpenThievius - Sly 1";
    config.width = 960;
    config.height = 720;
    // The host32 launcher paces the guest and simulated VBlank at exact NTSC
    // rate; tying swaps to monitor refresh would add a second, mismatched clock.
    config.vsync = false;
    config.software = openrac::platform::SoftwareRenderer::Allow;
    config.hidden = std::getenv("OPENRAC_TEST_HIDDEN_WINDOW") != nullptr;
    std::string error;
    g_window = openrac::platform::Window::open(config, error);
    if (!g_window) { g_platform.reset(); return 0; }

    const auto load = openrac::platform::Window::gl_loader();
    if (!openrac::gl::load(load, error)) { g_window.reset(); g_platform.reset(); return 0; }
    g_clear_color = reinterpret_cast<ClearColor>(load("glClearColor"));
    g_clear = reinterpret_cast<Clear>(load("glClear"));
    g_viewport = reinterpret_cast<Viewport>(load("glViewport"));
    if (!g_clear_color || !g_clear || !g_viewport) { g_window.reset(); g_platform.reset(); return 0; }

    g_textures = std::make_unique<openrac::renderer::TexturePool>();
    openrac::renderer::RenderState state{*g_textures, {0, 960, 720}};
    if (const char* setting = std::getenv("OPENRAC_SLY_DRAW_BLOCK")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFu)
            openrac::renderer::set_draw_buffer_block(static_cast<std::uint32_t>(parsed), 416);
    }
    g_direct = std::make_unique<openrac::renderer::DirectRenderer>(
        "sly1-direct", openrac::renderer::Bucket::Hud,
        openrac::renderer::DirectConfig{640, 224, 0},
        openrac::renderer::DirectRenderer::Input::Gif);
    if (!g_direct->init(state, error)) {
        g_direct.reset(); g_textures.reset(); g_window.reset(); g_platform.reset(); return 0;
    }
    if (!init_world_renderer(error)) {
        openrac::log::error("Sly world renderer startup failed: {}", error);
        g_world_shader.reset();
    }
    if (!init_movie_renderer(error)) {
        openrac::log::error("Sly movie frame renderer startup failed: {}", error);
        g_movie_shader.reset();
    }
    return 1;
}

extern "C" void openrac_native_movie_frame(const std::uint8_t* rgba,
                                             int width, int height) {
    if (rgba == nullptr || width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return;
    const std::size_t size = static_cast<std::size_t>(width) *
                             static_cast<std::size_t>(height) * 4u;
    auto frame = std::make_shared<std::vector<std::uint8_t>>(rgba, rgba + size);
    std::scoped_lock lock(g_movie_frame_mutex);
    g_movie_frame = std::move(frame);
    g_movie_frame_width = width;
    g_movie_frame_height = height;
    ++g_movie_frame_serial;
    g_movie_frame_active = true;
}

extern "C" void openrac_native_movie_end(void) {
    std::scoped_lock lock(g_movie_frame_mutex);
    g_movie_frame_active = false;
    g_movie_frame.reset();
}

extern "C" int openrac_native_window_poll_quit(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return 1;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) return 1;
    }
    return 0;
}

// Menu state accessor from the host32 main translation unit (opt-in menu driver).
extern "C" unsigned int sly1_native_menu_page_and_state(void) __attribute__((weak));

extern "C" int openrac_native_window_poll_input(void) {
    if (!g_input) return 1;
    SDL_Event event;
    bool quit = false;
    static unsigned traced_key_events = 0;
    static std::array<unsigned, 8> test_cross_frames{};
    static unsigned test_cross_count = 0;
    static unsigned test_cross_next = 0;
    static unsigned test_frame = 0;
    static bool test_cross_checked = false;
    std::scoped_lock lock(g_input_mutex);
    while (SDL_PollEvent(&event)) {
        static unsigned traced_window_events = 0;
        if (std::getenv("OPENRAC_TRACE_PAD") != nullptr &&
            event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && traced_window_events < 16u) {
            std::fprintf(stderr, "[window-focus] gained window=%u keyboard_focus=%p\n",
                         event.window.windowID, static_cast<void*>(SDL_GetKeyboardFocus()));
            ++traced_window_events;
        }
        if (std::getenv("OPENRAC_TRACE_PAD") != nullptr &&
            (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) &&
            traced_key_events < 64u) {
            std::fprintf(stderr,
                         "[key] type=%s key=%#x scancode=%u repeat=%u focus=%p\n",
                         event.type == SDL_EVENT_KEY_DOWN ? "down" : "up",
                         static_cast<unsigned>(event.key.key),
                         static_cast<unsigned>(event.key.scancode),
                         event.key.repeat ? 1u : 0u,
                         static_cast<void*>(SDL_GetKeyboardFocus()));
            ++traced_key_events;
        }
        if (event.type == SDL_EVENT_QUIT ||
            (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) quit = true;
        g_input->handle_event(event);
    }
    if (!test_cross_checked) {
        test_cross_checked = true;
        const char* setting = std::getenv("OPENRAC_TEST_PRESS_CROSS_AT_FRAMES");
        if (setting == nullptr) setting = std::getenv("OPENRAC_TEST_PRESS_SPACE_AFTER_FRAMES");
        const char* cursor = setting;
        while (cursor != nullptr && *cursor != '\0' && test_cross_count < test_cross_frames.size()) {
            char* end = nullptr;
            const unsigned long value = std::strtoul(cursor, &end, 10);
            if (end == cursor || value == 0 || value > 10000000ul) break;
            test_cross_frames[test_cross_count++] = static_cast<unsigned>(value);
            if (*end != ',') break;
            cursor = end + 1;
        }
    }
    ++test_frame;
    // Opt-in Up+Cross sequence (bitmask: 0x10=Up, 0x4000=Cross, 0x08=Start)
    // at native frames, for driving menus in hidden/stable mode.
    {
        static std::vector<unsigned> seq_frames;
        static std::vector<unsigned> seq_buttons;
        static std::vector<unsigned> seq_hold_frames;
        static bool seq_parsed = false;
        static std::size_t seq_next = 0;
        if (!seq_parsed) {
            seq_parsed = true;
            const char* s = std::getenv("OPENRAC_TEST_BUTTONS_AT_FRAMES");
            if (s != nullptr) {
                // format: frame:mask[:hold],frame:mask[:hold],...
                const char* p = s;
                while (*p != '\0' && seq_frames.size() < 32u) {
                    char* end = nullptr;
                    const unsigned long f = std::strtoul(p, &end, 0);
                    if (end == p || *end != ':') break;
                    p = end + 1;
                    const unsigned long m = std::strtoul(p, &end, 0);
                    if (end == p) break;
                    unsigned long hold = 1;
                    p = end;
                    if (*p == ':') {
                        ++p;
                        hold = std::strtoul(p, &end, 10);
                        if (end == p || hold == 0 || hold > 1000000ul) break;
                    }
                    seq_frames.push_back(static_cast<unsigned>(f));
                    seq_buttons.push_back(static_cast<unsigned>(m));
                    seq_hold_frames.push_back(static_cast<unsigned>(hold));
                    p = end;
                    if (*p == ',') ++p; else break;
                }
            }
        }
        while (seq_next < seq_frames.size() && test_frame >= seq_frames[seq_next]) {
            g_input->test_press_buttons_once(seq_buttons[seq_next],
                                             static_cast<int>(seq_hold_frames[seq_next]));
            std::fprintf(stderr,
                         "[test-input] injected buttons %#x for %u frames at native frame %u\n",
                         seq_buttons[seq_next], seq_hold_frames[seq_next], test_frame);
            ++seq_next;
        }
    }
    // Wait for the save-file prompt before selecting Yes. Frame scheduled
    // presses can pass before the slower game-thread input poll samples them.
    if (std::getenv("OPENRAC_TEST_SELECT_SAVE_YES") != nullptr &&
        sly1_native_menu_page_and_state != nullptr) {
        static bool save_yes_sent = false;
        const unsigned int packed = sly1_native_menu_page_and_state();
        const unsigned int page = packed & 0xFFu;
        const unsigned int uistate = (packed >> 8u) & 0xFFu;
        const unsigned int open = (packed >> 16u) & 0xFFu;
        if (!save_yes_sent && open != 0u && page == 7u && uistate == 5u) {
            g_input->test_press_buttons_once(0x4010u, 30);
            save_yes_sent = true;
            std::fprintf(stderr, "[test-input] selected save Yes on page 7\n");
        }
    }
    while (test_cross_next < test_cross_count && test_frame >= test_cross_frames[test_cross_next]) {
        g_input->test_press_cross_once();
        std::fprintf(stderr, "[test-input] injected Cross press %u at native frame %u\n",
                     test_cross_next + 1, test_frame);
        ++test_cross_next;
    }
    // State-driven menu driver: inject button presses when the game reaches a
    // given pause page, rather than at host-frame numbers (the game thread runs
    // much slower than the present loop). Cycle a short Up/Cross sequence on the
    // save-file prompt (page 7) to confirm it, then start on a page that looks
    // like gameplay. Gated by OPENRAC_TEST_DRIVE_MENU.
    if (std::getenv("OPENRAC_TEST_DRIVE_MENU") != nullptr &&
        sly1_native_menu_page_and_state != nullptr) {
        static unsigned int last_driven_page = 0xFFFFFFFFu;
        static unsigned int drive_step = 0;
        const unsigned int packed = sly1_native_menu_page_and_state();
        const unsigned int page = packed & 0xFFu;
        const unsigned int uistate = (packed >> 8u) & 0xFFu;
        if (page != last_driven_page) {
            last_driven_page = page;
            drive_step = 0;
            std::fprintf(stderr, "[menu-driver] page changed to %u ui=%u\n", page, uistate);
        }
        // Button bitmask: 0x10=Up, 0x4000=Cross, 0x08=Start. On page 7 replay
        // the windowed sequence that reliably reaches gameplay (Cross, Up,
        // Cross, Start, Cross, Down, Cross, Start). Hold each press ~30 host
        // frames (~0.5s) so the game thread's ~3 fps pad poll cannot miss it.
        constexpr unsigned int kSequence[] = {
            0x4000u, 0x10u, 0x4000u, 0x08u,
            0x4000u, 0x40u, 0x4000u, 0x08u};
        constexpr unsigned kStepFrames = 40u;
        constexpr unsigned kHoldFrames = 30u;
        if ((test_frame % kStepFrames) == 0u) {
            unsigned int buttons = 0;
            if (page == 7u || (uistate == 5u && page != 0u)) {
                buttons = kSequence[drive_step % (sizeof(kSequence) / sizeof(kSequence[0]))];
            } else if (uistate == 5u) {
                buttons = 0x08u;  // Start to dismiss a remaining menu
            }
            if (buttons != 0) {
                g_input->test_press_buttons_once(buttons, kHoldFrames);
                std::fprintf(stderr, "[menu-driver] inject %#x at page %u step %u\n",
                             buttons, page, drive_step);
            }
            ++drive_step;
        }
    }
    g_input->update();
    return quit ? 1 : 0;
}

extern "C" int openrac_native_pad_read(int port, unsigned char* report, unsigned int size) {
    std::scoped_lock lock(g_input_mutex);
    if (!g_input || report == nullptr || port < 0 || port >= 2 || size < 32) return 0;
    std::span<unsigned char, 32> output(report, 32);
    g_input->pad(port).write_read_buffer(output);
    if (std::getenv("OPENRAC_TRACE_PAD") != nullptr) {
        static std::array<std::uint16_t, 2> previous_buttons{0xFFFFu, 0xFFFFu};
        static std::array<unsigned char, 2> previous_status{0xFFu, 0xFFu};
        const std::uint16_t buttons = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(report[2]) << 8u) |
            static_cast<std::uint16_t>(report[3]));
        if (report[0] != previous_status[static_cast<std::size_t>(port)] ||
            buttons != previous_buttons[static_cast<std::size_t>(port)]) {
            std::fprintf(stderr,
                         "[pad] port=%d status=%02x held=%04x axes=%02x,%02x,%02x,%02x\n",
                         port, report[0], static_cast<std::uint16_t>(~buttons), report[4],
                         report[5], report[6], report[7]);
            previous_status[static_cast<std::size_t>(port)] = report[0];
            previous_buttons[static_cast<std::size_t>(port)] = buttons;
        }
    }
    return 1;
}

extern "C" std::size_t openrac_native_vu_ndc_copy(float* out, std::size_t max_floats);
extern "C" std::size_t openrac_native_vu_ndc_copy_full(float* out, std::size_t max_floats,
                                                       unsigned* meta, std::size_t max_meta);
extern "C" std::size_t openrac_native_vu_ndc_copy_group(float* out, std::size_t max_floats);
extern "C" std::size_t openrac_native_vu_ndc_take(float* out, std::size_t max_floats,
                                                  unsigned* meta, std::size_t max_meta);

static void dump_vu_ndc_points(int width, int height) {
    static bool done = false;
    if (done) return;
    if (std::getenv("OPENRAC_DUMP_VU_NDC") == nullptr) return;
    std::vector<float> data(65536u * 4u);
    std::vector<unsigned> meta(65536u * 4u);
    const std::size_t floats = openrac_native_vu_ndc_copy_full(
        data.data(), data.size(), meta.data(), meta.size());
    if (floats < 4000u) return;
    if (FILE* csv = std::fopen("/tmp/sly-vu-ndc.csv", "wb")) {
        for (std::size_t i = 0; i + 3u < floats; i += 4u) {
            const std::size_t vi = i / 4u;
            std::fprintf(csv, "%u,%u,%u,%u,%g,%g,%g,%g\n", meta[vi * 4u],
                         meta[vi * 4u + 1u], meta[vi * 4u + 2u],
                         meta[vi * 4u + 3u], data[i], data[i + 1u], data[i + 2u], data[i + 3u]);
        }
        std::fclose(csv);
    }
    done = true;
    const char* path = std::getenv("OPENRAC_DUMP_VU_NDC_PATH");
    if (path == nullptr || path[0] == '\0') path = "/tmp/sly-vu-ndc.ppm";
    if (FILE* file = std::fopen(path, "wb")) {
        std::fprintf(file, "P6 %d %d 255\n", width, height);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u, 0);
        for (std::size_t i = 0; i + 3u < floats; i += 4u) {
            const int px = static_cast<int>((data[i] / data[i + 3u] + 1.0f) * 0.5f * width);
            const int py = static_cast<int>((1.0f - data[i + 1u] / data[i + 3u]) * 0.5f * height);
            if (px < 0 || py < 0 || px >= width || py >= height) continue;
            const std::size_t at = (static_cast<std::size_t>(py) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px)) * 3u;
            pixels[at] = 255; pixels[at + 1u] = 255; pixels[at + 2u] = 255;
        }
        std::fwrite(pixels.data(), 1, pixels.size(), file);
        std::fclose(file);
        std::fprintf(stderr, "[native sly vu ndc dump] path=%s vertices=%zu\n", path, floats / 3u);
    }
}

// Port-side replacement for the retail VU1 vertex-packet producer: turn a
// captured model's transformed vertices into a GIF packet with PRIM 4
// (triangle strip) and XYZ2 register writes, then feed the native DirectRenderer.
static std::vector<std::uint8_t> build_world_gif_packet(
    const std::vector<NativeWorldMesh>& meshes) {
    std::vector<std::uint8_t> out;
    out.reserve(16u + meshes.size() * 64u * 16u);
    struct Qword { std::uint64_t lo = 0, hi = 0; };
    std::vector<Qword> payload;
    struct ClipVertex {
        float x, y, z, w;
        float r, g, b, a;
        float u, v;
    };
    const auto mix_vertex = [](const ClipVertex& a, const ClipVertex& b, float t) {
        return ClipVertex{
            a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t,
            a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t,
            a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t};
    };
    const auto append_ad = [&payload](std::uint64_t value, std::uint8_t address) {
        payload.push_back({value, address});
    };
    std::uint32_t only_request = 0;
    if (const char* setting = std::getenv("OPENRAC_SLY_GIF_PRODUCER_REQUEST")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0') only_request = static_cast<std::uint32_t>(parsed);
    }
    for (const auto& mesh : meshes) {
        if (only_request != 0u &&
            mesh.request != only_request)
            continue;
        const std::size_t count = mesh.vertices.size() / 9u;
        if (count < 3u || count > 4096u) continue;
        const bool textured = mesh.tex0 != 0u;
        // Isolated port-side draw state. Keep the game's actual material
        // registers where available, while disabling depth tests until the
        // native depth path is validated.
        append_ad(0u, 0x4Cu);  // FRAME_1: write all colour channels.
        append_ad(0u, 0x47u);  // TEST_1: depth test off.
        constexpr std::uint32_t kXyOffsetX = (2048u - 640u / 2u) << 4u;
        constexpr std::uint32_t kXyOffsetY = (2048u - 224u / 2u) << 4u;
        append_ad(kXyOffsetX | (static_cast<std::uint64_t>(kXyOffsetY) << 32u), 0x18u);
        if (textured) {
            append_ad(mesh.tex0, 0x06u);
            append_ad(mesh.tex1, 0x14u);
            append_ad(mesh.clamp, 0x08u);
        }
        // The preceding game batch can leave PRMODECONT selecting PRMODE.
        // Re-enable PRIM attributes so this packet's TME bit is authoritative.
        append_ad(1u, 0x1Au);
        // PRIM 3 (triangle list), optionally textured with STQ coordinates.
        append_ad(3u | (textured ? (1u << 4u) : 0u), 0x00u);

        std::vector<ClipVertex> vertices(count);
        for (std::size_t i = 0; i < count; ++i) {
            const float x = mesh.vertices[i * 9u + 0u];
            const float y = mesh.vertices[i * 9u + 1u];
            const float z = mesh.vertices[i * 9u + 2u];
            float clip[4]{};
            for (std::size_t row = 0; row < 4; ++row)
                clip[row] = mesh.mvp[row] * x + mesh.mvp[4u + row] * y +
                            mesh.mvp[8u + row] * z + mesh.mvp[12u + row];
            vertices[i] = {clip[0], clip[1], clip[2], clip[3],
                mesh.vertices[i * 9u + 3u], mesh.vertices[i * 9u + 4u],
                mesh.vertices[i * 9u + 5u], mesh.vertices[i * 9u + 6u],
                mesh.vertices[i * 9u + 7u], mesh.vertices[i * 9u + 8u]};
        }

        // Clip each triangle in homogeneous space before converting to the
        // GS's finite integer coordinate range. Otherwise behind-camera
        // vertices wrap through 16-bit XY and create screen-spanning spikes.
        const auto emit_triangle = [&](const ClipVertex& a, const ClipVertex& b,
                                       const ClipVertex& c) {
            std::array<ClipVertex, 12> polygon{};
            std::array<ClipVertex, 12> clipped{};
            std::size_t polygon_size = 3u;
            polygon[0] = a;
            polygon[1] = b;
            polygon[2] = c;
            const auto distance = [](const ClipVertex& v, unsigned plane) {
                switch (plane) {
                    case 0: return v.x + v.w;
                    case 1: return v.w - v.x;
                    case 2: return v.y + v.w;
                    case 3: return v.w - v.y;
                    case 4: return v.z + v.w;
                    default: return v.w - v.z;
                }
            };
            for (unsigned plane = 0; plane < 6u && polygon_size != 0u; ++plane) {
                std::size_t clipped_size = 0;
                ClipVertex previous = polygon[polygon_size - 1u];
                float previous_distance = distance(previous, plane);
                bool previous_inside = previous_distance >= 0.0f;
                for (std::size_t vertex = 0; vertex < polygon_size; ++vertex) {
                    const ClipVertex current = polygon[vertex];
                    const float current_distance = distance(current, plane);
                    const bool current_inside = current_distance >= 0.0f;
                    if (current_inside != previous_inside) {
                        const float denominator = previous_distance - current_distance;
                        if (std::isfinite(denominator) && std::abs(denominator) > 1.0e-12f &&
                            clipped_size < clipped.size()) {
                            clipped[clipped_size++] = mix_vertex(
                                previous, current, previous_distance / denominator);
                        }
                    }
                    if (current_inside && clipped_size < clipped.size())
                        clipped[clipped_size++] = current;
                    previous = current;
                    previous_distance = current_distance;
                    previous_inside = current_inside;
                }
                polygon_size = clipped_size;
                polygon.swap(clipped);
            }
            if (polygon_size < 3u) return;
            for (std::size_t fan = 1u; fan + 1u < polygon_size; ++fan) {
                const ClipVertex triangle[3]{polygon[0], polygon[fan], polygon[fan + 1u]};
                for (const ClipVertex& vertex : triangle) {
                    if (!std::isfinite(vertex.w) || vertex.w <= 1.0e-6f) return;
                    const float nx = vertex.x / vertex.w;
                    const float ny = vertex.y / vertex.w;
                    const float q = 1.0f / vertex.w;
                    if (!std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(q)) return;
                    const int sx = static_cast<int>(kXyOffsetX + std::lround(
                        std::clamp((nx + 1.0f) * 0.5f, 0.0f, 1.0f) * 640.0f * 16.0f));
                    const int sy = static_cast<int>(kXyOffsetY + std::lround(
                        std::clamp((1.0f - ny) * 0.5f, 0.0f, 1.0f) * 224.0f * 16.0f));
                    const auto channel = [](float value) {
                        if (!std::isfinite(value)) return std::uint64_t{0};
                        return static_cast<std::uint64_t>(std::lround(
                            std::clamp(value, 0.0f, 1.0f) * 255.0f));
                    };
                    const std::uint64_t rgbaq = channel(vertex.r) |
                        (channel(vertex.g) << 8u) | (channel(vertex.b) << 16u) |
                        (channel(vertex.a) << 24u) |
                        (static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(q)) << 32u);
                    append_ad(rgbaq, 0x01u);
                    if (textured) {
                        const float s = vertex.u * q;
                        const float t = vertex.v * q;
                        const std::uint64_t st = std::bit_cast<std::uint32_t>(s) |
                            (static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(t)) << 32u);
                        append_ad(st, 0x02u);
                    }
                    const std::uint64_t packed = static_cast<std::uint16_t>(sx) |
                        (static_cast<std::uint64_t>(static_cast<std::uint16_t>(sy)) << 16u);
                    append_ad(packed, 0x05u); // XYZ2
                }
            }
        };
        // BuildStripMeshPacket marks fog behaviour in VertexRefs.flags bit 7;
        // it does not mark strip starts. The captured sequence is one strip.
        for (std::size_t i = 2u; i < count; ++i) {
            const std::size_t ia = (i & 1u) ? i - 1u : i - 2u;
            const std::size_t ib = (i & 1u) ? i - 2u : i - 1u;
            emit_triangle(vertices[ia], vertices[ib], vertices[i]);
        }
    }
    if (payload.empty()) return out;
    auto append_qword = [&](std::uint64_t lo, std::uint64_t hi) {
        for (int b = 0; b < 8; ++b) out.push_back(static_cast<std::uint8_t>(lo >> (b * 8u)));
        for (int b = 0; b < 8; ++b) out.push_back(static_cast<std::uint8_t>(hi >> (b * 8u)));
    };
    // A GIFtag has a 15-bit NLOOP field. Split large captures into valid
    // packets and set EOP only on the last one.
    constexpr std::size_t kMaxLoops = 0x7FFFu;
    for (std::size_t begin = 0; begin < payload.size();) {
        const std::size_t loops = std::min(kMaxLoops, payload.size() - begin);
        const bool last = begin + loops == payload.size();
        const std::uint64_t tag_lo = static_cast<std::uint64_t>(loops) |
            (last ? (1ull << 15u) : 0u) | (1ull << 60u);
        append_qword(tag_lo, 0xEu);
        for (std::size_t i = 0; i < loops; ++i)
            append_qword(payload[begin + i].lo, payload[begin + i].hi);
        begin += loops;
    }
    return out;
}

extern "C" void openrac_native_window_present(void) {
    if (!g_window) return;
    int dump_width = 0, dump_height = 0;
    g_window->drawable_size(dump_width, dump_height);
    dump_vu_ndc_points(dump_width, dump_height);
    static std::uint64_t present_sequence = 0;
    const std::uint64_t this_present = ++present_sequence;
    int width = 0, height = 0;
    g_window->drawable_size(width, height);
    openrac::gl::glBindFramebuffer(openrac::gl::GL_FRAMEBUFFER, 0);
    g_viewport(0, 0, width, height);
    if (draw_movie_frame(width, height)) {
        g_window->swap();
        return;
    }
    if (std::getenv("OPENRAC_DEBUG_PRESERVE_FRAME") == nullptr) {
        openrac::gl::glColorMask(openrac::gl::GL_TRUE, openrac::gl::GL_TRUE,
                                 openrac::gl::GL_TRUE, openrac::gl::GL_TRUE);
        openrac::gl::glDepthMask(openrac::gl::GL_TRUE);
        openrac::gl::glDisable(openrac::gl::GL_SCISSOR_TEST);
        g_clear_color(0, 0, 0, 1);
        g_clear(0x00004100u);
    }
    std::shared_ptr<const std::vector<std::uint8_t>> ready_packets;
    std::shared_ptr<const std::vector<NativeWorldMesh>> ready_world_meshes;
    std::size_t world_mesh_replacements = 0;
    std::uint64_t ready_frame_serial = 0;
    {
        std::scoped_lock lock(g_render_frame_mutex);
        ready_packets = g_ready_packets;
        ready_world_meshes = g_ready_world_meshes;
        world_mesh_replacements = g_ready_world_mesh_replacements;
        ready_frame_serial = g_ready_frame_serial;
    }
    static const std::vector<std::uint8_t> kEmptyPackets;
    static const std::vector<NativeWorldMesh> kEmptyWorldMeshes;
    const auto& packets = ready_packets ? *ready_packets : kEmptyPackets;
    const auto& published_world_meshes = ready_world_meshes
        ? *ready_world_meshes : kEmptyWorldMeshes;
    if (std::getenv("OPENRAC_TRACE_NATIVE_FRAME_SNAPSHOT") != nullptr) {
        static unsigned notices = 0;
        if (notices++ < 96u)
            std::fprintf(stderr,
                         "[native frame present] host=%llu guest=%llu packets=%zu meshes=%zu\n",
                         static_cast<unsigned long long>(this_present),
                         static_cast<unsigned long long>(ready_frame_serial),
                         packets.size(), published_world_meshes.size());
    }
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static unsigned notices = 0;
        if ((!published_world_meshes.empty() || world_mesh_replacements != 0u) &&
            notices < 24u) {
            ++notices;
            std::fprintf(stderr,
                         "[native sly mesh queue] present=%llu meshes=%zu replaced=%zu\n",
                         static_cast<unsigned long long>(this_present),
                         published_world_meshes.size(), world_mesh_replacements);
        }
    }
#ifdef OPENRAC_VU_NDC_BY_DEFAULT
    bool vu_ndc_render = true;
#else
    bool vu_ndc_render = std::getenv("OPENRAC_SLY_VU_NDC_RENDER") != nullptr;
#endif
    std::vector<NativeWorldMesh> vu_ndc_world_meshes;
    if (vu_ndc_render) {
        vu_ndc_world_meshes = published_world_meshes;
        std::vector<float> ndc(65536u * 3u);
        std::vector<unsigned> meta(65536u * 4u);
        const std::size_t floats = openrac_native_vu_ndc_take(
            ndc.data(), ndc.size(), meta.data(), meta.size());
        const std::size_t count = floats / 3u;
        const bool screen01 = std::getenv("OPENRAC_SLY_VU_NDC_SCREEN01") != nullptr;
        int wanted_group = -1;
        if (const char* setting = std::getenv("OPENRAC_SLY_VU_NDC_GROUP")) {
            char* end = nullptr;
            const long parsed = std::strtol(setting, &end, 0);
            if (end != setting && parsed >= 0 && parsed < 1000) wanted_group = static_cast<int>(parsed);
        }
        int group_index = 0;
        std::size_t begin = 0;
        while (begin < count) {
            std::size_t end = begin + 1u;
            while (end < count &&
                   meta[end * 4u + 0u] == meta[(end - 1u) * 4u + 0u] &&
                   meta[end * 4u + 1u] == meta[(end - 1u) * 4u + 1u] &&
                   meta[end * 4u + 2u] == meta[(end - 1u) * 4u + 2u] &&
                   meta[end * 4u + 3u] > meta[(end - 1u) * 4u + 3u] &&
                   meta[end * 4u + 3u] - meta[(end - 1u) * 4u + 3u] <= 6u)
                ++end;
            if (end - begin >= 3u && (wanted_group < 0 || group_index == wanted_group)) {
                NativeWorldMesh ndc_mesh;
                ndc_mesh.instance = (0xdeadbeefull << 16u) | (begin & 0xFFFFu);
                ndc_mesh.source_mesh = meta[begin * 4u + 2u];
                for (std::size_t i = begin; i < end; ++i) {
                    ndc_mesh.vertices.push_back(screen01 ? ndc[i * 3u] * 2.0f - 1.0f : ndc[i * 3u]);
                    ndc_mesh.vertices.push_back(screen01 ? ndc[i * 3u + 1u] * 2.0f - 1.0f : ndc[i * 3u + 1u]);
                    ndc_mesh.vertices.push_back(ndc[i * 3u + 2u]);
                    for (int c = 0; c < 4; ++c) ndc_mesh.vertices.push_back(0.8f);
                    ndc_mesh.vertices.push_back(0.0f);
                    ndc_mesh.vertices.push_back(0.0f);
                }
                for (int i = 0; i < 16; ++i)
                    ndc_mesh.mvp[static_cast<std::size_t>(i)] = (i % 5 == 0) ? 1.0f : 0.0f;
                ndc_mesh.model_is_identity = true;
                vu_ndc_world_meshes.push_back(std::move(ndc_mesh));
            }
            ++group_index;
            begin = end;
        }
    }
    const auto& world_meshes = vu_ndc_render
        ? vu_ndc_world_meshes : published_world_meshes;
    // TextureRenderObject meshes are rendered to their own GS texture before
    // the packet stream draws the textured edge quad that consumes them.
    if (g_textures) {
        std::unordered_map<std::uint32_t, std::vector<NativeWorldMesh>> target_meshes;
        for (const auto& mesh : world_meshes)
            if (mesh.render_target && mesh.target_tbp != 0 &&
                mesh.target_width > 0 && mesh.target_height > 0 &&
                mesh.target_width <= 1024 && mesh.target_height <= 1024)
                target_meshes[mesh.target_tbp].push_back(mesh);
        for (const auto& [tbp, meshes] : target_meshes) {
            const int target_width = meshes.front().target_width;
            const int target_height = meshes.front().target_height;
            auto& target = g_native_texture_targets[tbp];
            if (!target.framebuffer || target.width != target_width ||
                target.height != target_height) {
                if (target.framebuffer) target.framebuffer->release();
                target.framebuffer = std::make_unique<openrac::renderer::FrameBuffer>();
                std::string error;
                if (!target.framebuffer->create(target_width, target_height, error)) {
                    std::fprintf(stderr,
                                 "[native sly target] framebuffer creation failed for TBP %#x (%dx%d): %s\n",
                                 tbp, target_width, target_height, error.c_str());
                    target.framebuffer.reset();
                    continue;
                }
                target.width = target_width;
                target.height = target_height;
            }
            if (target.texture == 0) {
                target.texture = g_textures->add(
                    openrac::renderer::Rgba8Image(target_width, target_height),
                    openrac::renderer::AlphaScale::Gs,
                    "native Sly render target", false, false);
            }
            openrac::gl::glBindFramebuffer(openrac::gl::GL_FRAMEBUFFER,
                                            target.framebuffer->id());
            g_viewport(0, 0, target_width, target_height);
            openrac::gl::glDisable(openrac::gl::GL_SCISSOR_TEST);
            openrac::gl::glColorMask(openrac::gl::GL_TRUE, openrac::gl::GL_TRUE,
                                     openrac::gl::GL_TRUE, openrac::gl::GL_TRUE);
            openrac::gl::glDepthMask(openrac::gl::GL_TRUE);
            g_clear_color(0, 0, 0, 0);
            g_clear(0x00004100u);
            render_world_meshes(meshes, true);
            if (g_direct) {
                g_direct->import_offscreen_layer(tbp, target.framebuffer->id(),
                                                 target_width, target_height);
            }
            auto target_pixels = target.framebuffer->read_rgba();
            const std::size_t target_pixel_bytes = target_pixels.size();
            if (std::getenv("OPENRAC_DUMP_NATIVE_TARGETS") != nullptr) {
                static unsigned target_dump = 0;
                if (target_dump < 8u) {
                    const char* directory = std::getenv("OPENRAC_DUMP_NATIVE_TARGET_DIR");
                    if (directory == nullptr || directory[0] == '\0') directory = "/tmp";
                    char path[512];
                    const int length = std::snprintf(
                        path, sizeof(path), "%s/sly1-native-target-%04x-%02u.ppm",
                        directory, tbp, target_dump);
                    if (length > 0 && static_cast<std::size_t>(length) < sizeof(path)) {
                        if (FILE* file = std::fopen(path, "wb")) {
                            std::fprintf(file, "P6 %d %d 255\n", target_width, target_height);
                            for (std::size_t i = 0; i + 3 < target_pixels.size(); i += 4)
                                std::fwrite(target_pixels.data() + i, 1, 3, file);
                            std::fclose(file);
                            std::fprintf(stderr, "[native target dump] tbp=%#x path=%s\n",
                                         tbp, path);
                            ++target_dump;
                        }
                    }
                }
            }
            openrac::renderer::Rgba8Image target_image(target_width, target_height);
            target_image.pixels = std::move(target_pixels);
            g_textures->replace_image(target.texture, std::move(target_image));
            g_textures->place(tbp, target.texture);
            if (std::getenv("OPENRAC_TRACE_SLY_RENDER_TARGETS") != nullptr) {
                static std::unordered_set<std::uint32_t> traced_targets;
                if (traced_targets.insert(tbp).second)
                    std::fprintf(stderr,
                                 "[native sly target pass] tbp=%#x size=%dx%d meshes=%zu framebuffer=%u pixels=%zu\n",
                                 tbp, target_width, target_height, meshes.size(),
                                 target.framebuffer->id(), target_pixel_bytes);
            }
        }
        openrac::gl::glBindFramebuffer(openrac::gl::GL_FRAMEBUFFER, 0);
        g_viewport(0, 0, width, height);
    }
    // Draw the textured world after the DirectRenderer's backdrop, not before it
    // (a pass here would be covered by the backdrop blit). Keep an opt-in early
    // pass for diagnosing that interaction.
    if (std::getenv("OPENRAC_SLY_GL_EARLY") != nullptr)
        render_world_meshes(world_meshes);
    const bool dump_world_frame = !world_meshes.empty() &&
        std::getenv("OPENRAC_DUMP_NATIVE_WORLD_FRAME") != nullptr;
    static unsigned world_frame_dump = 0;
    int paired_world_frame = -1;
    if (dump_world_frame) {
        const auto pixels = openrac::renderer::read_rgba(0, width, height);
        if (world_frame_dump < 8) {
            char path[512];
            paired_world_frame = static_cast<int>(world_frame_dump++);
            const char* dump_directory = std::getenv(
                "OPENRAC_DUMP_NATIVE_WORLD_FRAME_DIR");
            if (dump_directory == nullptr || dump_directory[0] == '\0')
                dump_directory = "/tmp";
            const int path_length = std::snprintf(
                path, sizeof(path), "%s/sly1-native-world-%02u.ppm",
                dump_directory, paired_world_frame);
            if (path_length >= 0 &&
                static_cast<std::size_t>(path_length) < sizeof(path)) {
                if (FILE* file = std::fopen(path, "wb")) {
                    std::fprintf(file, "P6 %d %d 255\n", width, height);
                    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
                        std::fwrite(pixels.data() + i, 1, 3, file);
                    std::fclose(file);
                    std::fprintf(stderr, "[native world frame dump] path=%s\n", path);
                }
            }
        }
    }
    std::vector<std::uint8_t> world_packet;
    if (std::getenv("OPENRAC_SLY_GIF_PRODUCER") != nullptr && !world_meshes.empty())
        world_packet = build_world_gif_packet(world_meshes);
    if (std::getenv("OPENRAC_TRACE_PRESENT") != nullptr) {
        std::fprintf(stderr, "[native present] frame=%llu packets=%zu world_draws=%zu\n",
                     static_cast<unsigned long long>(this_present), packets.size(),
                     world_meshes.size());
    }
    if (packets.size() >= 512u && packets.size() <= 16u * 1024u * 1024u &&
        std::getenv("OPENRAC_DUMP_NATIVE_PACKETS") != nullptr) {
        static unsigned dump_index = 0;
        static unsigned eligible_packet_frame = 0;
        unsigned skip = 0;
        if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_PACKET_SKIP")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed <= 100000u)
                skip = static_cast<unsigned>(parsed);
        }
        const unsigned this_packet_frame = eligible_packet_frame++;
        if (this_packet_frame >= skip && dump_index < 16) {
            char path[80];
            std::snprintf(path, sizeof(path), "/tmp/sly1-native-render-packets-%02u.bin",
                          dump_index++);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fwrite(packets.data(), 1, packets.size(), file);
                std::fclose(file);
                std::fprintf(stderr,
                             "[native packet dump] eligible_frame=%u bytes=%zu path=%s\n",
                             this_packet_frame, packets.size(), path);
            }
        }
    }
    if (g_textures) {
        openrac::renderer::RenderState state{*g_textures, {0, width, height}};
        if (!packets.empty() && g_direct) {
            openrac::renderer::FrameInput input;
            input.packets[static_cast<std::size_t>(openrac::renderer::Bucket::Hud)] = packets;
            g_direct->render(input, state);
        }
        if (std::getenv("OPENRAC_SLY_GL_AFTER_HUD_OFF") == nullptr &&
            !world_meshes.empty()) {
            // The backdrop wrote depth for its full-screen quads; clear depth so
            // the textured world meshes drawn on top are not all rejected.
            openrac::gl::glClear(openrac::gl::GL_DEPTH_BUFFER_BIT);
            render_world_meshes(world_meshes);
        }
        if (!world_packet.empty() && g_direct) {
            // The HUD pass leaves full-screen quads in the interpreter's vertex list;
            // without clearing, the world draw starts at vertex 0 and bridges them.
            g_direct->interpreter().clear();
            openrac::renderer::FrameInput world_input;
            world_input.packets[static_cast<std::size_t>(openrac::renderer::Bucket::Hud)] =
                world_packet;
            g_direct->render(world_input, state);
        }
        g_textures->end_frame();
    }
    const bool preserve_previous_frame = world_meshes.empty() &&
        (packets.empty() || !g_direct || !g_direct->last_frame_writes_display());
    const bool restored_previous_frame = preserve_previous_frame &&
        restore_present_frame(width, height);
    if (restored_previous_frame && std::getenv("OPENRAC_TRACE_PRESENT") != nullptr) {
        std::fprintf(stderr, "[native present] restored prior framebuffer after no-op draw\n");
    }
    if (std::getenv("OPENRAC_DUMP_NATIVE_FRAMES") != nullptr) {
        static unsigned frame_dump = 0;
        static unsigned frame_seen = 0;
        std::size_t minimum_bytes = 512;
        if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_MIN_BYTES")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed <= 16u * 1024u * 1024u)
                minimum_bytes = static_cast<std::size_t>(parsed);
        }
        unsigned skip = 0;
        if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_SKIP")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed <= 100000u)
                skip = static_cast<unsigned>(parsed);
        }
        unsigned stride = 1;
        if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_STRIDE")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0' && parsed > 0 && parsed <= 100000u)
                stride = static_cast<unsigned>(parsed);
        }
        // Native Sly world meshes are drawn on this present path without
        // passing through the DirectRenderer packet vector. Include them in
        // opt-in frame capture so the experimental world renderer can be
        // inspected even on frames with no HUD packets.
        const bool eligible = packets.size() >= minimum_bytes || !world_meshes.empty();
        const unsigned eligible_frame = eligible ? frame_seen++ : 0u;
        unsigned start_present = 0;
        if (const char* setting = std::getenv("OPENRAC_DUMP_NATIVE_FRAME_START")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 10);
            if (end != setting && *end == '\0') start_present = static_cast<unsigned>(parsed);
        }
        // Opt-in: once the game reports the gameplay UI state (3), latch and
        // keep capturing. The gameplay window can be shorter than the present
        // cadence, so a strict per-frame check misses it entirely.
        static bool gameplay_seen = false;
        bool gameplay_only_ok = true;
        if (std::getenv("OPENRAC_DUMP_GAMEPLAY_ONLY") != nullptr) {
            if (sly1_native_menu_page_and_state != nullptr) {
                const unsigned int packed = sly1_native_menu_page_and_state();
                if (((packed >> 8u) & 0xFFu) == 3u) gameplay_seen = true;
                gameplay_only_ok = gameplay_seen;
            }
        }
        const unsigned dump_cap = std::getenv("OPENRAC_DUMP_GAMEPLAY_ONLY") != nullptr
            ? 64u : 8u;
        if (eligible && gameplay_only_ok && this_present >= start_present &&
            eligible_frame >= skip && (eligible_frame - skip) % stride == 0u &&
            frame_dump < dump_cap) {
            const auto pixels = openrac::renderer::read_rgba(0, width, height);
            char path[512];
            const char* dump_directory = std::getenv(
                "OPENRAC_DUMP_NATIVE_FRAME_DIR");
            if (dump_directory == nullptr || dump_directory[0] == '\0')
                dump_directory = "/tmp";
            const int path_length = std::snprintf(
                path, sizeof(path), "%s/sly1-native-frame-%02u.ppm",
                dump_directory, frame_dump++);
            if (path_length >= 0 &&
                static_cast<std::size_t>(path_length) < sizeof(path)) {
                if (FILE* file = std::fopen(path, "wb")) {
                    std::fprintf(file, "P6 %d %d 255\n", width, height);
                    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
                        std::fwrite(pixels.data() + i, 1, 3, file);
                    }
                    std::fclose(file);
                    std::fprintf(stderr, "[native frame dump] path=%s\n", path);
                }
            }
        }
    }
    if (std::getenv("OPENRAC_TRACE_PRESENT_PIXEL") != nullptr) {
        constexpr std::array<std::array<int, 2>, 3> points{{{{299, 290}}, {{297, 381}}, {{76, 350}}}};
        std::array<std::array<std::uint8_t, 4>, points.size()> samples{};
        openrac::gl::glBindFramebuffer(openrac::gl::GL_READ_FRAMEBUFFER, 0);
        for (std::size_t i = 0; i < points.size(); ++i) {
            openrac::gl::glReadPixels(points[i][0], height - 1 - points[i][1], 1, 1,
                                      openrac::gl::GL_RGBA, openrac::gl::GL_UNSIGNED_BYTE,
                                      samples[i].data());
        }
        openrac::gl::glBindFramebuffer(openrac::gl::GL_READ_FRAMEBUFFER, 0);
        std::fprintf(stderr,
                     "[native present pixels] frame=%llu body0=%u,%u,%u body1=%u,%u,%u border=%u,%u,%u\n",
                     static_cast<unsigned long long>(this_present), samples[0][0], samples[0][1],
                     samples[0][2], samples[1][0], samples[1][1], samples[1][2], samples[2][0],
                     samples[2][1], samples[2][2]);
    }
    if (paired_world_frame >= 0) {
        const auto pixels = openrac::renderer::read_rgba(0, width, height);
        char path[112];
        std::snprintf(path, sizeof(path), "/tmp/sly1-native-world-after-hud-%02u.ppm",
                      static_cast<unsigned>(paired_world_frame));
        if (FILE* file = std::fopen(path, "wb")) {
            std::fprintf(file, "P6 %d %d 255\n", width, height);
            for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
                std::fwrite(pixels.data() + i, 1, 3, file);
            std::fclose(file);
            std::fprintf(stderr,
                         "[native paired world frame] index=%u world_draws=%zu hud_packet_bytes=%zu before=/tmp/sly1-native-world-%02u.ppm after=%s\n",
                         static_cast<unsigned>(paired_world_frame), world_meshes.size(),
                         packets.size(), static_cast<unsigned>(paired_world_frame), path);
        }
    }
    if (!restored_previous_frame) save_present_frame(width, height);
    g_window->swap();
}

// Publish both host-render inputs only after the guest has finished
// SubmitRenderFrame. The SDL presenter runs on another thread and may present
// several times per game frame; it must keep drawing the previous complete
// snapshot instead of draining a half-built frame.
extern "C" void openrac_native_window_frame_complete(void) {
    static std::uint64_t completed_frame_serial = 0;
    auto completed_packets = std::make_shared<std::vector<std::uint8_t>>();
    auto completed_meshes = std::make_shared<std::vector<NativeWorldMesh>>();
    std::size_t completed_replacements = 0;
    {
        std::scoped_lock lock(g_packets_mutex);
        completed_packets->swap(g_packets);
    }
    {
        std::scoped_lock lock(g_world_mesh_mutex);
        completed_meshes->swap(g_world_meshes);
        completed_replacements = g_world_mesh_replacements;
        g_world_mesh_replacements = 0;
    }
    {
        std::scoped_lock lock(g_render_frame_mutex);
        g_ready_packets = std::move(completed_packets);
        g_ready_world_meshes = std::move(completed_meshes);
        g_ready_world_mesh_replacements = completed_replacements;
        g_ready_frame_serial = ++completed_frame_serial;
        if (std::getenv("OPENRAC_TRACE_NATIVE_FRAME_SNAPSHOT") != nullptr &&
            completed_frame_serial <= 96u)
            std::fprintf(stderr,
                         "[native frame publish] guest=%llu packets=%zu meshes=%zu replacements=%zu\n",
                         static_cast<unsigned long long>(completed_frame_serial),
                         g_ready_packets->size(), g_ready_world_meshes->size(),
                         g_ready_world_mesh_replacements);
    }
}

extern "C" void openrac_native_window_destroy(void) {
    if (g_direct) g_direct->release();
    for (auto& [tbp, target] : g_native_texture_targets) {
        (void)tbp;
        if (target.framebuffer) target.framebuffer->release();
    }
    g_native_texture_targets.clear();
    if (g_world_vao != 0) openrac::gl::glDeleteVertexArrays(1, &g_world_vao);
    if (g_world_vbo != 0) openrac::gl::glDeleteBuffers(1, &g_world_vbo);
    if (g_movie_vao != 0) openrac::gl::glDeleteVertexArrays(1, &g_movie_vao);
    if (g_movie_texture != 0) openrac::gl::glDeleteTextures(1, &g_movie_texture);
    g_world_vao = 0;
    g_world_vbo = 0;
    g_world_shader.reset();
    g_movie_vao = 0;
    g_movie_texture = 0;
    g_movie_shader.reset();
    {
        std::scoped_lock lock(g_movie_frame_mutex);
        g_movie_frame.reset();
        g_movie_frame_active = false;
        g_movie_frame_width = 0;
        g_movie_frame_height = 0;
    }
    {
        std::scoped_lock lock(g_world_mesh_mutex);
        g_world_meshes.clear();
        g_world_mesh_world = 0;
    }
    {
        std::scoped_lock lock(g_render_frame_mutex);
        g_ready_packets.reset();
        g_ready_world_meshes.reset();
        g_ready_world_mesh_replacements = 0;
        g_ready_frame_serial = 0;
    }
    if (g_textures) g_textures->release_gl();
    if (g_present_texture != 0) openrac::gl::glDeleteTextures(1, &g_present_texture);
    if (g_present_framebuffer != 0) openrac::gl::glDeleteFramebuffers(1, &g_present_framebuffer);
    g_present_texture = 0;
    g_present_framebuffer = 0;
    g_present_frame_width = 0;
    g_present_frame_height = 0;
    g_present_frame_valid = false;
    g_direct.reset(); g_textures.reset(); g_window.reset();
    { std::scoped_lock lock(g_input_mutex); g_input.reset(); }
    g_platform.reset();
    g_clear_color = nullptr; g_clear = nullptr; g_viewport = nullptr;
    std::scoped_lock lock(g_packets_mutex); g_packets.clear();
}
