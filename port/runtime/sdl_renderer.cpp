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
    std::uint32_t source_mesh = 0;
    std::vector<float> vertices;
    std::array<float, 16> mvp{};
    std::uint64_t tex0 = 0;
    std::uint64_t tex1 = 0;
    std::uint64_t clamp = 0;
    std::uint64_t test = 0;
};

std::unique_ptr<openrac::platform::Platform> g_platform;
std::unique_ptr<sly1::platform::Input> g_input;
std::unique_ptr<openrac::platform::Window> g_window;
std::unique_ptr<openrac::renderer::TexturePool> g_textures;
std::unique_ptr<openrac::renderer::DirectRenderer> g_direct;
std::unique_ptr<openrac::renderer::Shader> g_world_shader;
std::mutex g_world_mesh_mutex;
std::vector<NativeWorldMesh> g_world_meshes;
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
openrac::gl::GLint g_world_debug_geometry_location = -1;
openrac::gl::GLint g_world_debug_color_location = -1;
ClearColor g_clear_color = nullptr;
Clear g_clear = nullptr;
Viewport g_viewport = nullptr;
std::mutex g_packets_mutex;
std::mutex g_input_mutex;
std::vector<std::uint8_t> g_packets;
std::atomic<std::uint64_t> g_dma_submit_count{0};
std::atomic<std::uint64_t> g_dma_accept_count{0};
std::atomic<std::uint64_t> g_dma_reject_count{0};
std::atomic<std::uint64_t> g_dma_bytes{0};
std::atomic<unsigned> g_dma_reject_notices{0};

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
uniform int u_debug_geometry;
uniform vec4 u_debug_color;
out vec4 frag_color;
void main() {
    if (u_debug_geometry != 0) {
        frag_color = u_debug_color;
        return;
    }
    vec4 color = v_color * 255.0;
    if (u_textured != 0) {
        vec4 texel = texture(u_texture, v_uv) * 255.0;
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
    frag_color = clamp(color, 0.0, 255.0) / 255.0;
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

void render_world_meshes(const std::vector<NativeWorldMesh>& meshes) {
    using namespace openrac::gl;
    if (!g_world_shader || meshes.empty()) return;
    const bool trace_projected_bounds =
        std::getenv("OPENRAC_TRACE_SLY_PRESENT_BOUNDS") != nullptr;
    const bool debug_solid_colors =
        std::getenv("OPENRAC_DEBUG_SLY_WORLD_SOLID_COLORS") != nullptr;
    const bool debug_skip_full_cover =
        std::getenv("OPENRAC_DEBUG_SLY_SKIP_FULL_COVER") != nullptr;
    static unsigned present_bounds_sample = 0;
    unsigned projected_meshes = 0;
    unsigned intersecting_meshes = 0;
    unsigned long long projected_vertices = 0;
    unsigned long long inside_vertices = 0;
    std::unordered_set<std::uint64_t> present_instances;
    std::unordered_set<std::uint64_t> present_materials;
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
        if (trace_projected_bounds) {
            if (!present_instances.insert(mesh.instance).second) ++duplicate_instances;
            present_materials.insert(mesh.tex0);
            present_signature ^= mesh.instance;
            present_signature *= 1099511628211ull;
            present_signature ^= mesh.tex0;
            present_signature *= 1099511628211ull;
            present_signature ^= static_cast<std::uint64_t>(mesh.vertices.size() / 9u);
            present_signature *= 1099511628211ull;
        }
        if (mesh.vertices.size() < 3u * 9u || mesh.vertices.size() > 4096u * 9u)
            continue;
        bool projected_cover = false;
        if (trace_projected_bounds || debug_skip_full_cover) {
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
            }
        }
        const auto tex0 = openrac::renderer::gs::Tex0::decode(mesh.tex0);
        const auto tex1 = openrac::renderer::gs::Tex1::decode(mesh.tex1);
        const auto clamp = openrac::renderer::gs::Clamp::decode(mesh.clamp);
        const auto test = openrac::renderer::gs::Test::decode(mesh.test);
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
        if (g_textures != nullptr && mesh.tex0 != 0) {
            texture = g_textures->resolve(tex0, openrac::renderer::gs::Texa{0, false, 0x80});
            if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
                static std::unordered_map<std::uint64_t, bool> texture_notices;
                if (texture_notices.emplace(mesh.tex0, true).second) {
                    const auto& image = g_textures->image(texture);
                    std::fprintf(stderr,
                                 "[native sly material] tbp=%#x cbp=%#x psm=%#x size=%dx%d handle=%u name=%s placeholder=%u test=%#llx tex1=%#llx clamp=%#llx\n",
                                 tex0.tbp0, tex0.cbp, tex0.psm, image.width, image.height,
                                 texture, g_textures->name(texture).c_str(),
                                 texture == g_textures->placeholder() ? 1u : 0u,
                                 static_cast<unsigned long long>(mesh.test),
                                 static_cast<unsigned long long>(mesh.tex1),
                                 static_cast<unsigned long long>(mesh.clamp));
                }
            }
        }
        glUniformMatrix4fv(g_world_mvp_location, 1, GL_FALSE, mesh.mvp.data());
        glUniform1i(g_world_texture_location, 0);
        glUniform1i(g_world_textured_location,
                    !debug_solid_colors && texture != 0 && mesh.tex0 != 0 ? 1 : 0);
        glUniform1i(g_world_tcc_location, tex0.tcc ? 1 : 0);
        glUniform1i(g_world_tfx_location, static_cast<GLint>(tex0.tfx));
        glUniform1i(g_world_debug_geometry_location, debug_solid_colors ? 1 : 0);
        if (debug_solid_colors) {
            const std::uint32_t color_hash = static_cast<std::uint32_t>(
                mesh.instance ^ (mesh.instance >> 32u));
            const float red = 0.25f + static_cast<float>(color_hash & 0xFFu) * (0.75f / 255.0f);
            const float green = 0.25f + static_cast<float>((color_hash >> 8u) & 0xFFu) * (0.75f / 255.0f);
            const float blue = 0.25f + static_cast<float>((color_hash >> 16u) & 0xFFu) * (0.75f / 255.0f);
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
        if (test.zte) {
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
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                     mesh.vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0,
                     static_cast<GLsizei>(mesh.vertices.size() / 9u));
    }
    if (trace_projected_bounds && present_bounds_sample < 64u) {
        std::fprintf(stderr,
                     "[native sly present bounds] frame=%u meshes=%zu unique=%zu duplicate=%u materials=%zu signature=%016llx projected=%u intersects=%u vertices=%llu inside=%llu ndc=%g,%g,%g..%g,%g,%g\n",
                     present_bounds_sample++, meshes.size(),
                     present_instances.size(), duplicate_instances,
                     present_materials.size(),
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
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
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
        if (stream.size() > kMaxBytes - g_packets.size()) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), stream.begin(), stream.end());
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
                                                           std::size_t size) {
    if (source == nullptr || size == 0) return;
    std::scoped_lock lock(g_packets_mutex);
    g_packets.insert(g_packets.end(), static_cast<const std::uint8_t*>(source),
                     static_cast<const std::uint8_t*>(source) + size);
}

extern "C" void openrac_native_renderer_submit_gif_stream(std::uint32_t source_address) {
    constexpr std::size_t kMaxBytes = 16 * 1024 * 1024;
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
        if (gif_size > kMaxBytes - g_packets.size()) {
            g_dma_reject_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_packets.insert(g_packets.end(), gif, gif + gif_size);
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
    std::uint32_t vertex_count, std::uint32_t source_mesh,
    const float* model, const float* view_projection,
    std::uint64_t tex0, std::uint64_t tex1, std::uint64_t clamp, std::uint64_t test) {
    if (world == 0 || vertices == nullptr || vertex_count < 3 || vertex_count > 4096 ||
        model == nullptr || view_projection == nullptr)
        return;
    NativeWorldMesh draw;
    draw.instance = instance;
    draw.source_mesh = source_mesh;
    draw.vertices.assign(vertices, vertices + static_cast<std::size_t>(vertex_count) * 9u);
    draw.tex0 = tex0;
    draw.tex1 = tex1;
    draw.clamp = clamp;
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
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD_BOUNDS") != nullptr) {
        static unsigned samples = 0;
        const unsigned sample = samples++;
        if (sample < 64u) {
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
                         "[native sly projected bounds] instance=%#llx vertices=%u projected=%u inside=%u local=%g,%g,%g..%g,%g,%g ndc=%g,%g..%g,%g z=%g..%g\n",
                         static_cast<unsigned long long>(instance), vertex_count,
                         projected, inside, min_local[0], min_local[1], min_local[2],
                         max_local[0], max_local[1], max_local[2],
                         min_x, min_y, max_x, max_y, min_z, max_z);
            if (sample < 4u) {
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
    while (test_cross_next < test_cross_count && test_frame >= test_cross_frames[test_cross_next]) {
        g_input->test_press_cross_once();
        std::fprintf(stderr, "[test-input] injected Cross press %u at native frame %u\n",
                     test_cross_next + 1, test_frame);
        ++test_cross_next;
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

extern "C" void openrac_native_window_present(void) {
    if (!g_window) return;
    static std::uint64_t present_sequence = 0;
    const std::uint64_t this_present = ++present_sequence;
    int width = 0, height = 0;
    g_window->drawable_size(width, height);
    openrac::gl::glBindFramebuffer(openrac::gl::GL_FRAMEBUFFER, 0);
    g_viewport(0, 0, width, height);
    if (std::getenv("OPENRAC_DEBUG_PRESERVE_FRAME") == nullptr) {
        openrac::gl::glColorMask(openrac::gl::GL_TRUE, openrac::gl::GL_TRUE,
                                 openrac::gl::GL_TRUE, openrac::gl::GL_TRUE);
        openrac::gl::glDepthMask(openrac::gl::GL_TRUE);
        openrac::gl::glDisable(openrac::gl::GL_SCISSOR_TEST);
        g_clear_color(0, 0, 0, 1);
        g_clear(0x00004100u);
    }
    std::vector<NativeWorldMesh> world_meshes;
    std::size_t world_mesh_replacements = 0;
    {
        std::scoped_lock lock(g_world_mesh_mutex);
        world_meshes.swap(g_world_meshes);
        world_mesh_replacements = g_world_mesh_replacements;
        g_world_mesh_replacements = 0;
    }
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static unsigned notices = 0;
        if ((!world_meshes.empty() || world_mesh_replacements != 0u) &&
            notices < 24u) {
            ++notices;
            std::fprintf(stderr,
                         "[native sly mesh queue] present=%llu meshes=%zu replaced=%zu\n",
                         static_cast<unsigned long long>(this_present),
                         world_meshes.size(), world_mesh_replacements);
        }
    }
    render_world_meshes(world_meshes);
    const bool dump_world_frame = !world_meshes.empty() &&
        std::getenv("OPENRAC_DUMP_NATIVE_WORLD_FRAME") != nullptr;
    static unsigned world_frame_dump = 0;
    int paired_world_frame = -1;
    if (dump_world_frame) {
        const auto pixels = openrac::renderer::read_rgba(0, width, height);
        if (world_frame_dump < 8) {
            char path[96];
            paired_world_frame = static_cast<int>(world_frame_dump++);
            std::snprintf(path, sizeof(path), "/tmp/sly1-native-world-%02u.ppm",
                          paired_world_frame);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fprintf(file, "P6 %d %d 255\n", width, height);
                for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
                    std::fwrite(pixels.data() + i, 1, 3, file);
                std::fclose(file);
                std::fprintf(stderr, "[native world frame dump] path=%s\n", path);
            }
        }
    }
    std::vector<std::uint8_t> packets;
    { std::scoped_lock lock(g_packets_mutex); packets.swap(g_packets); }
    if (std::getenv("OPENRAC_TRACE_PRESENT") != nullptr) {
        std::fprintf(stderr, "[native present] frame=%llu packets=%zu world_draws=%zu\n",
                     static_cast<unsigned long long>(this_present), packets.size(),
                     world_meshes.size());
    }
    if (packets.size() >= 512u && packets.size() <= 16u * 1024u * 1024u &&
        std::getenv("OPENRAC_DUMP_NATIVE_PACKETS") != nullptr) {
        static unsigned dump_index = 0;
        if (dump_index < 16) {
            char path[80];
            std::snprintf(path, sizeof(path), "/tmp/sly1-native-render-packets-%02u.bin",
                          dump_index++);
            if (FILE* file = std::fopen(path, "wb")) {
                std::fwrite(packets.data(), 1, packets.size(), file);
                std::fclose(file);
                std::fprintf(stderr, "[native packet dump] bytes=%zu path=%s\n",
                             packets.size(), path);
            }
        }
    }
    if (!packets.empty() && g_direct && g_textures) {
        openrac::renderer::FrameInput input;
        input.packets[static_cast<std::size_t>(openrac::renderer::Bucket::Hud)] = packets;
        openrac::renderer::RenderState state{*g_textures, {0, width, height}};
        g_direct->render(input, state);
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
        const bool eligible = packets.size() >= minimum_bytes;
        const unsigned eligible_frame = eligible ? frame_seen++ : 0u;
        if (eligible && eligible_frame >= skip &&
            (eligible_frame - skip) % stride == 0u && frame_dump < 8) {
            const auto pixels = openrac::renderer::read_rgba(0, width, height);
            char path[96];
            std::snprintf(path, sizeof(path), "/tmp/sly1-native-frame-%02u.ppm", frame_dump++);
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

extern "C" void openrac_native_window_destroy(void) {
    if (g_direct) g_direct->release();
    if (g_world_vao != 0) openrac::gl::glDeleteVertexArrays(1, &g_world_vao);
    if (g_world_vbo != 0) openrac::gl::glDeleteBuffers(1, &g_world_vbo);
    g_world_vao = 0;
    g_world_vbo = 0;
    g_world_shader.reset();
    {
        std::scoped_lock lock(g_world_mesh_mutex);
        g_world_meshes.clear();
        g_world_mesh_world = 0;
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
