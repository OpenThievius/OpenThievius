// SPDX-License-Identifier: AGPL-3.0-or-later
// Freestanding Linux/i386 entry for the no-emulation host32 build.
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
extern "C" char* getenv(const char*);
extern u32 D_00262E10; // g_player
extern u32 D_00274500; // g_imageRecordCountA
extern u32 D_00274504; // g_imageRecordsA
extern u32 D_00274508; // g_imageRecordCountB
extern u32 D_0027450C; // g_imageRecordsB

#if defined(OPENRAC_SLY1_WITH_SDL)
#include "openrac/native_window.h"
#include "platform/clock.h"
#include "../native_texture_bridge.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <ucontext.h>
alignas(16) thread_local u8 g_fault_trace_alt_stack[64 * 1024];
struct NativeSlyFrameSource {
    int stride_qwc = 0;
    u32 base = 0;
};
// Port-only material state for one FrameSource pass; the VU remains the source
// of truth for how the retail program combines these passes.
struct NativeSlyMaterial {
    u64 tex0 = 0;
    u64 tex1 = 0;
    u64 clamp = 0;
    u64 alpha = 0;
    u64 test = 0;
    bool alpha_valid = false;
};

static bool native_guest_range_readable(u32 address, u32 size);

static void native_trace_sly_image_records(u32 tbp, u32 cbp, u32 psm,
                                            u32 width, u32 height) {
    static bool enabled = std::getenv("OPENRAC_TRACE_SLY_IMAGE_RECORDS") != nullptr;
    if (!enabled) return;
    const char* filter = std::getenv("OPENRAC_TRACE_TEXTURE_MISS_TBP");
    if (filter == nullptr) filter = std::getenv("OPENRAC_TRACE_SLY_IMAGE_TBP");
    if (filter != nullptr) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(filter, &end, 0);
        if (end == filter || *end != '\0' || parsed > 0x3FFFul ||
            tbp != static_cast<u32>(parsed))
            return;
    } else if (tbp == 0u) {
        return;
    }
    static u32 notices = 0;
    if (notices++ >= 48u) return;
    const u32 image_count = D_00274500;
    const u32 image_table = D_00274504;
    const u32 palette_count = D_00274508;
    const u32 palette_table = D_0027450C;
    std::fprintf(stderr,
                 "[native sly image records] tbp=%#x cbp=%#x psm=%#x size=%ux%u images=%u@%#010x palettes=%u@%#010x\n",
                 tbp, cbp, psm, width, height, image_count, image_table,
                 palette_count, palette_table);
    if (image_count > 8192u || palette_count > 8192u ||
        (image_count != 0u && !native_guest_range_readable(image_table, image_count * 0x20u)) ||
        (palette_count != 0u && !native_guest_range_readable(palette_table, palette_count * 0x18u)))
        return;
    const volatile u8* images = reinterpret_cast<const volatile u8*>(image_table);
    for (u32 i = 0; i < image_count; ++i) {
        const volatile u8* record = images + i * 0x20u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x18u);
        if ((word & 0x3FFFu) != tbp) continue;
        const u32 bytes = *reinterpret_cast<const volatile u32*>(record + 0x0Cu);
        const u32 data = *reinterpret_cast<const volatile u32*>(record + 0x10u);
        std::fprintf(stderr,
                     "[native sly image match] i=%u word=%#llx bytes=%u data=%#010x blocks=%d bump=%u\n",
                     i, static_cast<unsigned long long>(word), bytes, data,
                     *reinterpret_cast<const volatile u32*>(record + 0x14u),
                     *reinterpret_cast<const volatile u16*>(record + 0x0Au));
    }
    const volatile u8* palettes = reinterpret_cast<const volatile u8*>(palette_table);
    for (u32 i = 0; i < palette_count; ++i) {
        const volatile u8* record = palettes + i * 0x18u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x10u);
        if (((word >> 37u) & 0x3FFFu) != cbp) continue;
        const u32 data = *reinterpret_cast<const volatile u32*>(record + 0x08u);
        std::fprintf(stderr,
                     "[native sly palette match] i=%u bits=%#llx data=%#010x blocks=%d bump=%u\n",
                     i, static_cast<unsigned long long>(word), data,
                     *reinterpret_cast<const volatile u32*>(record + 0x0Cu),
                     *reinterpret_cast<const volatile u16*>(record + 0x06u));
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_texture_lookup(u32 tbp, u32 cbp, u32 psm,
                                 u32 width, u32 height) {
    native_trace_sly_image_records(tbp, cbp, psm, width, height);
}

extern "C" int sly1_native_get_texture_records(
    u32 tbp, u32 cbp, u32 psm, NativeSlyTextureRecords* out) {
    if (out == nullptr) return 0;
    *out = {};
    const u32 image_count = D_00274500;
    const u32 image_table = D_00274504;
    const u32 palette_count = D_00274508;
    const u32 palette_table = D_0027450C;
    if (image_count > 8192u || palette_count > 8192u ||
        (image_count != 0u && !native_guest_range_readable(image_table, image_count * 0x20u)) ||
        (palette_count != 0u && !native_guest_range_readable(palette_table, palette_count * 0x18u)))
        return 0;

    const volatile u8* images = reinterpret_cast<const volatile u8*>(image_table);
    for (u32 i = 0; i < image_count; ++i) {
        const volatile u8* record = images + i * 0x20u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x18u);
        const u32 image_psm = *reinterpret_cast<const volatile u8*>(record + 0x08u);
        if ((word & 0x3FFFu) != tbp || image_psm != psm) continue;
        const u32 width = *reinterpret_cast<const volatile u16*>(record);
        const u32 height = *reinterpret_cast<const volatile u16*>(record + 0x02u);
        const u32 bytes = *reinterpret_cast<const volatile u32*>(record + 0x0Cu);
        const u32 data = *reinterpret_cast<const volatile u32*>(record + 0x10u);
        if (width == 0u || height == 0u || width > 1024u || height > 1024u ||
            bytes == 0u || bytes > (16u << 20u) ||
            !native_guest_range_readable(data, bytes))
            return 0;
        out->image_width = width;
        out->image_height = height;
        out->image_tbw = *reinterpret_cast<const volatile u8*>(record + 0x09u);
        out->image_psm = image_psm;
        out->image_bytes = bytes;
        out->image_data = reinterpret_cast<const std::uint8_t*>(data);
        break;
    }
    if (out->image_data == nullptr) return 0;

    const volatile u8* palettes = reinterpret_cast<const volatile u8*>(palette_table);
    for (u32 i = 0; i < palette_count; ++i) {
        const volatile u8* record = palettes + i * 0x18u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x10u);
        if (((word >> 37u) & 0x3FFFu) != cbp) continue;
        const u32 entries = *reinterpret_cast<const volatile u16*>(record + 0x04u);
        const u32 blocks = *reinterpret_cast<const volatile u32*>(record + 0x0Cu);
        const u32 data = *reinterpret_cast<const volatile u32*>(record + 0x08u);
        const u32 bytes = blocks * 16u;
        if ((entries != 16u && entries != 256u) || bytes == 0u ||
            bytes > 4096u || !native_guest_range_readable(data, bytes))
            continue;
        out->palette_entries = entries;
        out->palette_bytes = bytes;
        out->palette_data = reinterpret_cast<const std::uint8_t*>(data);
        break;
    }
    return 1;
}

static void native_trace_sly_asset_tables(u32 stage, u32 frame) {
    if (std::getenv("OPENRAC_TRACE_SLY_IMAGE_RECORDS") == nullptr || stage != 8u ||
        (frame % 60u) != 0u)
        return;
    static u32 last_image_count = 0xFFFFFFFFu;
    static u32 last_image_table = 0xFFFFFFFFu;
    static u32 last_palette_count = 0xFFFFFFFFu;
    static u32 last_palette_table = 0xFFFFFFFFu;
    const u32 image_count = D_00274500;
    const u32 image_table = D_00274504;
    const u32 palette_count = D_00274508;
    const u32 palette_table = D_0027450C;
    if (image_count == last_image_count && image_table == last_image_table &&
        palette_count == last_palette_count && palette_table == last_palette_table)
        return;
    last_image_count = image_count;
    last_image_table = image_table;
    last_palette_count = palette_count;
    last_palette_table = palette_table;
    const char* tbp_setting = std::getenv("OPENRAC_TRACE_SLY_IMAGE_TBP");
    const char* cbp_setting = std::getenv("OPENRAC_TRACE_SLY_PALETTE_CBP");
    const char* psm_setting = std::getenv("OPENRAC_TRACE_SLY_IMAGE_PSM");
    const char* width_setting = std::getenv("OPENRAC_TRACE_SLY_IMAGE_WIDTH");
    const char* height_setting = std::getenv("OPENRAC_TRACE_SLY_IMAGE_HEIGHT");
    char* end = nullptr;
    unsigned long parsed_tbp = tbp_setting != nullptr
        ? std::strtoul(tbp_setting, &end, 0) : 0x26A0ul;
    const u32 wanted_tbp = end != tbp_setting && *end == '\0' && parsed_tbp <= 0x3FFFul
        ? static_cast<u32>(parsed_tbp) : 0x26A0u;
    end = nullptr;
    unsigned long parsed_cbp = cbp_setting != nullptr
        ? std::strtoul(cbp_setting, &end, 0) : 0x1EDEul;
    const u32 wanted_cbp = end != cbp_setting && *end == '\0' && parsed_cbp <= 0x3FFFul
        ? static_cast<u32>(parsed_cbp) : 0x1EDEu;
    end = nullptr;
    unsigned long parsed_psm = psm_setting != nullptr
        ? std::strtoul(psm_setting, &end, 0) : 0x14ul;
    const u32 wanted_psm = end != psm_setting && *end == '\0' && parsed_psm <= 0x3Ful
        ? static_cast<u32>(parsed_psm) : 0x14u;
    end = nullptr;
    unsigned long parsed_width = width_setting != nullptr
        ? std::strtoul(width_setting, &end, 0) : 128ul;
    const u32 wanted_width = end != width_setting && *end == '\0' && parsed_width <= 1024ul
        ? static_cast<u32>(parsed_width) : 128u;
    end = nullptr;
    unsigned long parsed_height = height_setting != nullptr
        ? std::strtoul(height_setting, &end, 0) : 128ul;
    const u32 wanted_height = end != height_setting && *end == '\0' && parsed_height <= 1024ul
        ? static_cast<u32>(parsed_height) : 128u;
    std::fprintf(stderr,
                 "[native sly asset tables] stage=%u frame=%u image_records=%u@%#010x palette_records=%u@%#010x target_tbp=%#x target_cbp=%#x target=%ux%u psm=%#x\n",
                 stage, frame, image_count, image_table, palette_count, palette_table,
                 wanted_tbp, wanted_cbp, wanted_width, wanted_height, wanted_psm);
    if (image_count > 8192u || palette_count > 8192u ||
        (image_count != 0u && !native_guest_range_readable(image_table, image_count * 0x20u)) ||
        (palette_count != 0u && !native_guest_range_readable(palette_table, palette_count * 0x18u)))
        return;
    const volatile u8* images = reinterpret_cast<const volatile u8*>(image_table);
    for (u32 i = 0; i < image_count; ++i) {
        const volatile u8* record = images + i * 0x20u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x18u);
        const u16 width = *reinterpret_cast<const volatile u16*>(record);
        const u16 height = *reinterpret_cast<const volatile u16*>(record + 0x02u);
        const u8 psm = *reinterpret_cast<const volatile u8*>(record + 0x08u);
        const u8 tbw = *reinterpret_cast<const volatile u8*>(record + 0x09u);
        if ((word & 0x3FFFu) != wanted_tbp &&
            !(width == wanted_width && height == wanted_height && psm == wanted_psm))
            continue;
        std::fprintf(stderr,
                     "[native sly asset image] i=%u word=%#llx dimensions=%ux%u psm=%#x tbw=%u flags=%#x bytes=%u data=%#010x blocks=%d bump=%u\n",
                     i, static_cast<unsigned long long>(word), width, height, psm, tbw,
                     *reinterpret_cast<const volatile u32*>(record + 0x04u),
                     *reinterpret_cast<const volatile u32*>(record + 0x0Cu),
                     *reinterpret_cast<const volatile u32*>(record + 0x10u),
                     *reinterpret_cast<const volatile u32*>(record + 0x14u),
                     *reinterpret_cast<const volatile u16*>(record + 0x0Au));
    }
    const volatile u8* palettes = reinterpret_cast<const volatile u8*>(palette_table);
    for (u32 i = 0; i < palette_count; ++i) {
        const volatile u8* record = palettes + i * 0x18u;
        const u64 word = *reinterpret_cast<const volatile u64*>(record + 0x10u);
        if (((word >> 37u) & 0x3FFFu) != wanted_cbp) continue;
        std::fprintf(stderr,
                     "[native sly asset palette] i=%u bits=%#llx data=%#010x blocks=%d bump=%u\n",
                     i, static_cast<unsigned long long>(word),
                     *reinterpret_cast<const volatile u32*>(record + 0x08u),
                     *reinterpret_cast<const volatile u32*>(record + 0x0Cu),
                     *reinterpret_cast<const volatile u16*>(record + 0x06u));
    }
}
struct NativeSlyMesh {
    u32 source_mesh = 0;
    u32 kind = 0xFFFFFFFFu;
    int source_frame = 0;
    u32 vertex_weight_count = 0;
    u32 morph_target_count = 0;
    u32 morph_ents_address = 0;
    u32 morph_vertices_address = 0;
    u32 morph_weights_address = 0;
    std::vector<NativeSlyFrameSource> sources;
    std::vector<NativeSlyMaterial> materials;
    std::vector<float> vertices;
    std::vector<float> vertex_weights;
    std::vector<float> morph_deltas;
    std::vector<u8> morph_keys;
    std::vector<u8> vertex_indices;
};
struct alignas(16) NativeMathVec4 { float v[4]; };
struct alignas(16) NativeMathMat3 { NativeMathVec4 rows[3]; };
struct alignas(16) NativeMathMat4 { float v[16]; };
struct alignas(16) NativeMathQuad16 { u32 v[4]; };

// Reuse the decomp's CPU matrix routines to construct the render-target
// projection when the PS2 packet builder has not populated RangeRecord+0x70.
// This is only game math; native rendering still goes through OpenGL.
extern "C" void sly_native_build_projection(NativeMathMat4*, float, float, float,
                                             float, float, float)
    asm("_Z13func_00189F58P4Mat4ffffff");
extern "C" void sly_native_build_view_projection(float*, float*, void*,
                                                  NativeMathQuad16*)
    asm("_Z13func_001442C8PfS_PvP6Quad16");

static bool native_guest_range_readable(u32 address, u32 size) {
    const u64 end = static_cast<u64>(address) + size;
    return (address >= 0x00010000u && end <= 0x02000000ull) ||
           (address >= 0x08000000u && end <= 0x10000000ull);
}

static bool native_build_range_record_matrix(u32 range_address, float* out) {
    if (out == nullptr || !native_guest_range_readable(range_address, 0x50u))
        return false;
    const volatile float* range = reinterpret_cast<const volatile float*>(range_address);
    const float near_range = range[0x04u / 4u];
    const float far_range = range[0x08u / 4u];
    const float inner_distance = range[0x40u / 4u];
    const float outer_distance = range[0x44u / 4u];
    if (!std::isfinite(near_range) || !std::isfinite(far_range) ||
        !std::isfinite(inner_distance) || !std::isfinite(outer_distance) ||
        near_range <= 0.0f || far_range <= 0.0f)
        return false;

    NativeMathVec4 position{};
    NativeMathVec4 axis{};
    NativeMathVec4 hint{};
    for (u32 i = 0; i < 4u; ++i) {
        position.v[i] = range[0x10u / 4u + i];
        axis.v[i] = range[0x20u / 4u + i];
        hint.v[i] = range[0x30u / 4u + i];
    }
    NativeMathMat3 basis{};
    NativeMathMat4 projection{};
    if (std::fabs(near_range - far_range) <= 0.0001f) {
        const float scale = 0.5f / near_range;
        if (!std::isfinite(scale)) return false;
        projection.v[0] = scale;
        projection.v[5] = scale;
        projection.v[10] = 1.0f;
        projection.v[15] = 1.0f;
        projection.v[12] = 0.5f;
        projection.v[13] = 0.5f;
    } else {
        const float denominator = far_range - near_range;
        if (std::fabs(denominator) < 1.0e-8f) return false;
        const float k = (far_range * inner_distance - near_range * outer_distance) /
                        denominator;
        const float lo = inner_distance - k;
        const float hi = outer_distance - k;
        if (!std::isfinite(k) || !std::isfinite(lo) || !std::isfinite(hi) ||
            std::fabs(lo) < 1.0e-8f)
            return false;
        for (u32 i = 0; i < 4u; ++i) position.v[i] += axis.v[i] * k;
        const float scale = 0.5f / (near_range / lo);
        sly_native_build_projection(&projection, scale, scale, 0.5f, 0.5f, lo, hi);
    }
    float axis_length = std::sqrt(axis.v[0] * axis.v[0] + axis.v[1] * axis.v[1] +
                                  axis.v[2] * axis.v[2]);
    if (!std::isfinite(axis_length)) return false;
    if (axis_length < 0.0001f) {
        basis.rows[0].v[0] = 1.0f;
    } else {
        for (u32 i = 0; i < 3u; ++i)
            basis.rows[0].v[i] = axis.v[i] / axis_length;
        basis.rows[0].v[3] = axis.v[3] / axis_length;
    }
    float cross[3] = {
        hint.v[1] * basis.rows[0].v[2] - hint.v[2] * basis.rows[0].v[1],
        hint.v[2] * basis.rows[0].v[0] - hint.v[0] * basis.rows[0].v[2],
        hint.v[0] * basis.rows[0].v[1] - hint.v[1] * basis.rows[0].v[0]};
    float cross_length = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] +
                                   cross[2] * cross[2]);
    if (!std::isfinite(cross_length)) return false;
    if (cross_length < 0.0001f) {
        float fallback[3] = {0.0f, 0.0f, 1.0f};
        if (std::fabs(basis.rows[0].v[2]) >= 0.9f) {
            fallback[1] = 1.0f;
            fallback[2] = 0.0f;
        }
        cross[0] = fallback[1] * basis.rows[0].v[2] - fallback[2] * basis.rows[0].v[1];
        cross[1] = fallback[2] * basis.rows[0].v[0] - fallback[0] * basis.rows[0].v[2];
        cross[2] = fallback[0] * basis.rows[0].v[1] - fallback[1] * basis.rows[0].v[0];
        cross_length = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] +
                                 cross[2] * cross[2]);
        if (cross_length < 0.0001f) return false;
    }
    for (u32 i = 0; i < 3u; ++i) basis.rows[1].v[i] = cross[i] / cross_length;
    basis.rows[1].v[3] = 0.0f;
    basis.rows[2].v[0] = basis.rows[0].v[1] * basis.rows[1].v[2] -
                         basis.rows[0].v[2] * basis.rows[1].v[1];
    basis.rows[2].v[1] = basis.rows[0].v[2] * basis.rows[1].v[0] -
                         basis.rows[0].v[0] * basis.rows[1].v[2];
    basis.rows[2].v[2] = basis.rows[0].v[0] * basis.rows[1].v[1] -
                         basis.rows[0].v[1] * basis.rows[1].v[0];
    basis.rows[2].v[3] = 0.0f;
    NativeMathQuad16 range_projection{};
    sly_native_build_view_projection(position.v, basis.rows[0].v, &projection,
                                     &range_projection);
    if (std::getenv("OPENRAC_TRACE_SLY_RENDER_TARGETS") != nullptr) {
        static u32 range_math_traces = 0;
        if (range_math_traces++ < 2u) {
            std::fprintf(stderr, "[native sly range basis] rows=");
            for (u32 i = 0; i < 12u; ++i)
                std::fprintf(stderr, "%s%g", i == 0u ? "" : ",",
                             basis.rows[0].v[i]);
            std::fprintf(stderr, " projection=");
            for (u32 i = 0; i < 16u; ++i)
                std::fprintf(stderr, "%s%g", i == 0u ? "" : ",", projection.v[i]);
            std::fprintf(stderr, " range_matrix=");
            for (u32 i = 0; i < 16u; ++i)
                std::fprintf(stderr, "%s%g", i == 0u ? "" : ",",
                             reinterpret_cast<const float*>(&range_projection)[i]);
            std::fputc('\n', stderr);
        }
    }
    const float* range_matrix = reinterpret_cast<const float*>(&range_projection);
    // DrawPlacedObjectParts multiplies this range projection by the retail
    // unit-square-to-clip matrix before passing it to DrawModelPart. That
    // matrix scales x/y into [-1,1] and removes depth for the target pass.
    for (u32 row = 0; row < 4u; ++row) {
        const u32 base = row * 4u;
        out[base + 0u] = range_matrix[base + 0u] * 2.0f - range_matrix[base + 3u];
        out[base + 1u] = range_matrix[base + 1u] * 2.0f - range_matrix[base + 3u];
        out[base + 2u] = 0.0f;
        out[base + 3u] = range_matrix[base + 3u];
    }
    // Range projections can face the opposite homogeneous-W convention from
    // OpenGL. Negating the complete matrix preserves x/w and y/w while moving
    // the same geometry into OpenGL's positive-W clip half-space.
    if (out[15] < 0.0f)
        for (u32 i = 0; i < 16u; ++i) out[i] = -out[i];
    for (u32 i = 0; i < 16u; ++i)
        if (!std::isfinite(out[i])) return false;
    return true;
}
static std::mutex g_native_sly_mesh_mutex;
static thread_local u32 g_native_sly_morph_vertex_count = 0;
static std::unordered_map<u32, NativeSlyMesh> g_native_sly_meshes_by_header;
static std::unordered_map<u32, NativeSlyMesh> g_native_sly_meshes_by_packet;
extern "C" unsigned long long openrac_native_renderer_dma_submit_count();
extern "C" unsigned long long openrac_native_renderer_dma_accept_count();
extern "C" unsigned long long openrac_native_renderer_dma_reject_count();
extern "C" unsigned long long openrac_native_renderer_dma_bytes();
extern "C" void openrac_native_movie_frame(const u8* rgba, int width, int height);
extern "C" void openrac_native_movie_end();
extern "C" int sly1_native_critical_owner();
extern "C" unsigned int sly1_native_critical_depth();
extern "C" unsigned int sly1_native_critical_spins();
#endif

struct NativeFunctionAddress { u32 retail_address; u32 native_address; };
namespace sly1 {
extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const u32 sly1_native_function_address_count;
}
extern "C" unsigned int sly1_native_host32_preceding_retail_mapping_for_pc(unsigned int pc) {
    u32 native_function = 0;
    u32 retail_function = 0;
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address != 0 && entry.native_address <= pc &&
            entry.native_address > native_function) {
            native_function = entry.native_address;
            retail_function = entry.retail_address;
        }
    }
    return retail_function;
}
extern "C" void func_001FAE18();
extern "C" int sly1_native_host32_crt_initialized = 0;
extern "C" volatile u32 sly1_native_postload_entry = 0;
extern "C" volatile u32 sly1_native_postload_bind = 0;
extern "C" volatile u32 sly1_native_bind_self = 0;
extern "C" volatile u32 sly1_native_bind_context = 0;
extern "C" volatile u32 sly1_native_bind_world = 0;
extern "C" volatile u32 sly1_native_bind_key = 0;
extern "C" volatile u32 sly1_native_find_world = 0;
extern "C" volatile u32 sly1_native_find_id = 0;
extern "C" volatile u32 sly1_native_find_context = 0;
extern "C" volatile u32 sly1_native_find_esp_before = 0;
extern "C" volatile u32 sly1_native_find_esp_after = 0;
extern "C" volatile u32 sly1_native_matrix_source = 0;
extern "C" volatile u32 sly1_native_matrix_output = 0;
extern "C" volatile u32 sly1_native_bind_ebp_entry = 0;
extern "C" volatile u32 sly1_native_bind_ebp_after_find = 0;
extern "C" volatile u32 sly1_native_motion_stage = 0;
extern "C" volatile u32 sly1_native_motion_self = 0;
extern "C" volatile u32 sly1_native_motion_hook = 0;
extern "C" volatile u32 sly1_native_motion_velocity = 0;
extern "C" volatile u32 sly1_native_motion_accel = 0;
extern "C" volatile u32 sly1_native_motion_tris = 0;
extern "C" volatile u32 sly1_native_motion_entry_hook = 0;
extern "C" volatile u32 sly1_native_motion_entry_velocity = 0;
extern "C" volatile u32 sly1_native_motion_entry_accel = 0;
extern "C" volatile u32 sly1_native_motion_entry_tris = 0;
extern "C" volatile u32 sly1_native_motion_entry_other = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_id = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_id = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_vertices = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_vertices = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_desc = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_desc = 0;
extern "C" volatile u32 sly1_native_motion_entry_all = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_faces = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_face_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_vert_ptr = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_copy_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_faces = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_face_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_return = 0;
extern "C" volatile u32 sly1_native_motion_copy_calls = 0;
extern "C" volatile u32 sly1_native_motion_copy_self = 0;
extern "C" volatile u32 sly1_native_motion_copy_vertices = 0;
extern "C" volatile u32 sly1_native_motion_helper_src = 0;
extern "C" volatile u32 sly1_native_motion_helper_extra = 0;
extern "C" volatile u32 sly1_native_motion_helper_src_id = 0;
extern "C" volatile u32 sly1_native_motion_helper_src_vertices = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_return = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_arg1 = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_arg2 = 0;
extern "C" volatile u32 sly1_native_arrival_world = 0;
extern "C" volatile u32 sly1_native_arrival_id = 0;
extern "C" volatile u32 sly1_native_arrival_parent = 0;
extern "C" volatile u32 sly1_native_arrival_result = 0;
extern "C" volatile u32 sly1_native_arrival_place_self = 0;
extern "C" volatile u32 sly1_native_arrival_rotation_self = 0;
extern "C" volatile u32 sly1_native_contact_object = 0;
extern "C" volatile u32 sly1_native_contact_descriptor = 0;
extern "C" volatile u32 sly1_native_contact_callback = 0;
extern "C" volatile u32 sly1_native_contact_end = 0;
extern "C" volatile u32 sly1_native_camera_target = 0;
extern "C" volatile u32 sly1_native_camera_target_descriptor = 0;
extern "C" volatile u32 sly1_native_camera_target_callback = 0;
extern "C" volatile u32 sly1_native_oscillator_debug[24] = {};
#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_host32_trace_dma_call(
    unsigned int channel, unsigned int source, unsigned int caller,
    unsigned int gif_channel) {
    const char* enabled = std::getenv("OPENRAC_TRACE_DMA_CALL");
    if (enabled == nullptr || enabled[0] == '0') return;
    static std::atomic<u32> notice_count{0};
    const u32 notice = notice_count.fetch_add(1, std::memory_order_relaxed);
    if (notice < 128u || (notice >= 4096u && notice % 16384u == 0u)) {
        std::fprintf(stderr,
                     "[native direct DMA] #%u channel=%#010x source=%#010x caller=%#010x gif=%#010x\n",
                     notice + 1u, channel, source, caller, gif_channel);
    }
}
extern "C" void sly1_native_host32_trace_dma_channel(
    unsigned int index, unsigned int address, unsigned int caller) {
    const char* enabled = std::getenv("OPENRAC_TRACE_DMA_CALL");
    if (enabled == nullptr || enabled[0] == '0') return;
    std::fprintf(stderr,
                 "[native direct DMA channel] index=%u address=%#010x caller=%#010x\n",
                 index, address, caller);
}
extern "C" void sly1_native_host32_trace_spr_copy(
    unsigned int direction, unsigned int chain, unsigned int tags,
    unsigned int transfer_index, unsigned int count, unsigned int source,
    unsigned int target, unsigned int size, unsigned int result) {
    const char* enabled = std::getenv("OPENRAC_TRACE_DMA_CALL");
    if (enabled == nullptr || enabled[0] == '0') return;
    static std::atomic<u32> notice_count{0};
    const u32 notice = notice_count.fetch_add(1, std::memory_order_relaxed);
    if (notice < 128u) {
        std::fprintf(stderr,
                     "[native SPR copy] #%u direction=%u chain=%#010x tags=%#010x transfer=%u count=%u first=%#010x->%#010x size=%u result=%u\n",
                     notice + 1u, direction, chain, tags, transfer_index,
                     count, source, target, size, result);
    }
}
extern "C" void sly1_native_host32_wait_frame(void) {
    // The retail main loop is paced by the PS2 display cycle. SDL owns the
    // native display cadence here, so keep the game thread at that cadence.
    SDL_Delay(16);
}
#endif
void func_00185758(int, int);
extern "C" int sly1_i386_syscall6(int, int, int, int, int, int, int);
#if !defined(OPENRAC_SLY1_WITH_SDL)
extern "C" __attribute__((weak)) void openrac_native_renderer_submit_dma_chain(u32) {}
#endif

namespace {

static bool native_trace_player_tree_owner(u32 candidate);

constexpr int kProtRead = 1;
constexpr int kProtWrite = 2;
constexpr int kMapPrivate = 2;
constexpr int kMapFixedNoReplace = 0x100000;
constexpr int kMapAnonymous = 0x20;
constexpr u32 kPage = 4096;

int syscall1(int number, int a) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a) : "memory", "cc");
    return result;
}

int syscall3(int number, int a, int b, int c) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a), "c"(b), "d"(c) : "memory", "cc");
    return result;
}

int syscall2(int number, int a, int b) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a), "c"(b) : "memory", "cc");
    return result;
}

void write_text(const char* text) {
    u32 size = 0;
    while (text[size] != 0) ++size;
    syscall3(4, 2, (int)text, (int)size);
}

void write_hex(const char* prefix, int value) {
    char line[64];
    u32 n = 0;
    while (prefix[n] != 0) { line[n] = prefix[n]; ++n; }
    const char hex[] = "0123456789abcdef";
    line[n++] = '0'; line[n++] = 'x';
    for (int i = 7; i >= 0; --i) line[n++] = hex[((u32)value >> (i * 4)) & 15];
    line[n++] = '\n';
    syscall3(4, 2, (int)line, (int)n);
}

void write_stack_value(u32 offset, u32 value) {
    char label[] = "native host32: fault stack +0000 ";
    const char hex[] = "0123456789abcdef";
    label[28] = hex[(offset >> 12) & 15];
    label[29] = hex[(offset >> 8) & 15];
    label[30] = hex[(offset >> 4) & 15];
    label[31] = hex[offset & 15];
    write_hex(label, (int)value);
}

bool syscall_failed(int result);

#if defined(OPENRAC_SLY1_WITH_SDL)
__attribute__((no_instrument_function)) void native_fault_trace(
    int signal_number, siginfo_t* info, void* context) {
    const auto* state = static_cast<const ucontext_t*>(context);
    write_hex("native host32: fatal signal ", signal_number);
    write_hex("native host32: fault address ", (int)info->si_addr);
    write_hex("native host32: fault eip ", state->uc_mcontext.gregs[REG_EIP]);
    write_hex("native host32: fault esp ", state->uc_mcontext.gregs[REG_ESP]);
    write_hex("native host32: fault ebp ", state->uc_mcontext.gregs[REG_EBP]);
    write_hex("native host32: fault eax ", state->uc_mcontext.gregs[REG_EAX]);
    write_hex("native host32: fault ebx ", state->uc_mcontext.gregs[REG_EBX]);
    write_hex("native host32: fault ecx ", state->uc_mcontext.gregs[REG_ECX]);
    write_hex("native host32: fault edx ", state->uc_mcontext.gregs[REG_EDX]);
    write_hex("native host32: fault esi ", state->uc_mcontext.gregs[REG_ESI]);
    write_hex("native host32: fault edi ", state->uc_mcontext.gregs[REG_EDI]);
    const u32 fault_eip = (u32)state->uc_mcontext.gregs[REG_EIP];
    const u32 fault_ebp = (u32)state->uc_mcontext.gregs[REG_EBP];
    u32 handler_sp;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(handler_sp));
    write_hex("native host32: fault signal stack flags ", state->uc_stack.ss_flags);
    write_hex("native host32: fault handler stack pointer ", (int)handler_sp);
    if (fault_eip >= 0x080d0000u && fault_eip < 0x080e0000u &&
        fault_ebp >= 0x10000000u && fault_ebp < 0xf0000000u) {
        const volatile u32* frame = (const volatile u32*)fault_ebp;
        write_hex("native host32: oscillator local self ", (int)frame[-0x124 / 4]);
        write_hex("native host32: oscillator local part ", (int)frame[-0x148 / 4]);
        write_hex("native host32: oscillator local obj ", (int)frame[-0xb8 / 4]);
        write_hex("native host32: oscillator local mesh ", (int)frame[-0xb4 / 4]);
        write_hex("native host32: oscillator local n ", (int)frame[-0x110 / 4]);
        write_hex("native host32: oscillator local weights ", (int)frame[-0x10c / 4]);
        write_hex("native host32: oscillator local verts ", (int)frame[-0x108 / 4]);
        write_hex("native host32: oscillator local data verts ", (int)frame[-0x134 / 4]);
        write_hex("native host32: oscillator local source ", (int)frame[-0x130 / 4]);
    }
    for (u32 i = 0; i < 24; ++i)
        write_hex("native host32: oscillator probe ",
                  (int)sly1_native_oscillator_debug[i]);
        write_hex("native host32: arrival world ", (int)sly1_native_arrival_world);
        write_hex("native host32: arrival id ", (int)sly1_native_arrival_id);
        write_hex("native host32: arrival parent ", (int)sly1_native_arrival_parent);
        write_hex("native host32: arrival result ", (int)sly1_native_arrival_result);
        write_hex("native host32: arrival place self ", (int)sly1_native_arrival_place_self);
        write_hex("native host32: arrival rotation self ", (int)sly1_native_arrival_rotation_self);
        write_hex("native host32: contact object ", (int)sly1_native_contact_object);
        write_hex("native host32: contact descriptor ", (int)sly1_native_contact_descriptor);
        write_hex("native host32: contact callback ", (int)sly1_native_contact_callback);
        write_hex("native host32: contact end ", (int)sly1_native_contact_end);
        write_hex("native host32: camera target ", (int)sly1_native_camera_target);
        write_hex("native host32: camera target descriptor ", (int)sly1_native_camera_target_descriptor);
        write_hex("native host32: camera target callback ", (int)sly1_native_camera_target_callback);
    write_hex("native host32: last PostLoad entry ", (int)sly1_native_postload_entry);
    write_hex("native host32: last PostLoad bind ", (int)sly1_native_postload_bind);
    write_hex("native host32: Bind self ", (int)sly1_native_bind_self);
    write_hex("native host32: Bind context ", (int)sly1_native_bind_context);
    write_hex("native host32: Bind world ", (int)sly1_native_bind_world);
    write_hex("native host32: Bind key ", (int)sly1_native_bind_key);
    write_hex("native host32: Find world ", (int)sly1_native_find_world);
    write_hex("native host32: Find id ", (int)sly1_native_find_id);
    write_hex("native host32: Find context ", (int)sly1_native_find_context);
    write_hex("native host32: Find ESP before ", (int)sly1_native_find_esp_before);
    write_hex("native host32: Find ESP after ", (int)sly1_native_find_esp_after);
    write_hex("native host32: matrix source ", (int)sly1_native_matrix_source);
    write_hex("native host32: matrix output ", (int)sly1_native_matrix_output);
    write_hex("native host32: Bind EBP entry ", (int)sly1_native_bind_ebp_entry);
    write_hex("native host32: Bind EBP after Find ", (int)sly1_native_bind_ebp_after_find);
    write_hex("native host32: motion init stage ", (int)sly1_native_motion_stage);
    write_hex("native host32: motion init self ", (int)sly1_native_motion_self);
    write_hex("native host32: motion init hook ", (int)sly1_native_motion_hook);
    write_hex("native host32: motion init velocity ", (int)sly1_native_motion_velocity);
    write_hex("native host32: motion init accel ", (int)sly1_native_motion_accel);
    write_hex("native host32: motion init tris ", (int)sly1_native_motion_tris);
    write_hex("native host32: motion entry hook ", (int)sly1_native_motion_entry_hook);
    write_hex("native host32: motion entry velocity ", (int)sly1_native_motion_entry_velocity);
    write_hex("native host32: motion entry accel ", (int)sly1_native_motion_entry_accel);
    write_hex("native host32: motion entry tris ", (int)sly1_native_motion_entry_tris);
    write_hex("native host32: motion entry other ", (int)sly1_native_motion_entry_other);
    write_hex("native host32: motion entry self id ", (int)sly1_native_motion_entry_self_id);
    write_hex("native host32: motion entry other id ", (int)sly1_native_motion_entry_other_id);
    write_hex("native host32: motion entry self vertices ", (int)sly1_native_motion_entry_self_vertices);
    write_hex("native host32: motion entry other vertices ", (int)sly1_native_motion_entry_other_vertices);
    write_hex("native host32: motion entry self desc ", (int)sly1_native_motion_entry_self_desc);
    write_hex("native host32: motion entry other desc ", (int)sly1_native_motion_entry_other_desc);
    write_hex("native host32: motion entry all ", (int)sly1_native_motion_entry_all);
    write_hex("native host32: motion entry self faces ", (int)sly1_native_motion_entry_self_faces);
    write_hex("native host32: motion entry self face count ", (int)sly1_native_motion_entry_self_face_count);
    write_hex("native host32: motion entry self verts ptr ", (int)sly1_native_motion_entry_self_vert_ptr);
    write_hex("native host32: motion entry self meshCopy count ", (int)sly1_native_motion_entry_self_copy_count);
    write_hex("native host32: motion entry other faces ", (int)sly1_native_motion_entry_other_faces);
    write_hex("native host32: motion entry other face count ", (int)sly1_native_motion_entry_other_face_count);
    write_hex("native host32: motion entry return ", (int)sly1_native_motion_entry_return);
    write_hex("native host32: motion copy calls ", (int)sly1_native_motion_copy_calls);
    write_hex("native host32: motion copy self ", (int)sly1_native_motion_copy_self);
    write_hex("native host32: motion copy vertices ", (int)sly1_native_motion_copy_vertices);
    write_hex("native host32: mesh helper source ", (int)sly1_native_motion_helper_src);
    write_hex("native host32: mesh helper extra ", (int)sly1_native_motion_helper_extra);
    write_hex("native host32: mesh helper source id ", (int)sly1_native_motion_helper_src_id);
    write_hex("native host32: mesh helper source vertices ", (int)sly1_native_motion_helper_src_vertices);
    write_hex("native host32: mesh copy entry return ", (int)sly1_native_motion_copy_entry_return);
    write_hex("native host32: mesh copy entry arg1 ", (int)sly1_native_motion_copy_entry_arg1);
    write_hex("native host32: mesh copy entry arg2 ", (int)sly1_native_motion_copy_entry_arg2);
    const u32 fault_esp = (u32)state->uc_mcontext.gregs[REG_ESP];
    if (fault_esp >= 0xC0000000u && fault_esp < 0xF0000000u) {
        const u32* stack = reinterpret_cast<const u32*>(fault_esp);
        write_hex("native host32: fault stack +00 ", stack[0]);
        write_hex("native host32: fault stack +04 ", stack[1]);
        write_hex("native host32: fault stack +08 ", stack[2]);
        write_hex("native host32: fault stack +0c ", stack[3]);
        write_hex("native host32: fault stack +10 ", stack[4]);
        write_hex("native host32: fault stack +14 ", stack[5]);
        write_hex("native host32: fault stack +1c ", stack[7]);
        write_hex("native host32: fault stack +20 ", stack[8]);
        write_hex("native host32: fault stack +24 ", stack[9]);
        write_hex("native host32: fault stack +28 ", stack[10]);
        write_hex("native host32: fault stack +2c ", stack[11]);
        write_hex("native host32: fault stack +30 ", stack[12]);
        write_hex("native host32: fault stack +34 ", stack[13]);
        write_hex("native host32: fault stack +3c ", stack[0x3c / 4]);
        write_hex("native host32: fault stack +40 ", stack[0x40 / 4]);
        write_hex("native host32: fault stack +44 ", stack[0x44 / 4]);
        write_hex("native host32: fault stack +48 ", stack[0x48 / 4]);
        write_hex("native host32: fault stack +4c ", stack[0x4c / 4]);
        write_hex("native host32: fault stack +50 ", stack[0x50 / 4]);
        write_hex("native host32: fault stack +54 ", stack[0x54 / 4]);
        write_hex("native host32: fault stack +7c ", stack[0x7c / 4]);
        write_hex("native host32: fault stack +80 ", stack[0x80 / 4]);
        write_hex("native host32: fault stack +84 ", stack[0x84 / 4]);
        write_hex("native host32: fault stack +88 ", stack[0x88 / 4]);
        for (u32 offset = 0x70; offset <= 0xc0; offset += 4) {
            write_stack_value(offset, stack[offset / 4]);
        }
        for (u32 offset = 0x1d0; offset <= 0x230; offset += 4) {
            write_stack_value(offset, stack[offset / 4]);
        }
        const u32 suspected_collider = stack[0x0c / 4];
        const u32 suspected_other = (u32)state->uc_mcontext.gregs[REG_EBX];
        if (suspected_collider >= 0x01000000u && suspected_collider < 0x08000000u) {
            const u32* collider = reinterpret_cast<const u32*>(suspected_collider);
            write_hex("native host32: suspected collider ", (int)suspected_collider);
            write_hex("native host32: collider verts count ", (int)collider[0x384 / 4]);
            write_hex("native host32: collider verts ", (int)collider[0x388 / 4]);
            write_hex("native host32: collider faces count ", (int)collider[0x38c / 4]);
            write_hex("native host32: collider faces ", (int)collider[0x390 / 4]);
        }
        if (suspected_other >= 0x01000000u && suspected_other < 0x08000000u) {
            const u32* other = reinterpret_cast<const u32*>(suspected_other);
            write_hex("native host32: suspected other ", (int)suspected_other);
            write_hex("native host32: other verts count ", (int)other[0x384 / 4]);
            write_hex("native host32: other verts ", (int)other[0x388 / 4]);
            write_hex("native host32: other faces count ", (int)other[0x38c / 4]);
            write_hex("native host32: other faces ", (int)other[0x390 / 4]);
        }
        const u32 suspected_edi = (u32)state->uc_mcontext.gregs[REG_EDI];
        if (suspected_edi >= 0x01000000u && suspected_edi < 0x08000000u) {
            const u32* object = reinterpret_cast<const u32*>(suspected_edi);
            const u32 hook = object[0x3c8 / 4];
            write_hex("native host32: edi object ", (int)suspected_edi);
            write_hex("native host32: object hook ", (int)hook);
            if (hook >= 0x01000000u && hook < 0x08000000u) {
                const u32* motion = reinterpret_cast<const u32*>(hook);
                write_hex("native host32: motion velocity ", (int)motion[0]);
                write_hex("native host32: motion accel ", (int)motion[1]);
                write_hex("native host32: motion tris ", (int)motion[2]);
            }
            write_hex("native host32: mesh copy verts count ", (int)object[0x3a8 / 4]);
            write_hex("native host32: mesh copy verts ", (int)object[0x3ac / 4]);
            write_hex("native host32: mesh copy faces count ", (int)object[0x3b0 / 4]);
            write_hex("native host32: mesh copy faces ", (int)object[0x3b4 / 4]);
            write_hex("native host32: mesh copy edges ", (int)object[0x3bc / 4]);
        }
        const u32 context_pointer = stack[0x4c / 4];
        if (context_pointer >= fault_esp && context_pointer < fault_esp + 0x4000u) {
            const u32* object = reinterpret_cast<const u32*>(context_pointer);
            write_hex("native host32: stack object +00 ", object[0x00 / 4]);
            write_hex("native host32: stack object +04 ", object[0x04 / 4]);
            write_hex("native host32: stack object +08 ", object[0x08 / 4]);
            write_hex("native host32: stack object +14 ", object[0x14 / 4]);
            write_hex("native host32: stack object +18 ", object[0x18 / 4]);
            write_hex("native host32: stack object +34 ", object[0x34 / 4]);
        }
        const u32 linked_object = stack[0x1e8 / 4];
        if (linked_object >= 0x01000000u && linked_object < 0x10000000u) {
            const u32* object = reinterpret_cast<const u32*>(linked_object);
            const u32 descriptor = object[0];
            write_hex("native host32: linked object ", (int)linked_object);
            write_hex("native host32: linked descriptor ", (int)descriptor);
            write_hex("native host32: linked object id ", (int)object[2]);
            write_hex("native host32: linked object world ", (int)object[0x14 / 4]);
            if (descriptor >= 0x08040000u && descriptor < 0x09000000u) {
                write_hex("native host32: linked m0f ",
                          (int)*(const u32*)(descriptor + 0x4c));
            }
        }
        for (u32 offset = 0x8c; offset <= 0x800; offset += 4) {
            const u32 value = stack[offset / 4];
            if (value >= 0x08040000u && value < 0x09000000u) {
                write_stack_value(offset, value);
            }
        }
    }
    syscall1(252, 128 + signal_number);
}

void install_fault_trace() {
    stack_t alternate{};
    alternate.ss_sp = g_fault_trace_alt_stack;
    alternate.ss_size = sizeof(g_fault_trace_alt_stack);
    const int altstack_result = sigaltstack(&alternate, nullptr);
    write_hex("native host32: install alternate stack result ", altstack_result);
    write_hex("native host32: install alternate stack base ",
              (int)alternate.ss_sp);
    struct sigaction action{};
    action.sa_sigaction = native_fault_trace;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
}

int make_game_constants_writable(dl_phdr_info* image, size_t, void*) {
    if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
    for (ElfW(Half) i = 0; i < image->dlpi_phnum; ++i) {
        const ElfW(Phdr)& segment = image->dlpi_phdr[i];
        if (segment.p_type != PT_LOAD || (segment.p_flags & PF_R) == 0 ||
            (segment.p_flags & (PF_W | PF_X)) != 0) continue;
        const u32 begin = (u32)(image->dlpi_addr + segment.p_vaddr) & ~(kPage - 1);
        const u32 limit = (u32)(image->dlpi_addr + segment.p_vaddr + segment.p_memsz);
        const u32 end = (limit + kPage - 1) & ~(kPage - 1);
        const int result = syscall3(125, (int)begin, (int)(end - begin),
                                    kProtRead | kProtWrite);
        if (syscall_failed(result)) {
            write_hex("native host32: writable constants mprotect error ", result);
        } else {
            write_hex("native host32: writable constants begin ", (int)begin);
            write_hex("native host32: writable constants end ", (int)end);
        }
    }
    return 1;
}

void make_game_constants_writable() {
    dl_iterate_phdr(make_game_constants_writable, nullptr);
}
#endif

u32 resolve_native(u32 address) {
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.retail_address == address || entry.native_address == address)
            return entry.native_address;
    }
    return 0;
}

u16 read16(const u8* p) { return (u16)p[0] | ((u16)p[1] << 8); }
u32 read32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
void write32(u8* p, u32 v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
u32 align_down(u32 value) { return value & ~(kPage - 1); }
u32 align_up(u32 value) { return (value + kPage - 1) & ~(kPage - 1); }
bool syscall_failed(int result) { return (u32)result >= 0xfffff001u; }

u64 divide_u64_u32(u64 numerator, u32 denominator) {
    u64 quotient = 0;
    u32 remainder = 0;
    if (denominator == 0) return 0;
    for (int bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | (u32)((numerator >> bit) & 1);
        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= (u64)1 << bit;
        }
    }
    return quotient;
}

int g_disc_fd = -1;
u32 g_disc_stride = 0;
u32 g_disc_payload = 0;
u32 g_disc_sector_count = 0;
int g_disc_read_error = 0;
volatile u32 g_disc_read_count = 0;
volatile u32 g_disc_sectors_read = 0;
extern "C" volatile u32 sly1_native_iop_boot_stage = 0;
extern "C" volatile u32 sly1_native_startup_callback_index = 0;
extern "C" volatile u32 D_002613F0;
bool g_startup_trace_enabled = false;
volatile sig_atomic_t g_native_pc_sample_pc = 0;
volatile sig_atomic_t g_native_pc_sample_sp = 0;
volatile sig_atomic_t g_native_pc_sample_bp = 0;
volatile sig_atomic_t g_native_pc_sample_sequence = 0;
volatile sig_atomic_t g_native_pc_sample_stack[8] = {};
volatile u32 g_native_semaphore_trace_active = 0;
volatile u32 g_semaphore_trace_event_count = 0;
volatile u32 g_native_trace_first_world = 0;
volatile u32 g_native_trace_target_world = 0;

extern "C" void sly1_native_trace_camera(u32 stage, u32 camera_address,
                                           u32 world_address) {
    static volatile u32 printed_mask = 0;
    if (!g_startup_trace_enabled ||
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_TRANSFORM") == nullptr ||
        camera_address == 0)
        return;
    if (stage < 32 && (__atomic_fetch_or(&printed_mask, 1u << stage,
                                         __ATOMIC_RELAXED) & (1u << stage)))
        return;
    const volatile float* position = reinterpret_cast<const volatile float*>(camera_address + 0x40u);
    const volatile float* rotation = reinterpret_cast<const volatile float*>(camera_address + 0x80u);
    const volatile float* view_projection = reinterpret_cast<const volatile float*>(camera_address + 0x100u);
    std::fprintf(stderr,
                 "[native camera stage] stage=%u world=%#010x camera=%#010x pos=%g,%g,%g rot=%g,%g,%g;%g,%g,%g;%g,%g,%g\n",
                 stage, world_address, camera_address,
                 position[0], position[1], position[2],
                 rotation[0], rotation[1], rotation[2],
                 rotation[4], rotation[5], rotation[6],
                 rotation[8], rotation[9], rotation[10]);
    std::fprintf(stderr,
                 "[native camera matrix] stage=%u rows=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g\n",
                 stage, view_projection[0], view_projection[1], view_projection[2], view_projection[3],
                 view_projection[4], view_projection[5], view_projection[6], view_projection[7],
                 view_projection[8], view_projection[9], view_projection[10], view_projection[11],
                 view_projection[12], view_projection[13], view_projection[14], view_projection[15]);
}

#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_trace_note_world(u32 world) {
    if (!g_startup_trace_enabled || world == 0) return;
    u32 first = __atomic_load_n(&g_native_trace_first_world, __ATOMIC_RELAXED);
    if (first == 0) {
        __atomic_compare_exchange_n(&g_native_trace_first_world, &first, world, false,
                                   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
        first = __atomic_load_n(&g_native_trace_first_world, __ATOMIC_RELAXED);
    }
    if (world != first)
        __atomic_store_n(&g_native_trace_target_world, world, __ATOMIC_RELAXED);
}

extern "C" void sly1_native_trace_placed_object_load(u32 object_address,
                                                        u32 stream_address,
                                                        u32 stage) {
    const char* enabled = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_LOAD");
    if (!g_startup_trace_enabled || enabled == nullptr || object_address == 0)
        return;
    u32 object_id = 96;
    if (const char* id = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_ID")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(id, &end, 0);
        if (end != id && *end == '\0' && parsed <= 0xFFFFu)
            object_id = static_cast<u32>(parsed);
    }
    if (*reinterpret_cast<const volatile u32*>(object_address + 8u) != object_id)
        return;
    if (const char* target = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_ADDRESS")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(target, &end, 0);
        if (end != target && *end == '\0' && parsed != 0 &&
            object_address != static_cast<u32>(parsed)) return;
    }
    static u32 printed = 0;
    if (__atomic_fetch_add(&printed, 1u, __ATOMIC_RELAXED) >= 128u) return;
    const u32 parent_address =
        *reinterpret_cast<const volatile u32*>(object_address + 0x18u);
    const volatile u32* rotation =
        reinterpret_cast<const volatile u32*>(object_address + 0xD0u);
    const volatile u32* world_rotation =
        reinterpret_cast<const volatile u32*>(object_address + 0x110u);
    const volatile float* position =
        reinterpret_cast<const volatile float*>(object_address + 0x100u);
    const volatile float* world_position =
        reinterpret_cast<const volatile float*>(object_address + 0x140u);
    if (stage == 7 || stage == 8) {
        const u32 position_attachment =
            *reinterpret_cast<const volatile u32*>(object_address + 0x1ECu);
        const u32 orientation_attachment =
            *reinterpret_cast<const volatile u32*>(object_address + 0x1F0u);
        const volatile u32* va = reinterpret_cast<const volatile u32*>(orientation_attachment);
        const volatile u32* spin = reinterpret_cast<const volatile u32*>(object_address + 0x160u);
        const volatile u32* accel = reinterpret_cast<const volatile u32*>(object_address + 0x180u);
        std::fprintf(stderr,
                     " attachment_state pos=%#010x rot=%#010x rot_vt=%#010x owner=%#010x mode=%d spin=%08x,%08x,%08x,%08x accel=%08x,%08x,%08x,%08x",
                     position_attachment, orientation_attachment,
                     orientation_attachment != 0 ? va[0] : 0,
                     orientation_attachment != 0 ? va[1] : 0,
                     orientation_attachment != 0 ? static_cast<int>(*reinterpret_cast<const volatile signed char*>(orientation_attachment + 0x11u)) : -1,
                     spin[0], spin[1], spin[2], spin[3], accel[0], accel[1], accel[2], accel[3]);
    }
    std::fprintf(stderr,
                 "[native placed rotation] stage=%u object=%#010x id=%u parent=%#010x stream=%#010x",
                 stage, object_address, object_id, parent_address, stream_address);
    if (stream_address != 0) {
        const volatile u32* stream = reinterpret_cast<const volatile u32*>(stream_address);
        const u32 source_address = stream[0x18 / 4];
        const int remaining = static_cast<int>(stream[0x1C / 4]);
        std::fprintf(stderr,
                     " flags=%#x mode=%d source=%#010x remaining=%d next=%#010x next_count=%d sector=%d bytes_left=%d",
                     stream[0], static_cast<int>(stream[1]), source_address, remaining,
                     stream[0x20 / 4], static_cast<int>(stream[0x24 / 4]),
                     static_cast<int>(stream[0x48 / 4]),
                     static_cast<int>(stream[0x40 / 4]));
    }
    if (stage == 0 && stream_address != 0) {
        const volatile u32* stream = reinterpret_cast<const volatile u32*>(stream_address);
        const u32 source_address = stream[0x18 / 4];
        const int remaining = static_cast<int>(stream[0x1C / 4]);
        if (source_address != 0 && remaining >= 36) {
        const volatile u32* source = reinterpret_cast<const volatile u32*>(source_address);
        std::fprintf(stderr, " input=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x",
                     source[0], source[1], source[2], source[3], source[4], source[5],
                     source[6], source[7], source[8]);
        }
    }
    if (stage == 1 || stage >= 2) {
        std::fprintf(stderr,
                     " rotation=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x world_rotation=%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x pos=%g,%g,%g world_pos=%g,%g,%g",
                     rotation[0], rotation[1], rotation[2], rotation[3],
                     rotation[4], rotation[5], rotation[6], rotation[7],
                     rotation[8], rotation[9], rotation[10], rotation[11],
                     world_rotation[0], world_rotation[1], world_rotation[2], world_rotation[3],
                     world_rotation[4], world_rotation[5], world_rotation[6], world_rotation[7],
                     world_rotation[8], world_rotation[9], world_rotation[10], world_rotation[11],
                     position[0], position[1], position[2],
                     world_position[0], world_position[1], world_position[2]);
    }
    std::fputc('\n', stderr);
}

extern "C" void sly1_native_trace_pose_rotation(u32 owner_address,
                                                   u32 attachment_address,
                                                   u32 stage,
                                                   u32 matrix_address,
                                                   u32 vector_address,
                                                   float value,
                                                   float reach,
                                                   float rate) {
    const char* enabled = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_LOAD");
    if (!g_startup_trace_enabled || enabled == nullptr || owner_address == 0 ||
        attachment_address == 0) return;
    u32 object_id = 96;
    if (const char* id = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_ID")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(id, &end, 0);
        if (end != id && *end == '\0' && parsed <= 0xFFFFu)
            object_id = static_cast<u32>(parsed);
    }
    if (*reinterpret_cast<const volatile u32*>(owner_address + 8u) != object_id)
        return;
    if (const char* target = std::getenv("OPENRAC_TRACE_PLACED_OBJECT_ADDRESS")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(target, &end, 0);
        if (end != target && *end == '\0' && parsed != 0 &&
            owner_address != static_cast<u32>(parsed)) return;
    }
    static volatile u32 printed = 0;
    if (__atomic_fetch_add(&printed, 1u, __ATOMIC_RELAXED) >= 128u) return;
    const volatile u32* matrix = reinterpret_cast<const volatile u32*>(matrix_address);
    const volatile u32* vector = reinterpret_cast<const volatile u32*>(vector_address);
    const volatile float* attachment =
        reinterpret_cast<const volatile float*>(attachment_address);
    const u32 attachment_state =
        *reinterpret_cast<const volatile u32*>(attachment_address + 0x1Cu);
    const u32 vtable_address =
        *reinterpret_cast<const volatile u32*>(attachment_address);
    std::fprintf(stderr,
                 "[native pose rotation] stage=%u owner=%#010x attachment=%#010x mode=%d key=%d track=%d attachment_end=%g state=%#010x getRot=%#010x matrix=",
                 stage, owner_address, attachment_address,
                 static_cast<int>(*reinterpret_cast<const volatile signed char*>(attachment_address + 0x11u)),
                 *reinterpret_cast<const volatile int*>(attachment_address + 0x14u),
                 *reinterpret_cast<const volatile int*>(attachment_address + 0x28u),
                 attachment[0x18 / 4],
                 attachment_state,
                 vtable_address != 0 ? *reinterpret_cast<const volatile u32*>(vtable_address + 0x14u) : 0);
    for (u32 i = 0; i < 12; ++i)
        std::fprintf(stderr, "%s%08x", i == 0 ? "" : ",", matrix[i]);
    std::fprintf(stderr, " vector=%08x,%08x,%08x,%08x value=%g reach=%g rate=%g\n",
                 vector[0], vector[1], vector[2], vector[3], value, reach, rate);
}

extern "C" void sly1_native_trace_model_part(u32 request_address, u32 world,
                                               u32 camera_address) {
    if (request_address == 0) return;
    const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
    const u32 owner_address = request_address != 0 ? request[0x70 / 4] : 0;
    const bool player_trace = std::getenv("OPENRAC_TRACE_PLAYER") != nullptr &&
                              native_trace_player_tree_owner(owner_address);
    if ((!g_startup_trace_enabled || world == 0 ||
         world != __atomic_load_n(&g_native_trace_target_world, __ATOMIC_RELAXED)) &&
        !player_trace) return;
    bool source_trace_enabled = false;
    u32 source_trace_target = 0;
    if (const char* setting = std::getenv("OPENRAC_TRACE_MODEL_MESH_SOURCE")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            source_trace_enabled = true;
            source_trace_target = static_cast<u32>(parsed);
        }
    }
    static u32 part_count = 0;
    const u32 trace_index = __atomic_fetch_add(&part_count, 1u, __ATOMIC_RELAXED);
    if ((!source_trace_enabled && trace_index >= (player_trace ? 128u : 32u)) ||
        (source_trace_enabled && trace_index >= 512u)) return;

    const u32 part_address = request[0x78 / 4];
    const u32 skin_address = request[0x74 / 4];
    if (part_address == 0) return;
    const volatile u32* part = reinterpret_cast<const volatile u32*>(part_address);
    u32 owner_class = 0xFFFFFFFFu;
    u32 owner_flags = 0;
    if (owner_address != 0) {
        const u32 class_address = *reinterpret_cast<const volatile u32*>(owner_address);
        if (class_address != 0) {
            const volatile u32* class_descriptor =
                reinterpret_cast<const volatile u32*>(class_address);
            owner_class = class_descriptor[1];
            owner_flags = class_descriptor[2];
        }
    }
    const u32 mesh_count = part[0x58 / 4];
    const u32 mesh_address = part[0x5C / 4];
    if (source_trace_enabled && mesh_address != 0 && mesh_count <= 4096u) {
        for (u32 i = 0; i < mesh_count; ++i) {
            const volatile u32* mesh = reinterpret_cast<const volatile u32*>(
                mesh_address + i * 0x50u);
            const u32 source = mesh[0x18 / 4];
            if (source != source_trace_target) continue;
            const volatile u32* skin = skin_address != 0
                ? reinterpret_cast<const volatile u32*>(skin_address) : nullptr;
            std::fprintf(stderr,
                         "[native model source match] source=%#010x draw=%u request=%#010x owner=%#010x class=%u part=%#010x skin=%#010x mesh=%u kind=%u bones=%u lit=%u skinned=%u\n",
                         source_trace_target, trace_index, request_address, owner_address,
                         owner_class, part_address, skin_address, i,
                         static_cast<unsigned>(*reinterpret_cast<const volatile u8*>(source)),
                         mesh[0x24 / 4], part[0x18 / 4], part[0x28 / 4]);
            if (skin != nullptr)
                std::fprintf(stderr,
                             "[native model source skin] source=%#010x flags=%#x count=%u words=%#010x bones=%#010x\n",
                             source_trace_target, skin[0x38 / 4], skin[0x40 / 4],
                             skin[0x44 / 4], skin[0x3C / 4]);
        }
    }
    const u32 strip_count = part[0x60 / 4];
    static u32 traced_lit_world_part = 0;
    bool finite_model = true;
    const volatile float* model = reinterpret_cast<const volatile float*>(request_address + 0x10u);
    for (u32 i = 0; i < 16u; ++i)
        if (!std::isfinite(model[i])) finite_model = false;
    if (owner_class == 2u && !finite_model &&
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_TRANSFORM") != nullptr) {
        static u32 traced_invalid_models = 0;
        if (__atomic_fetch_add(&traced_invalid_models, 1u, __ATOMIC_RELAXED) < 4u) {
            const volatile float* object_position =
                reinterpret_cast<const volatile float*>(owner_address + 0x140u);
            const volatile float* object_rotation =
                reinterpret_cast<const volatile float*>(owner_address + 0x110u);
            const volatile float* local_position =
                reinterpret_cast<const volatile float*>(owner_address + 0x100u);
            const volatile float* local_rotation =
                reinterpret_cast<const volatile float*>(owner_address + 0xD0u);
            const u32 parent_address = *reinterpret_cast<const volatile u32*>(owner_address + 0x18u);
            std::fprintf(stderr,
                         "[native invalid model source] index=%u owner=%#010x id=%u part=%#010x parent=%#010x\n",
                         trace_index, owner_address,
                         *reinterpret_cast<const volatile u32*>(owner_address + 8u),
                         part_address, parent_address);
            std::fprintf(stderr,
                         "[native invalid model source] local_pos=%g,%g,%g,%g world_pos=%g,%g,%g,%g\n",
                         local_position[0], local_position[1], local_position[2], local_position[3],
                         object_position[0], object_position[1], object_position[2], object_position[3]);
            for (u32 row = 0; row < 3u; ++row) {
                std::fprintf(stderr,
                             "[native invalid model source] local_rotation%u=%g,%g,%g,%g world_rotation%u=%g,%g,%g,%g\n",
                             row, local_rotation[row * 4u], local_rotation[row * 4u + 1u],
                             local_rotation[row * 4u + 2u], local_rotation[row * 4u + 3u],
                             row, object_rotation[row * 4u], object_rotation[row * 4u + 1u],
                             object_rotation[row * 4u + 2u], object_rotation[row * 4u + 3u]);
            }
            if (parent_address != 0) {
                const volatile u32* parent_words =
                    reinterpret_cast<const volatile u32*>(parent_address);
                const volatile u32* parent_descriptor =
                    reinterpret_cast<const volatile u32*>(parent_words[0]);
                const volatile float* parent_local_position =
                    reinterpret_cast<const volatile float*>(parent_address + 0x100u);
                const volatile float* parent_position =
                    reinterpret_cast<const volatile float*>(parent_address + 0x140u);
                const volatile float* parent_rotation =
                    reinterpret_cast<const volatile float*>(parent_address + 0x110u);
                std::fprintf(stderr,
                             "[native invalid model source] parent=%#010x parent_id=%u parent_class=%u parent_parent=%#010x local_pos=%g,%g,%g world_pos=%g,%g,%g\n",
                             parent_address, parent_words[2], parent_descriptor[1], parent_words[6],
                             parent_local_position[0], parent_local_position[1], parent_local_position[2],
                             parent_position[0], parent_position[1], parent_position[2]);
                for (u32 row = 0; row < 3u; ++row) {
                    std::fprintf(stderr,
                                 "[native invalid model source] parent_local_rotation%u=%g,%g,%g world_rotation%u=%g,%g,%g\n",
                                 row,
                                 *reinterpret_cast<const volatile float*>(parent_address + 0xD0u + (row * 16u)),
                                 *reinterpret_cast<const volatile float*>(parent_address + 0xD4u + (row * 16u)),
                                 *reinterpret_cast<const volatile float*>(parent_address + 0xD8u + (row * 16u)),
                                 row, parent_rotation[row * 4u], parent_rotation[row * 4u + 1u],
                                 parent_rotation[row * 4u + 2u]);
                }
                u32 ancestor = parent_address;
                for (u32 depth = 0; depth < 8u && ancestor != 0; ++depth) {
                    const volatile u32* words =
                        reinterpret_cast<const volatile u32*>(ancestor);
                    const u32 descriptor_address = words[0];
                    const volatile u32* descriptor = descriptor_address != 0
                        ? reinterpret_cast<const volatile u32*>(descriptor_address)
                        : nullptr;
                    const volatile float* local =
                        reinterpret_cast<const volatile float*>(ancestor + 0x100u);
                    const volatile float* world =
                        reinterpret_cast<const volatile float*>(ancestor + 0x140u);
                    const volatile float* local_rotation =
                        reinterpret_cast<const volatile float*>(ancestor + 0xD0u);
                    const volatile float* world_rotation =
                        reinterpret_cast<const volatile float*>(ancestor + 0x110u);
                    std::fprintf(stderr,
                                 "[native invalid model ancestor] depth=%u object=%#010x id=%u class=%u parent=%#010x local=%g,%g,%g world=%g,%g,%g local_rotation=%g,%g,%g;%g,%g,%g;%g,%g,%g world_rotation=%g,%g,%g;%g,%g,%g;%g,%g,%g\n",
                                 depth, ancestor, words[2],
                                 descriptor != nullptr ? descriptor[1] : 0xFFFFFFFFu,
                                 words[6], local[0], local[1], local[2], world[0], world[1],
                                 world[2], local_rotation[0], local_rotation[1], local_rotation[2],
                                 local_rotation[4], local_rotation[5], local_rotation[6],
                                 local_rotation[8], local_rotation[9], local_rotation[10],
                                 world_rotation[0], world_rotation[1], world_rotation[2],
                                 world_rotation[4], world_rotation[5], world_rotation[6],
                                 world_rotation[8], world_rotation[9], world_rotation[10]);
                    ancestor = words[6];
                }
            }
        }
    }
    if (owner_class == 2u && finite_model &&
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_TRANSFORM") != nullptr &&
        camera_address != 0 &&
        __atomic_load_n(&traced_lit_world_part, __ATOMIC_RELAXED) == 0) {
        __atomic_store_n(&traced_lit_world_part, part_address, __ATOMIC_RELAXED);
        const volatile float* camera_position = reinterpret_cast<const volatile float*>(camera_address + 0x40u);
        const volatile float* camera_rotation = reinterpret_cast<const volatile float*>(camera_address + 0x80u);
        const volatile float* camera_matrix = reinterpret_cast<const volatile float*>(camera_address + 0x100u);
        const volatile float* camera_scale = reinterpret_cast<const volatile float*>(camera_address + 0x1F0u);
        std::fprintf(stderr, "[native model transform] index=%u request=%#010x camera=%#010x\n",
                     trace_index, request_address, camera_address);
        for (u32 row = 0; row < 4u; ++row) {
            std::fprintf(stderr,
                         "[native model transform] model%u=%g,%g,%g,%g viewProjection%u=%g,%g,%g,%g\n",
                         row, model[row * 4u], model[row * 4u + 1u],
                         model[row * 4u + 2u], model[row * 4u + 3u],
                         row, camera_matrix[row * 4u], camera_matrix[row * 4u + 1u],
                         camera_matrix[row * 4u + 2u], camera_matrix[row * 4u + 3u]);
        }
        std::fprintf(stderr,
                     "[native model transform] position=%g,%g,%g rotation=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g scale=%g,%g\n",
                     camera_position[0], camera_position[1], camera_position[2],
                     camera_rotation[0], camera_rotation[1], camera_rotation[2], camera_rotation[3],
                     camera_rotation[4], camera_rotation[5], camera_rotation[6], camera_rotation[7],
                     camera_rotation[8], camera_rotation[9], camera_rotation[10], camera_rotation[11],
                     camera_scale[0], camera_scale[1]);
    }
    std::fprintf(stderr,
                 "[native model draw] index=%u world=%#010x request=%#010x owner=%#010x class=%u class_flags=%#x part=%#010x skin=%#010x meshes=%u strips=%u flags=%#x lit=%u skinned=%u matrix=%#x,%#x,%#x,%#x\n",
                 trace_index, world, request_address, owner_address,
                 owner_class, owner_flags, part_address, skin_address,
                 mesh_count, strip_count, part[0x2C / 4], part[0x18 / 4],
                 part[0x28 / 4], request[0x10 / 4], request[0x20 / 4],
                 request[0x30 / 4], request[0x40 / 4]);
    if (skin_address != 0) {
        const volatile u32* skin = reinterpret_cast<const volatile u32*>(skin_address);
        std::fprintf(stderr,
                     "[native model skin] index=%u flags=%#x count=%u words=%#010x bones=%#010x rgba=%08x\n",
                     trace_index, skin[0x38 / 4], skin[0x40 / 4],
                     skin[0x44 / 4], skin[0x04 / 4], skin[0x3C / 4]);
    }
    const u32 bounded_mesh_count = mesh_count < 4u ? mesh_count : 4u;
    for (u32 i = 0; i < bounded_mesh_count && mesh_address != 0; ++i) {
        const volatile u32* mesh = reinterpret_cast<const volatile u32*>(
            mesh_address + i * 0x50u);
        const u32 kind_address = mesh[0x18 / 4];
        const u32 kind = kind_address != 0
                             ? *reinterpret_cast<const volatile u8*>(kind_address)
                             : 0xFFFFFFFFu;
        std::fprintf(stderr,
                     "[native model mesh] index=%u mesh=%u kind=%u source=%#010x bones=%u qwc=%u,%u,%u data=%#010x,%#010x,%#010x\n",
                     trace_index, i, kind, kind_address, mesh[0x24 / 4], mesh[0x38 / 4],
                     mesh[0x3C / 4], mesh[0x40 / 4], mesh[0x44 / 4],
                     mesh[0x48 / 4], mesh[0x4C / 4]);
        if (kind_address != 0 && std::getenv("OPENRAC_TRACE_NATIVE_MODEL_STRIPS") != nullptr) {
            const volatile u32* strip_mesh = reinterpret_cast<const volatile u32*>(kind_address);
            const u32 strip_count = strip_mesh[0x18 / 4];
            const u32 strip_address = strip_mesh[0x1C / 4];
            const u32 frame_count = strip_mesh[0x20 / 4];
            std::fprintf(stderr,
                         "[native model strip source] index=%u mesh=%u mode=%u color=%08x strips=%u sources=%#010x frames=%u\n",
                         trace_index, i,
                         *reinterpret_cast<const volatile u8*>(kind_address + 1u),
                         strip_mesh[1], strip_count, strip_address, frame_count);
            const u32 bounded_strip_count = strip_count < 8u ? strip_count : 8u;
            for (u32 strip_index = 0; strip_index < bounded_strip_count && strip_address != 0;
                 ++strip_index) {
                const volatile u32* source = reinterpret_cast<const volatile u32*>(
                    strip_address + strip_index * 8u);
                std::fprintf(stderr,
                             "[native model frame source] index=%u mesh=%u strip=%u stride=%u base=%#010x\n",
                             trace_index, i, strip_index, source[0], source[1]);
            }
        }
        if (owner_class == 2u && finite_model && kind == 0u &&
            std::getenv("OPENRAC_DUMP_NATIVE_WORLD_PART") != nullptr) {
            static u32 dumped_world_part = 0;
            if (__atomic_exchange_n(&dumped_world_part, 1u, __ATOMIC_RELAXED) == 0) {
                const u32 packet_address[3] = {mesh[0x44 / 4], mesh[0x48 / 4], mesh[0x4C / 4]};
                const u32 packet_qwc[3] = {mesh[0x38 / 4], mesh[0x3C / 4], mesh[0x40 / 4]};
                for (u32 packet = 0; packet < 3u; ++packet) {
                    if (packet_address[packet] == 0 || packet_qwc[packet] == 0 ||
                        packet_qwc[packet] > 4096u) continue;
                    char path[96];
                    std::snprintf(path, sizeof(path), "/tmp/sly-world-part-%u.bin", packet);
                    if (FILE* file = std::fopen(path, "wb")) {
                        const std::size_t bytes = static_cast<std::size_t>(packet_qwc[packet]) * 16u;
                        const std::size_t written = std::fwrite(
                            reinterpret_cast<const void*>(packet_address[packet]), 1, bytes, file);
                        std::fclose(file);
                        std::fprintf(stderr,
                                     "[native world part dump] part=%#010x mesh=%u packet=%u address=%#010x qwc=%u written=%zu path=%s\n",
                                     part_address, i, packet, packet_address[packet],
                                     packet_qwc[packet], written, path);
                    }
                }
            }
        }
        if (kind == 6u &&
            std::getenv("OPENRAC_DUMP_NATIVE_MODEL_PACKET") != nullptr) {
            static u32 dumped = 0;
            if (__atomic_exchange_n(&dumped, 1u, __ATOMIC_RELAXED) == 0) {
                const u32 packet_address = mesh[0x44 / 4];
                const u32 packet_qwc = mesh[0x38 / 4];
                if (packet_address != 0 && packet_qwc > 0 && packet_qwc <= 4096u) {
                    const char* path = "/tmp/sly-kind6-model-packet.bin";
                    if (FILE* file = std::fopen(path, "wb")) {
                        const std::size_t bytes = static_cast<std::size_t>(packet_qwc) * 16u;
                        const std::size_t written = std::fwrite(
                            reinterpret_cast<const void*>(packet_address), 1, bytes, file);
                        std::fclose(file);
                        std::fprintf(stderr,
                                     "[native model packet dump] kind=%u qwc=%u bytes=%zu written=%zu path=%s\n",
                                     kind, packet_qwc, bytes, written, path);
                    }
                }
            }
        }
    }
}

extern "C" void sly1_native_trace_placed_object_parts(u32 owner_address,
                                                          u32 camera_address,
                                                          u32 part_count,
                                                          u32 parts_address,
                                                          u32 states_address) {
    const bool trace_player = std::getenv("OPENRAC_TRACE_PLAYER") != nullptr &&
                              native_trace_player_tree_owner(owner_address);
    const bool trace_all_models = std::getenv("OPENRAC_TRACE_MODEL_PARTS") != nullptr;
    if (!trace_player && !trace_all_models) return;
    static volatile u32 samples = 0;
    const u32 sample = __atomic_fetch_add(&samples, 1u, __ATOMIC_RELAXED);
    if (sample >= 256u) return;
    const u32 descriptor_address =
        *reinterpret_cast<const volatile u32*>(owner_address);
    if (descriptor_address < 0x00010000u || descriptor_address >= 0x02000000u)
        return;
    const volatile u32* descriptor =
        reinterpret_cast<const volatile u32*>(descriptor_address);
    std::fprintf(stderr,
                 "[native placed model parts] sample=%u world=%#010x owner=%#010x class=%u id=%u camera=%#010x parts=%u array=%#010x states=%#010x\n",
                 sample,
                 *reinterpret_cast<const volatile u32*>(owner_address + 0x14u),
                 owner_address, descriptor[1],
                 *reinterpret_cast<const volatile u32*>(owner_address + 8u),
                 camera_address, part_count, parts_address, states_address);
    if (parts_address == 0 || part_count == 0 || part_count > 8u) return;
    const volatile u32* parts = reinterpret_cast<const volatile u32*>(parts_address);
    for (u32 i = 0; i < part_count; ++i) {
        const volatile u32* part = parts + i * (0x70u / 4u);
        std::fprintf(stderr,
                     "[native placed model part] owner=%#010x index=%u pos=%g,%g,%g radius=%g kind=%d hook=%#010x matrix=%#010x\n",
                     owner_address, i,
                     static_cast<double>(*reinterpret_cast<const volatile float*>(part + 0)),
                     static_cast<double>(*reinterpret_cast<const volatile float*>(part + 1)),
                     static_cast<double>(*reinterpret_cast<const volatile float*>(part + 2)),
                     static_cast<double>(*reinterpret_cast<const volatile float*>(part + 4)),
                     static_cast<int>(part[5]), part[14], part[26]);
    }
}

extern "C" void sly1_native_trace_placed_object_draw(u32 owner_address,
                                                         u32 camera_address,
                                                         u32 stage) {
    if (std::getenv("OPENRAC_TRACE_MODEL_PARTS") == nullptr) return;
    static std::atomic<u32> samples{0};
    if (samples.fetch_add(1, std::memory_order_relaxed) >= 32u) return;
    const volatile u32* owner = reinterpret_cast<const volatile u32*>(owner_address);
    const volatile u32* descriptor = reinterpret_cast<const volatile u32*>(owner[0]);
    const volatile u32* camera = reinterpret_cast<const volatile u32*>(camera_address);
    const volatile float* position = reinterpret_cast<const volatile float*>(owner_address + 0x140u);
    std::fprintf(stderr,
                 "[native placed object draw] stage=%u owner=%#010x class=%u id=%u flags=%#018llx mask=%#010x area=%#010x pos=%g,%g,%g radius=%g far=%g parts=%u part_array=%#010x children=%#010x methods=%#010x,%#010x,%#010x\n",
                 stage, owner_address, descriptor[1], owner[2],
                 static_cast<unsigned long long>(
                     *reinterpret_cast<const volatile u64*>(owner_address + 0x2C8u)),
                 *reinterpret_cast<const volatile u32*>(owner_address + 0x88u), camera[0x87],
                 static_cast<double>(position[0]), static_cast<double>(position[1]),
                 static_cast<double>(position[2]),
                 static_cast<double>(*reinterpret_cast<const volatile float*>(owner_address + 0x2A8u)),
                 static_cast<double>(*reinterpret_cast<const volatile float*>(owner_address + 0x80u)),
                 *reinterpret_cast<const volatile u32*>(owner_address + 0x238u),
                 *reinterpret_cast<const volatile u32*>(owner_address + 0x23Cu), owner[0x34u / 4u],
                 descriptor[0xA0u / 4u], descriptor[0xA4u / 4u], descriptor[0xA8u / 4u]);
}

extern "C" void sly1_native_capture_model_mesh(u32 mesh_address, u32 vecs_address,
                                                   u32 nrms_address, u32 tints_address,
                                                   u32 uvs_address,
                                                   u32 vertex_count, u32 refs_address,
                                                   u32 cursor_address, u32 header_address,
                                                   u32 ents_address, u32 verts_address,
                                                   u32 nweights, u32 weights_address,
                                                   u32 header_weight_count) {
    const u32 morph_vertex_count = g_native_sly_morph_vertex_count;
    if (mesh_address == 0 || vecs_address == 0 ||
        refs_address == 0 || header_address == 0 || vertex_count < 3 ||
        vertex_count > 4096)
        return;
    const volatile u8* mesh = reinterpret_cast<const volatile u8*>(mesh_address);
    // Capture supported strip mesh builders; the live player route can contain
    // both lit kind 0 and unlit kinds such as 5.
    const u8 mk = mesh[0];
    if (!(mk == 0u || mk == 1u || mk == 4u || mk == 5u || mk == 6u ||
          mk == 7u || mk == 8u || mk == 9u || mk == 10u)) return;
    const volatile u8* refs = reinterpret_cast<const volatile u8*>(refs_address);
    const volatile float* positions = reinterpret_cast<const volatile float*>(vecs_address);
    const volatile float* normals = nrms_address != 0
        ? reinterpret_cast<const volatile float*>(nrms_address) : nullptr;
    const volatile u8* tints = reinterpret_cast<const volatile u8*>(tints_address);
    const volatile float* uvs = uvs_address != 0
        ? reinterpret_cast<const volatile float*>(uvs_address) : nullptr;
    // Each FrameSource describes a frame-strided GIF A+D state packet. These
    // packets share the VertexRefs array; they are not independent vertex spans.
    const u32 source_count = *reinterpret_cast<const volatile u32*>(mesh_address + 0x18u);
    const u32 sources_address = *reinterpret_cast<const volatile u32*>(mesh_address + 0x1Cu);
    if (source_count == 0u || source_count > 16u ||
        sources_address == 0) {
        static u32 unsupported_strip_notices = 0;
        if (unsupported_strip_notices++ < 12u)
            std::fprintf(stderr,
                         "[native sly mesh skipped] mesh=%#010x kind=%u sources=%u\n",
                         mesh_address, mesh[0], source_count);
        return;
    }
    NativeSlyMesh captured;
    captured.source_mesh = mesh_address;
    captured.kind = mk;
    captured.morph_target_count = nweights;
    captured.morph_ents_address = ents_address;
    captured.morph_vertices_address = verts_address;
    captured.morph_weights_address = weights_address;
    // StripHeader::count is the number of weights supplied for each vertex;
    // the separate PartMesh bone list maps those weights into the skin palette.
    captured.vertex_weight_count = header_weight_count;
    if (header_weight_count > 0u && header_weight_count <= 4u &&
        static_cast<u64>(header_address) + 0x2Cu <= 0x02000000ull) {
        const volatile u8* keys = reinterpret_cast<const volatile u8*>(header_address + 0x28u);
        captured.morph_keys.assign(keys, keys + header_weight_count);
    }
    captured.vertices.reserve(static_cast<std::size_t>(vertex_count) * 9u);
    captured.vertex_indices.reserve(vertex_count);
    if (weights_address != 0 && header_weight_count > 0u &&
        header_weight_count <= 4u)
        captured.vertex_weights.reserve(static_cast<std::size_t>(vertex_count) *
                                         header_weight_count);
    const volatile u32* sources = reinterpret_cast<const volatile u32*>(sources_address);
    std::int32_t frame = 0;
    if (cursor_address != 0) {
        const volatile u32* cursor = reinterpret_cast<const volatile u32*>(cursor_address);
        frame = static_cast<std::int32_t>(cursor[2]);
    }
    captured.source_frame = frame;
    captured.sources.reserve(source_count);
    for (u32 i = 0; i < source_count; ++i) {
        captured.sources.push_back({
            static_cast<std::int32_t>(sources[i * 2u]), sources[i * 2u + 1u]});
    }
    for (const auto& source : captured.sources) {
        const std::int64_t selected = static_cast<std::int64_t>(source.base) +
            static_cast<std::int64_t>(frame) * source.stride_qwc * 16;
        if (source.stride_qwc < 7 || source.stride_qwc > 4096 ||
            selected < 0x10ll || selected > 0x01FFFF80ll)
            continue;
        const volatile u64* packet = reinterpret_cast<const volatile u64*>(
            static_cast<u32>(selected));
        const u64 gif_tag = packet[0];
        const u64 gif_registers = packet[1];
        const u32 loop_count = static_cast<u32>(gif_tag & 0x7FFFu);
        const u32 format = static_cast<u32>((gif_tag >> 58u) & 3u);
        const u32 register_count = static_cast<u32>((gif_tag >> 60u) & 0xFu);
        if (format != 0u || register_count != 1u ||
            (gif_registers & 0xFu) != 0xEu || loop_count == 0u || loop_count > 6u ||
            loop_count + 1u > static_cast<u32>(source.stride_qwc))
            continue;
        NativeSlyMaterial material;
        for (u32 i = 0; i < loop_count; ++i) {
            const u64 value = packet[2u + i * 2u];
            const u32 address = static_cast<u32>(packet[3u + i * 2u] & 0x7Fu);
            switch (address) {
            // Kind-5 strips use context-2 GS registers; kind-0 uses context-1.
            case 0x47: case 0x48: material.test = value; break;   // TEST_1 / TEST_2
            case 0x06: case 0x07: material.tex0 = value; break;  // TEX0_1 / TEX0_2
            case 0x14: case 0x15: material.tex1 = value; break;  // TEX1_1 / TEX1_2
            case 0x08: case 0x09: material.clamp = value; break; // CLAMP_1 / CLAMP_2
            case 0x42: case 0x43: material.alpha = value; material.alpha_valid = true; break; // ALPHA_1 / ALPHA_2
            default: break;                                      // other GS state
            }
        }
        captured.materials.push_back(material);
    }
    if (captured.materials.empty()) return;
    const bool encode_normal_colors = mesh[0] == 0u && normals != nullptr &&
        std::getenv("OPENRAC_DEBUG_SLY_WORLD_NORMAL_COLORS") != nullptr;
    for (u32 i = 0; i < vertex_count; ++i) {
        const u8 position_index = refs[i * 4u];
        const u8 tint_index = refs[i * 4u + 3u] & 0x7Fu;
        const u8 flags = refs[i * 4u + 3u];
        captured.vertex_indices.push_back(position_index);
        const u32 source = position_index;
        const volatile float* chosen = positions;
        bool vert_filter_match = true;
        if (const char* setting = std::getenv("OPENRAC_DUMP_SLY_MESH_VERTS")) {
            if (setting[0] != '\0' && setting[0] != '1') {
                char* end = nullptr;
                const unsigned long parsed = std::strtoul(setting, &end, 0);
                if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul)
                    vert_filter_match = mesh_address == static_cast<u32>(parsed);
            }
        }
        const bool dump_all_mesh_vertices =
            std::getenv("OPENRAC_DUMP_SLY_MESH_VERTS_FULL") != nullptr;
        if (std::getenv("OPENRAC_DUMP_SLY_MESH_VERTS") != nullptr &&
            (i < 32u || dump_all_mesh_vertices) &&
            vert_filter_match) {
            static u32 vert_notices = 0;
            if (vert_notices++ < 2048u)
                std::fprintf(stderr,
                             "[native sly mesh vert] mesh=%#010x i=%u idx=%u flags=%02x xyz=%g,%g,%g\n",
                             mesh_address, i, source, flags,
                             static_cast<double>(chosen[source * 3u]),
                             static_cast<double>(chosen[source * 3u + 1u]),
                             static_cast<double>(chosen[source * 3u + 2u]));
        }
        captured.vertices.push_back(static_cast<float>(chosen[source * 3u]));
        captured.vertices.push_back(static_cast<float>(chosen[source * 3u + 1u]));
        captured.vertices.push_back(static_cast<float>(chosen[source * 3u + 2u]));
        if (weights_address != 0 && header_weight_count > 0u &&
            header_weight_count <= 4u) {
            const volatile float* input_weights =
                reinterpret_cast<const volatile float*>(weights_address);
            for (u32 j = 0; j < header_weight_count; ++j)
                captured.vertex_weights.push_back(static_cast<float>(
                    input_weights[position_index * header_weight_count + j]));
        }
        bool wrote_normal_color = false;
        if (encode_normal_colors) {
            const u8 normal_index = refs[i * 4u + 1u];
            if (normal_index != 0xFFu) {
                for (u32 channel = 0; channel < 3u; ++channel) {
                    const float normal = normals[static_cast<u32>(normal_index) * 3u + channel];
                    captured.vertices.push_back(std::clamp(normal * 0.5f + 0.5f, 0.0f, 1.0f));
                }
                captured.vertices.push_back(1.0f);
                wrote_normal_color = true;
            }
        }
        if (!wrote_normal_color) {
            for (u32 channel = 0; channel < 4u; ++channel) {
                const u8 base = mesh[4u + channel];
                const float color = tint_index == 0x7Fu || tints == nullptr
                    ? static_cast<float>((base + 1u) >> 1u) / 255.0f
                    : static_cast<float>((base * tints[tint_index * 4u + channel] + 0x100u) / 0x1FEu) / 255.0f;
                captured.vertices.push_back(color);
            }
        }
        const u8 uv_index = refs[i * 4u + 2u];
        captured.vertices.push_back(uvs != nullptr && uv_index != 0xFFu
            ? uvs[uv_index * 2u] : 0.0f);
        captured.vertices.push_back(uvs != nullptr && uv_index != 0xFFu
            ? uvs[uv_index * 2u + 1u] : 0.0f);
        // The VU consumes XYZ and supplies its homogeneous coordinate separately.
    }
    if (nweights > 0u && nweights <= 32u && ents_address >= 0x00010000u &&
        static_cast<u64>(ents_address) + static_cast<u64>(nweights) * 8u <= 0x02000000ull &&
        verts_address >= 0x00010000u && verts_address < 0x02000000u &&
        morph_vertex_count > 0u && morph_vertex_count <= 65535u &&
        static_cast<u64>(vertex_count) * nweights <= 262144ull) {
        const volatile u32* target_entries = reinterpret_cast<const volatile u32*>(ents_address);
        const volatile float* target_positions =
            reinterpret_cast<const volatile float*>(verts_address);
        std::vector<float> deltas(static_cast<std::size_t>(nweights) * vertex_count * 3u);
        bool valid_deltas = true;
        for (u32 target = 0; target < nweights && valid_deltas; ++target) {
            const u32 indices_address = target_entries[target * 2u];
            if (indices_address < 0x00010000u ||
                static_cast<u64>(indices_address) +
                    static_cast<u64>(vertex_count) * sizeof(u16) > 0x02000000ull) {
                valid_deltas = false;
                break;
            }
            const volatile u16* target_indices =
                reinterpret_cast<const volatile u16*>(indices_address);
            for (u32 vertex = 0; vertex < vertex_count; ++vertex) {
                const u32 base_index = refs[vertex * 4u];
                const u32 target_index = target_indices[vertex];
                if (target_index >= morph_vertex_count) {
                    valid_deltas = false;
                    break;
                }
                if (static_cast<u64>(verts_address) +
                        (static_cast<u64>(target_index) + 1u) * 3u * sizeof(float) >
                    0x02000000ull) {
                    valid_deltas = false;
                    break;
                }
                for (u32 axis = 0; axis < 3u; ++axis) {
                    const float base = positions[base_index * 3u + axis];
                    const float target_position = target_positions[target_index * 3u + axis];
                    const float delta = target_position - base;
                    if (!std::isfinite(base) || !std::isfinite(target_position) ||
                        !std::isfinite(delta)) {
                        valid_deltas = false;
                        break;
                    }
                    deltas[(static_cast<std::size_t>(target) * vertex_count + vertex) * 3u + axis] = delta;
                }
                if (!valid_deltas) break;
            }
        }
        if (valid_deltas) captured.morph_deltas = std::move(deltas);
    }
    std::scoped_lock lock(g_native_sly_mesh_mutex);
    g_native_sly_meshes_by_header[header_address] = std::move(captured);
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static u32 notices = 0;
        if (notices++ < 16u)
            std::fprintf(stderr, "[native sly mesh capture] mesh=%#010x header=%#010x vertices=%u registered=%zu\n",
                         mesh_address, header_address, vertex_count,
                         g_native_sly_meshes_by_header.size());
    }
}

extern "C" void sly1_native_set_morph_vertex_count(u32 count) {
    g_native_sly_morph_vertex_count = count;
}

extern "C" void sly1_native_bind_model_mesh(u32 header_address,
                                                u32 packet_address) {
    if (header_address == 0 || packet_address == 0) return;
    std::scoped_lock lock(g_native_sly_mesh_mutex);
    const auto captured = g_native_sly_meshes_by_header.find(header_address);
    if (captured == g_native_sly_meshes_by_header.end()) return;
    const std::size_t vertex_count = captured->second.vertices.size() / 9u;
    g_native_sly_meshes_by_packet[packet_address] = std::move(captured->second);
    const NativeSlyMesh& bound = g_native_sly_meshes_by_packet[packet_address];
    g_native_sly_meshes_by_header.erase(captured);
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static u32 notices = 0;
        if (notices++ < 32u)
            std::fprintf(stderr,
                         "[native sly mesh bind] header=%#010x packet=%#010x vertices=%zu\n",
                         header_address, packet_address, vertex_count);
    }
    if (std::getenv("OPENRAC_TRACE_SLY_STRIP_LAYOUT") != nullptr) {
        static u32 layout_notices = 0;
        bool layout_filter_match = true;
        if (const char* setting =
                std::getenv("OPENRAC_TRACE_SLY_STRIP_LAYOUT_SOURCE")) {
            if (setting[0] != '\0' && setting[0] != '1') {
                char* end = nullptr;
                const unsigned long parsed = std::strtoul(setting, &end, 0);
                if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul)
                    layout_filter_match = bound.source_mesh == static_cast<u32>(parsed);
            }
        }
        if (layout_filter_match && !bound.sources.empty() && layout_notices++ < 64u) {
            if (header_address < 0x10u || header_address >= 0x02000000u)
                return;
            const volatile u8* header = reinterpret_cast<const volatile u8*>(header_address);
            const u32 unpack_address = *reinterpret_cast<const volatile u32*>(header + 0x1Cu);
            const std::int32_t header_count =
                *reinterpret_cast<const volatile std::int32_t*>(header + 0x24u);
            std::fprintf(stderr,
                         "[native sly strip layout] mesh=%#010x header=%#010x packet=%#010x verts=%zu sources=%zu frame=%d unpack=%#010x count=%d",
                         bound.source_mesh, header_address, packet_address, vertex_count,
                         bound.sources.size(), bound.source_frame, unpack_address,
                         header_count);
            if (unpack_address >= 0x10u && unpack_address < 0x02000000u) {
                const volatile u16* unpack = reinterpret_cast<const volatile u16*>(unpack_address);
                std::fprintf(stderr,
                             " off=%u,%u,%u,%u strip=%u,%u,%u,%u flags=%u,%u",
                             unpack[0], unpack[1], unpack[2], unpack[3],
                             unpack[4], unpack[5], unpack[6], unpack[7],
                             unpack[8], unpack[9]);
            }
            std::fprintf(stderr, "\n");
            for (std::size_t i = 0; i < bound.sources.size(); ++i) {
                const auto& source = bound.sources[i];
                const std::int64_t selected = static_cast<std::int64_t>(source.base) +
                    static_cast<std::int64_t>(bound.source_frame) * source.stride_qwc * 16;
                u64 tag = 0;
                u64 regs = 0;
                if (source.stride_qwc > 0 && selected >= 0x10ll &&
                    selected <= 0x01FFFF80ll) {
                    const volatile u64* packet = reinterpret_cast<const volatile u64*>(
                        static_cast<u32>(selected));
                    tag = packet[0];
                    regs = packet[1];
                }
                std::fprintf(stderr,
                             "[native sly strip source] mesh=%#010x index=%zu stride_qwc=%d base=%#010x selected=%#010llx gif=%016llx,%016llx",
                             bound.source_mesh, i, source.stride_qwc, source.base,
                             static_cast<unsigned long long>(selected),
                             static_cast<unsigned long long>(tag),
                             static_cast<unsigned long long>(regs));
                const u32 gif_loops = static_cast<u32>(tag & 0x7FFFu);
                if (tag != 0 && gif_loops <= 6u &&
                    gif_loops + 1u <= static_cast<u32>(source.stride_qwc)) {
                    const volatile u64* packet = reinterpret_cast<const volatile u64*>(
                        static_cast<u32>(selected));
                    for (u32 entry = 0; entry < gif_loops; ++entry) {
                        const u64 value = packet[2u + entry * 2u];
                        const u32 address = static_cast<u32>(
                            packet[3u + entry * 2u] & 0x7Fu);
                        std::fprintf(stderr, " r%02x=%016llx", address,
                                     static_cast<unsigned long long>(value));
                    }
                }
                std::fprintf(stderr, "\n");
            }
        }
    }
}

static bool native_trace_player_tree_owner(u32 candidate) {
    const u32 player = D_00262E10;
    if (player < 0x00010000u || player >= 0x02000000u) return false;
    for (u32 depth = 0; depth < 16u && candidate >= 0x00010000u &&
         candidate < 0x02000000u; ++depth) {
        if (candidate == player) return true;
        candidate = *(volatile u32*)(candidate + 0x18u);
    }
    return false;
}

extern "C" void sly1_native_trace_model_hook(u32 owner_address, u32 part_index,
                                               u32 source_part, u32 scratch_part,
                                               u32 hook_address, u32 kind) {
    const bool player_trace = std::getenv("OPENRAC_TRACE_PLAYER") != nullptr &&
                              native_trace_player_tree_owner(owner_address);
    if (!g_startup_trace_enabled && !player_trace) return;
    static std::atomic<u32> trace_count{0};
    if (trace_count.fetch_add(1u, std::memory_order_relaxed) >= 128u) return;
    const u32 source_hook = source_part != 0
        ? *reinterpret_cast<const volatile u32*>(source_part + 0x38u) : 0u;
    const u32 owner_id = owner_address != 0
        ? *reinterpret_cast<const volatile u32*>(owner_address + 8u) : 0u;
    std::fprintf(stderr,
                 "[native model hook] owner=%#010x owner_id=%u index=%u kind=%u source=%#010x source_hook=%#010x scratch=%#010x scratch_hook=%#010x\n",
                 owner_address, owner_id, part_index, kind, source_part,
                 source_hook, scratch_part, hook_address);
}

extern "C" void sly1_native_trace_queue_model_parts(u32 owner_address,
                                                       u32 camera_address,
                                                       u32 part_count,
                                                       u32 parts_address,
                                                       u32 states_address) {
    const bool player_trace = std::getenv("OPENRAC_TRACE_PLAYER") != nullptr &&
                              native_trace_player_tree_owner(owner_address);
    if (!g_startup_trace_enabled && !player_trace) return;
    static std::atomic<u32> trace_count{0};
    const u32 sample = trace_count.fetch_add(1u, std::memory_order_relaxed);
    if (sample >= 64u) return;
    std::fprintf(stderr,
                 "[native QueueModelPartDraws] #%u owner=%#010x camera=%#010x count=%u parts=%#010x states=%#010x\n",
                 sample, owner_address, camera_address, part_count,
                 parts_address, states_address);
    if (sample == 0u && parts_address != 0 && states_address != 0) {
        const volatile u32* p = reinterpret_cast<const volatile u32*>(parts_address);
        const volatile u32* s = reinterpret_cast<const volatile u32*>(states_address);
        std::fprintf(stderr,
                     "[native QueueModelPartDraws source0] pos=%08x,%08x,%08x,%08x radius=%08x kind=%08x near=%08x far=%08x dist=%08x hook=%08x matrix=%08x state_mask=%08x fade=%08x until=%08x\n",
                     p[0], p[1], p[2], p[3], p[4], p[5], p[7], p[8],
                     p[9], p[14], p[26], s[0], s[4], s[5]);
    }
}

extern "C" void sly1_native_trace_queue_model_result(
    u32 owner_address, u32 views, u32 seen, u32 mask_culled,
    u32 plane_culled, u32 fade_rejected, u32 fade_zero, u32 queued,
    u32 part_count) {
    const bool player_trace = std::getenv("OPENRAC_TRACE_PLAYER") != nullptr &&
                              native_trace_player_tree_owner(owner_address);
    if (!g_startup_trace_enabled && !player_trace) return;
    static std::atomic<u32> trace_count{0};
    const u32 sample = trace_count.fetch_add(1u, std::memory_order_relaxed);
    if (sample >= 64u) return;
    std::fprintf(stderr,
                 "[native QueueModelPartDraws result] #%u owner=%#010x views=%#x parts=%u seen=%u mask=%u plane=%u fade=%u zero=%u queued=%u\n",
                 sample, owner_address, views, part_count, seen,
                 mask_culled, plane_culled, fade_rejected, fade_zero, queued);
}

extern "C" void sly1_native_trace_model_cull(
    u32 owner_address, u32 part_index, u32 state_mask, u32 views,
    float pos_x, float pos_y, float pos_z, float radius, float near_fade,
    u32 signs) {
    if (!g_startup_trace_enabled) return;
    static std::atomic<u32> trace_count{0};
    const u32 sample = trace_count.fetch_add(1u, std::memory_order_relaxed);
    if (sample >= 32u) return;
    std::fprintf(stderr,
                 "[native model cull] #%u owner=%#010x part=%u mask=%#x views=%#x pos=%g,%g,%g radius=%g near=%g signs=%#x\n",
                 sample, owner_address, part_index, state_mask, views,
                 pos_x, pos_y, pos_z, radius, near_fade, signs);
}

extern "C" void sly1_native_trace_spr_block(
    u32 owner_address, u32 source_parts, u32 scratch_parts,
    u32 source_states, u32 scratch_states, u32 count, u32 transfer_index) {
    if (!g_startup_trace_enabled) return;
    static std::atomic<u32> trace_count{0};
    const u32 sample = trace_count.fetch_add(1u, std::memory_order_relaxed);
    if (sample >= 16u || count > 52u) return;
    const volatile u32* src = reinterpret_cast<const volatile u32*>(source_parts);
    const volatile u32* dst = reinterpret_cast<const volatile u32*>(scratch_parts);
    const volatile u32* src_state = reinterpret_cast<const volatile u32*>(source_states);
    const volatile u32* dst_state = reinterpret_cast<const volatile u32*>(scratch_states);
    const u32 part_words = count * (0x70u / 4u);
    const u32 state_words = count * (0x28u / 4u);
    u32 part_mismatches = 0, state_mismatches = 0;
    u32 first_part_mismatch = 0xFFFFFFFFu, first_state_mismatch = 0xFFFFFFFFu;
    for (u32 i = 0; i < part_words; ++i) {
        if (src[i] != dst[i]) {
            if (first_part_mismatch == 0xFFFFFFFFu) first_part_mismatch = i;
            ++part_mismatches;
        }
    }
    for (u32 i = 0; i < state_words; ++i) {
        if (src_state[i] != dst_state[i]) {
            if (first_state_mismatch == 0xFFFFFFFFu) first_state_mismatch = i;
            ++state_mismatches;
        }
    }
    std::fprintf(stderr,
                 "[native SPR block compare] #%u owner=%#010x transfer=%u count=%u part_mismatch=%u first=%u src=%08x,%08x,%08x,%08x dst=%08x,%08x,%08x,%08x state_mismatch=%u first=%u masks=%08x/%08x\n",
                 sample, owner_address, transfer_index, count,
                 part_mismatches, first_part_mismatch,
                 src[0], src[1], src[2], src[3], dst[0], dst[1], dst[2], dst[3],
                 state_mismatches, first_state_mismatch, src_state[0], dst_state[0]);
}

extern "C" void sly1_native_trace_mesh_sources(u32 mesh_address, u32 cursor_address) {
    if (mesh_address == 0 || std::getenv("OPENRAC_TRACE_SLY_FRAME_SOURCES") == nullptr)
        return;
    const volatile u32* mesh = reinterpret_cast<const volatile u32*>(mesh_address);
    if (const char* setting = std::getenv("OPENRAC_TRACE_SLY_FRAME_SOURCE_MESH")) {
        if (setting[0] != '\0' && setting[0] != '1') {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 0);
            if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul &&
                mesh_address != static_cast<u32>(parsed))
                return;
        }
    }
    const u32 count = mesh[0x18 / 4];
    const u32 sources_address = mesh[0x1C / 4];
    const std::int32_t frame_count = static_cast<std::int32_t>(mesh[0x20 / 4]);
    std::int32_t frame = 0;
    u32 cursor_flags = 0;
    if (cursor_address != 0) {
        const volatile u32* cursor = reinterpret_cast<const volatile u32*>(cursor_address);
        cursor_flags = cursor[0];
        frame = static_cast<std::int32_t>(cursor[2]);
    }
    static std::unordered_map<u64, u64> last_state;
    static u32 notices = 0;
    const u64 key = (static_cast<u64>(mesh_address) << 32u) | cursor_address;
    const u64 state = (static_cast<u64>(cursor_flags) << 32u) |
                      static_cast<u32>(frame);
    const auto previous = last_state.find(key);
    if (previous != last_state.end() && previous->second == state) return;
    if (notices++ >= 96u) return;
    last_state[key] = state;
    std::fprintf(stderr,
                 "[native sly frame sources] mesh=%#010x kind=%u sources=%u frame_count=%d cursor=%#010x flags=%#x frame=%d\n",
                 mesh_address, *reinterpret_cast<const volatile u8*>(mesh_address),
                 count, frame_count, cursor_address, cursor_flags, frame);
    if (count == 0u || count > 16u || sources_address == 0) return;
    const volatile u32* sources = reinterpret_cast<const volatile u32*>(sources_address);
    for (u32 i = 0; i < count; ++i) {
        const std::int32_t stride = static_cast<std::int32_t>(sources[i * 2u]);
        const u32 base = sources[i * 2u + 1u];
        const std::int64_t offset = static_cast<std::int64_t>(frame) * stride * 16;
        const std::int64_t selected = static_cast<std::int64_t>(base) + offset;
        std::fprintf(stderr,
                     "[native sly frame source] mesh=%#010x index=%u stride_qwc=%d base=%#010x selected=%#010llx bytes=%lld\n",
                     mesh_address, i, stride, base,
                     static_cast<unsigned long long>(selected),
                     static_cast<long long>(stride) * 16);
        if (std::getenv("OPENRAC_TRACE_SLY_FRAME_SOURCE_DATA") != nullptr &&
            stride > 0 && stride <= 16 && selected >= 0x10ll &&
            selected + static_cast<std::int64_t>(stride) * 16 <= 0x02000000ll) {
            const volatile u64* words = reinterpret_cast<const volatile u64*>(
                static_cast<u32>(selected));
            for (std::int32_t qword = 0; qword < stride; ++qword) {
                std::fprintf(stderr,
                             "[native sly frame source data] mesh=%#010x index=%u qword=%d lo=%016llx hi=%016llx\n",
                             mesh_address, i, qword,
                             static_cast<unsigned long long>(words[qword * 2]),
                             static_cast<unsigned long long>(words[qword * 2 + 1]));
            }
        }
    }
}

extern "C" void sly1_native_submit_model_part(u32 request_address, u32 world,
                                                 u32 camera_address) {
    if (request_address == 0 || world == 0 || camera_address == 0) return;
    // Isolation flag: skip all mesh submission to test whether the intermittent
    // host-heap corruption originates in this path.
    if (getenv("OPENRAC_NO_MESH_SUBMIT") != nullptr) return;
    const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
    const u32 part_address = request[0x78 / 4];
    if (part_address == 0) return;
    const u32 owner_address = request[0x70 / 4];
    u32 owner_class = 0xFFFFFFFFu;
    u32 class_address = 0;
    if (owner_address >= 0x00010000u && owner_address <= 0x01FFFFFCu) {
        class_address = *reinterpret_cast<const volatile u32*>(owner_address);
        const bool class_in_guest_ram = class_address >= 0x00010000u &&
            class_address <= 0x01FFFFF8u;
        const bool class_in_relocated_data = class_address >= 0x08000000u &&
            class_address < 0x10000000u;
        if (class_in_guest_ram || class_in_relocated_data)
            owner_class = *reinterpret_cast<const volatile u32*>(class_address + 4u);
    }
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static u32 owner_notices = 0;
        if (owner_notices++ < 16u)
            std::fprintf(stderr,
                         "[native sly world owner] request=%#010x owner=%#010x descriptor=%#010x class=%u\n",
                         request_address, owner_address, class_address, owner_class);
    }
    u32 target_tbp = 0;
    u32 target_width = 0;
    u32 target_height = 0;
    u32 target_range_address = 0;
    const bool is_render_target = owner_class == 0x61u;
    if (owner_class == 0x61u) {
        const auto readable = [](u32 address, u32 size) {
            const u64 end = static_cast<u64>(address) + size;
            return (address >= 0x00010000u && end <= 0x02000000ull) ||
                   (address >= 0x08000000u && end <= 0x10000000ull);
        };
        u32 anim_record = 0;
        u32 texture_table = 0;
        u32 entries = 0;
        u32 texture_header = 0;
        u32 entry_count = 0;
        u64 target_tex0 = 0;
        if (readable(owner_address + 0x2D0u, sizeof(u32)))
            target_range_address = *reinterpret_cast<const volatile u32*>(owner_address + 0x2D0u);
        if (readable(target_range_address, sizeof(u32)))
            anim_record = *reinterpret_cast<const volatile u32*>(target_range_address);
        if (readable(anim_record + 0x14u, sizeof(u32)))
            texture_table = *reinterpret_cast<const volatile u32*>(anim_record + 0x14u);
        if (readable(texture_table, 0x10u)) {
            const volatile u8* table = reinterpret_cast<const volatile u8*>(texture_table);
            entry_count = table[0x04u];
            entries = *reinterpret_cast<const volatile u32*>(table + 0x0Cu);
        }
        if (entry_count != 0 && readable(entries, sizeof(u32)))
            texture_header = *reinterpret_cast<const volatile u32*>(entries);
        if (readable(texture_header, 0x20u)) {
            const volatile u8* header = reinterpret_cast<const volatile u8*>(texture_header);
            target_width = *reinterpret_cast<const volatile u16*>(header);
            target_height = *reinterpret_cast<const volatile u16*>(header + 2u);
            target_tex0 = *reinterpret_cast<const volatile u64*>(header + 0x18u);
            target_tbp = static_cast<u32>(target_tex0 & 0x3FFFu);
        }
        if (std::getenv("OPENRAC_TRACE_SLY_RENDER_TARGETS") != nullptr) {
            static u32 traced_target_owner = 0;
            if (traced_target_owner != owner_address) {
                traced_target_owner = owner_address;
                std::fprintf(stderr,
                             "[native sly render target] request=%#010x owner=%#010x range=%#010x anim=%#010x table=%#010x entries=%#010x count=%u texture=%#010x size=%ux%u tex0=%#llx\n",
                             request_address, owner_address, target_range_address, anim_record,
                             texture_table, entries, entry_count, texture_header,
                             target_width, target_height,
                             static_cast<unsigned long long>(target_tex0));
                if (target_range_address != 0) {
                    const volatile float* source_matrix =
                        reinterpret_cast<const volatile float*>(target_range_address + 0x70u);
                    const volatile float* draw_matrix =
                        reinterpret_cast<const volatile float*>(request_address + 0x80u);
                    std::fprintf(stderr, "[native sly target matrices] range=");
                    for (u32 element = 0; element < 16u; ++element)
                        std::fprintf(stderr, "%s%g", element == 0u ? "" : ",",
                                     static_cast<double>(source_matrix[element]));
                    std::fprintf(stderr, " request=");
                    for (u32 element = 0; element < 16u; ++element)
                        std::fprintf(stderr, "%s%g", element == 0u ? "" : ",",
                                     static_cast<double>(draw_matrix[element]));
                    std::fputc('\n', stderr);
                }
            }
        }
    }
    const volatile u32* part = reinterpret_cast<const volatile u32*>(part_address);
    u32 mesh_count = part[0x58 / 4];
    if (mesh_count > 128u) mesh_count = 128u;
    const u32 mesh_address = part[0x5C / 4];
    if (mesh_count == 0 || mesh_address == 0) return;
    const float* model = reinterpret_cast<const float*>(request_address + 0x10u);
    float* view_projection = reinterpret_cast<float*>(
        is_render_target ? request_address + 0x80u : camera_address + 0x100u);
    if (is_render_target) {
        bool missing_target_matrix = true;
        for (u32 element = 0; element < 16u; ++element)
            missing_target_matrix = missing_target_matrix && view_projection[element] == 0.0f;
        if (missing_target_matrix) {
            static u32 generated_target_matrices = 0;
            const bool generated = native_build_range_record_matrix(
                target_range_address, view_projection);
            if (generated && std::getenv("OPENRAC_TRACE_SLY_RENDER_TARGETS") != nullptr &&
                generated_target_matrices < 8u) {
                const volatile float* range = reinterpret_cast<const volatile float*>(
                    target_range_address);
                std::fprintf(stderr,
                             "[native sly target matrix] generated=%u range=%#010x request=%#010x near=%g far=%g inner=%g outer=%g pos=%g,%g,%g axis=%g,%g,%g hint=%g,%g,%g vp=%g,%g,%g,%g\n",
                             generated ? 1u : 0u, target_range_address,
                             request_address,
                             range[0x04u / 4u], range[0x08u / 4u],
                             range[0x40u / 4u], range[0x44u / 4u],
                             range[0x10u / 4u], range[0x14u / 4u], range[0x18u / 4u],
                             range[0x20u / 4u], range[0x24u / 4u], range[0x28u / 4u],
                             range[0x30u / 4u], range[0x34u / 4u], range[0x38u / 4u],
                             view_projection[0], view_projection[5],
                             view_projection[10], view_projection[15]);
            }
            if (generated) ++generated_target_matrices;
        }
    }
    const u32 skin_address = request[0x74 / 4];
    if (const char* setting =
            std::getenv("OPENRAC_TRACE_SLY_GL_TRANSFORM_REQUEST")) {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        static u32 transform_trace_samples = 0;
        if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul &&
            request_address == static_cast<u32>(parsed) &&
            transform_trace_samples++ < 4u) {
            const volatile float* owner_local = owner_address >= 0x00010000u &&
                    static_cast<u64>(owner_address) + 0x110u <= 0x02000000ull
                ? reinterpret_cast<const volatile float*>(owner_address + 0x100u)
                : nullptr;
            const volatile float* owner_world = owner_address >= 0x00010000u &&
                    static_cast<u64>(owner_address) + 0x150u <= 0x02000000ull
                ? reinterpret_cast<const volatile float*>(owner_address + 0x140u)
                : nullptr;
            const u32 parent_address = owner_address >= 0x00010000u &&
                    static_cast<u64>(owner_address) + 0x1Cu <= 0x02000000ull
                ? *reinterpret_cast<const volatile u32*>(owner_address + 0x18u) : 0u;
            const u32 part_matrix_address = *reinterpret_cast<const volatile u32*>(
                part_address + 0x68u);
            const u32 camera_facing_address = *reinterpret_cast<const volatile u32*>(
                part_address + 0x34u);
            const volatile float* parent_world = parent_address >= 0x00010000u &&
                    static_cast<u64>(parent_address) + 0x150u <= 0x02000000ull
                ? reinterpret_cast<const volatile float*>(parent_address + 0x140u)
                : nullptr;
            std::fprintf(stderr,
                         "[native sly GL transform audit] request=%#010x owner=%#010x id=%u class=%u parent=%#010x part=%#010x part_matrix=%#010x camera_facing=%#010x part_local_pos=%g,%g,%g request_pos=%g,%g,%g radius=%g lit=%u skinned=%u model_pos=%g,%g,%g owner_pos=%g,%g,%g owner_world_pos=%g,%g,%g parent_world_pos=%g,%g,%g\n",
                         request_address, owner_address,
                         owner_address >= 0x00010000u && owner_address <= 0x01FFFFFCu
                             ? *reinterpret_cast<const volatile u32*>(owner_address + 8u) : 0u,
                         owner_class, parent_address, part_address,
                         part_matrix_address, camera_facing_address,
                         static_cast<double>(*reinterpret_cast<const volatile float*>(part_address + 0x00u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(part_address + 0x04u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(part_address + 0x08u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(request_address + 0x60u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(request_address + 0x64u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(request_address + 0x68u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(part_address + 0x10u)),
                         *reinterpret_cast<const volatile u32*>(part_address + 0x18u),
                         *reinterpret_cast<const volatile u32*>(part_address + 0x28u),
                         model[12], model[13], model[14],
                         owner_local != nullptr ? static_cast<double>(owner_local[0]) : 0.0,
                         owner_local != nullptr ? static_cast<double>(owner_local[1]) : 0.0,
                         owner_local != nullptr ? static_cast<double>(owner_local[2]) : 0.0,
                         owner_world != nullptr ? static_cast<double>(owner_world[0]) : 0.0,
                         owner_world != nullptr ? static_cast<double>(owner_world[1]) : 0.0,
                         owner_world != nullptr ? static_cast<double>(owner_world[2]) : 0.0,
                         parent_world != nullptr ? static_cast<double>(parent_world[0]) : 0.0,
                         parent_world != nullptr ? static_cast<double>(parent_world[1]) : 0.0,
                         parent_world != nullptr ? static_cast<double>(parent_world[2]) : 0.0);
            for (u32 row = 0; row < 4u; ++row) {
                std::fprintf(stderr, "[native sly GL transform audit] model%u=", row);
                for (u32 column = 0; column < 4u; ++column)
                    std::fprintf(stderr, "%s%g", column == 0u ? "" : ",",
                                 static_cast<double>(model[row * 4u + column]));
                std::fprintf(stderr, " camera%u=", row);
                for (u32 column = 0; column < 4u; ++column)
                    std::fprintf(stderr, "%s%g", column == 0u ? "" : ",",
                                 static_cast<double>(view_projection[row * 4u + column]));
                std::fputc('\n', stderr);
            }
            if (native_guest_range_readable(part_matrix_address, 16u * sizeof(float))) {
                const volatile float* part_matrix =
                    reinterpret_cast<const volatile float*>(part_matrix_address);
                std::fprintf(stderr, "[native sly GL transform audit] part_matrix_rows=");
                for (u32 element = 0; element < 16u; ++element)
                    std::fprintf(stderr, "%s%g", element == 0u ? "" : ",",
                                 static_cast<double>(part_matrix[element]));
                std::fputc('\n', stderr);
            }
        }
    }
    if (std::getenv("OPENRAC_TRACE_SLY_CAMERA") != nullptr &&
        native_trace_player_tree_owner(owner_address)) {
        static u32 camera_trace_notices = 0;
        if (camera_trace_notices++ < 24u) {
            const volatile float* camera_position =
                reinterpret_cast<const volatile float*>(camera_address + 0x40u);
            const volatile float* view_projection =
                reinterpret_cast<const volatile float*>(camera_address + 0x100u);
            const volatile float* model =
                reinterpret_cast<const volatile float*>(request_address + 0x10u);
            const volatile float* part = reinterpret_cast<const volatile float*>(part_address);
            std::fprintf(stderr,
                         "[native sly camera] request=%#010x owner=%#010x world=%#010x camera=%#010x pos=%g,%g,%g zoom=%g->%g distance_scale=%g proj_scale=%g,%g near_far=%g,%g request_fade=%g,%g part_radius_scale=%g part_radius=%g model_pos=%g,%g,%g vp_row0=%g,%g,%g,%g vp_row1=%g,%g,%g,%g vp_row3=%g,%g,%g,%g\n",
                         request_address, owner_address, world, camera_address,
                         static_cast<double>(camera_position[0]),
                         static_cast<double>(camera_position[1]),
                         static_cast<double>(camera_position[2]),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1C4u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1C8u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1C0u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1F0u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1F4u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1E4u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(camera_address + 0x1E8u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(request_address + 0x50u)),
                         static_cast<double>(*reinterpret_cast<const volatile float*>(request_address + 0x54u)),
                         static_cast<double>(part[0x50u / 4u]),
                         static_cast<double>(part[0x10u / 4u]),
                         static_cast<double>(model[12]), static_cast<double>(model[13]),
                         static_cast<double>(model[14]),
                         static_cast<double>(view_projection[0]),
                         static_cast<double>(view_projection[1]),
                         static_cast<double>(view_projection[2]),
                         static_cast<double>(view_projection[3]),
                         static_cast<double>(view_projection[4]),
                         static_cast<double>(view_projection[5]),
                         static_cast<double>(view_projection[6]),
                         static_cast<double>(view_projection[7]),
                         static_cast<double>(view_projection[12]),
                         static_cast<double>(view_projection[13]),
                         static_cast<double>(view_projection[14]),
                         static_cast<double>(view_projection[15]));
        }
    }
    if (std::getenv("OPENRAC_TRACE_SLY_CAMERA_TARGET") != nullptr &&
        owner_class == 2u) {
        static u32 target_trace_notices = 0;
        if (target_trace_notices++ < 24u) {
            const volatile u32* camera =
                reinterpret_cast<const volatile u32*>(camera_address);
            const u32 target_address = camera[0x2E0u / 4u];
            const volatile float* target_record_position =
                reinterpret_cast<const volatile float*>(camera_address + 0x300u);
            const volatile float* target_record_focus =
                reinterpret_cast<const volatile float*>(camera_address + 0x330u);
            const volatile float* look_focus =
                reinterpret_cast<const volatile float*>(camera_address + 0x2D0u);
            const volatile float* tracking_offset =
                reinterpret_cast<const volatile float*>(camera_address + 0x250u);
            const volatile float* owner_position = owner_address >= 0x00010000u &&
                    owner_address <= 0x01FFFEBCu
                ? reinterpret_cast<const volatile float*>(owner_address + 0x140u)
                : nullptr;
            const volatile float* target_position = target_address >= 0x00010000u &&
                    target_address <= 0x01FFFEBCu
                ? reinterpret_cast<const volatile float*>(target_address + 0x140u)
                : nullptr;
            std::fprintf(stderr,
                         "[native sly camera target] owner=%#010x owner_id=%u target=%#010x camera=%#010x target_record=%g,%g,%g,%g target_focus=%g,%g,%g,%g look_focus=%g,%g,%g,%g tracking_offset=%g,%g,%g,%g owner_world=%g,%g,%g target_world=%g,%g,%g\n",
                         owner_address,
                         *reinterpret_cast<const volatile u32*>(owner_address + 8u),
                         target_address, camera_address,
                         static_cast<double>(target_record_position[0]),
                         static_cast<double>(target_record_position[1]),
                         static_cast<double>(target_record_position[2]),
                         static_cast<double>(target_record_position[3]),
                         static_cast<double>(target_record_focus[0]),
                         static_cast<double>(target_record_focus[1]),
                         static_cast<double>(target_record_focus[2]),
                         static_cast<double>(target_record_focus[3]),
                         static_cast<double>(look_focus[0]),
                         static_cast<double>(look_focus[1]),
                         static_cast<double>(look_focus[2]),
                         static_cast<double>(look_focus[3]),
                         static_cast<double>(tracking_offset[0]),
                         static_cast<double>(tracking_offset[1]),
                         static_cast<double>(tracking_offset[2]),
                         static_cast<double>(tracking_offset[3]),
                         owner_position != nullptr ? static_cast<double>(owner_position[0]) : 0.0,
                         owner_position != nullptr ? static_cast<double>(owner_position[1]) : 0.0,
                         owner_position != nullptr ? static_cast<double>(owner_position[2]) : 0.0,
                         target_position != nullptr ? static_cast<double>(target_position[0]) : 0.0,
                         target_position != nullptr ? static_cast<double>(target_position[1]) : 0.0,
                         target_position != nullptr ? static_cast<double>(target_position[2]) : 0.0);
        }
    }
    const u32 attachment_address = owner_address >= 0x00010000u &&
            owner_address <= 0x01FFFFD0u
        ? owner_address + 0x22Cu : 0u;
    const volatile u32* attachment_block = attachment_address != 0
        ? reinterpret_cast<const volatile u32*>(attachment_address) : nullptr;
    const std::int32_t attachment_count_raw = attachment_block != nullptr
        ? *reinterpret_cast<const volatile std::int32_t*>(attachment_address) : 0;
    const u32 attachment_count = attachment_count_raw > 0 &&
            attachment_count_raw <= 256
        ? static_cast<u32>(attachment_count_raw) : 0u;
    const u32 attachment_items = attachment_block != nullptr
        ? attachment_block[1] : 0u;
    const u32 attachment_keys = attachment_block != nullptr
        ? attachment_block[2] : 0u;
    const bool trace_attachments =
        std::getenv("OPENRAC_TRACE_SLY_ATTACHMENTS") != nullptr;
    static u32 traced_attachment_owner = 0;
    bool trace_this_owner = false;
    std::array<bool, 256> traced_bones{};
    static u32 notices = 0;
    for (u32 i = 0; i < mesh_count; ++i) {
        const u32 part_mesh = mesh_address + i * 0x50u;
        const u32 source_mesh = *reinterpret_cast<const volatile u32*>(part_mesh + 0x18u);
        const u32 packet_address = *reinterpret_cast<const volatile u32*>(part_mesh + 0x44u);
        if (source_mesh == 0) continue;
        const u8 kind = *reinterpret_cast<const volatile u8*>(source_mesh);
        if (!(kind == 0u || kind == 1u || kind == 4u || kind == 5u || kind == 6u ||
              kind == 7u || kind == 8u || kind == 9u || kind == 10u))
            continue;
        const u32 bone_count = *reinterpret_cast<const volatile u32*>(part_mesh + 0x24u);
        const bool trace_owner_candidate = trace_attachments && bone_count != 0u &&
            native_trace_player_tree_owner(owner_address) &&
            owner_address != 0 && owner_address != traced_attachment_owner;
        NativeSlyMesh mesh;
        {
            std::scoped_lock lock(g_native_sly_mesh_mutex);
            const auto found = g_native_sly_meshes_by_packet.find(packet_address);
            if (packet_address == 0 || found == g_native_sly_meshes_by_packet.end()) {
                if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr && notices++ < 32u)
                    std::fprintf(stderr, "[native sly mesh miss] source=%#010x packet=%#010x kind=%u registered=%zu request=%#010x\n",
                                 source_mesh, packet_address,
                                 *reinterpret_cast<const volatile u8*>(source_mesh),
                                 g_native_sly_meshes_by_packet.size(), request_address);
                continue;
            }
            // Opt-in: validate the shared map entry's vector sizes before the
            // deep copy. A corrupted size here would drive a bogus allocation
            // in operator= (the glibc detection point).
            if (getenv("OPENRAC_CHECK_MESH_SANITY") != nullptr) {
                const auto& src = found->second;
                if (src.vertices.size() > 4096u * 9u ||
                    src.materials.size() > 64u || src.sources.size() > 64u ||
                    (src.vertices.size() % 9u) != 0u) {
                    std::fprintf(stderr,
                        "[mesh-sanity] BAD entry packet=%#010x verts=%zu mats=%zu srcs=%zu\n",
                        packet_address, src.vertices.size(),
                        src.materials.size(), src.sources.size());
                }
            }
            mesh = found->second;
        }
        if (const char* setting =
                std::getenv("OPENRAC_TRACE_SLY_GL_TRANSFORM_REQUEST")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(setting, &end, 0);
            static u32 coordinate_trace_samples = 0;
            if (end != setting && *end == '\0' && parsed <= 0xFFFFFFFFul &&
                request_address == static_cast<u32>(parsed) &&
                coordinate_trace_samples++ < 8u && !mesh.vertices.empty()) {
                double local_sum[3]{};
                double row_sum[3]{};
                double column_sum[3]{};
                const std::size_t vertices = mesh.vertices.size() / 9u;
                for (std::size_t vertex = 0; vertex < vertices; ++vertex) {
                    const float* position = mesh.vertices.data() + vertex * 9u;
                    const double x = position[0];
                    const double y = position[1];
                    const double z = position[2];
                    local_sum[0] += x;
                    local_sum[1] += y;
                    local_sum[2] += z;
                    row_sum[0] += x * model[0] + y * model[4] + z * model[8] + model[12];
                    row_sum[1] += x * model[1] + y * model[5] + z * model[9] + model[13];
                    row_sum[2] += x * model[2] + y * model[6] + z * model[10] + model[14];
                    column_sum[0] += x * model[0] + y * model[1] + z * model[2] + model[3];
                    column_sum[1] += x * model[4] + y * model[5] + z * model[6] + model[7];
                    column_sum[2] += x * model[8] + y * model[9] + z * model[10] + model[11];
                }
                const double inverse_count = 1.0 / static_cast<double>(vertices);
                const volatile float* request_position =
                    reinterpret_cast<const volatile float*>(request_address + 0x60u);
                for (u32 axis = 0; axis < 3u; ++axis) {
                    local_sum[axis] *= inverse_count;
                    row_sum[axis] *= inverse_count;
                    column_sum[axis] *= inverse_count;
                }
                const double row_delta = std::sqrt(
                    std::pow(row_sum[0] - request_position[0], 2.0) +
                    std::pow(row_sum[1] - request_position[1], 2.0) +
                    std::pow(row_sum[2] - request_position[2], 2.0));
                const double column_delta = std::sqrt(
                    std::pow(column_sum[0] - request_position[0], 2.0) +
                    std::pow(column_sum[1] - request_position[1], 2.0) +
                    std::pow(column_sum[2] - request_position[2], 2.0));
                std::fprintf(stderr,
                             "[native sly GL coordinate audit] request=%#010x source=%#010x mesh=%u vertices=%zu local_centroid=%g,%g,%g row_transformed_centroid=%g,%g,%g row_delta_to_request_pos=%g column_transformed_centroid=%g,%g,%g column_delta_to_request_pos=%g part_radius=%g\n",
                             request_address, source_mesh, i, vertices,
                             local_sum[0], local_sum[1], local_sum[2],
                             row_sum[0], row_sum[1], row_sum[2], row_delta,
                             column_sum[0], column_sum[1], column_sum[2], column_delta,
                             static_cast<double>(*reinterpret_cast<const volatile float*>(
                                 part_address + 0x10u)));
            }
        }
        if (std::getenv("OPENRAC_TRACE_SLY_MESH_DEFORM") != nullptr &&
            (mesh.kind == 0u || mesh.kind == 6u)) {
            static u32 deformation_notices = 0;
            if (deformation_notices++ < 512u)
                std::fprintf(stderr,
                             "[native sly deformation inputs] world=%#010x request=%#010x owner=%#010x source=%#010x kind=%u vertices=%zu morph_targets=%u skin_weights=%u ents=%#010x verts=%#010x weights=%#010x\n",
                             world, request_address, owner_address, mesh.source_mesh,
                             mesh.kind, mesh.vertices.size() / 9u,
                             mesh.morph_target_count, mesh.vertex_weight_count,
                             mesh.morph_ents_address, mesh.morph_vertices_address,
                             mesh.morph_weights_address);
        }
        if (trace_owner_candidate) {
            traced_attachment_owner = owner_address;
            trace_this_owner = true;
            std::fprintf(stderr,
                         "[native sly attachment list] owner=%#010x skin=%#010x list=%#010x count=%d items=%#010x keys=%#010x palette=%#010x\n",
                         owner_address, skin_address, attachment_address,
                         attachment_count_raw, attachment_items, attachment_keys,
                         skin_address >= 0x00010000u && skin_address < 0x02000000u
                             ? *reinterpret_cast<const volatile u32*>(skin_address + 0x04u)
                             : 0u);
        }
        const u32 bone_array_address = skin_address >= 0x00010000u &&
                static_cast<u64>(skin_address) + 0x08u <= 0x02000000ull
            ? *reinterpret_cast<const volatile u32*>(skin_address + 0x04u) : 0u;
        const volatile u32* bone_indices = bone_count > 0u && bone_count <= 4u
            ? reinterpret_cast<const volatile u32*>(part_mesh + 0x28u) : nullptr;
        const bool trace_player_skin_packet =
            std::getenv("OPENRAC_TRACE_SLY_SKIN_PACKET_PLAYER") != nullptr &&
            native_trace_player_tree_owner(owner_address);
        if (std::getenv("OPENRAC_TRACE_SLY_SKIN_PACKET") != nullptr &&
            bone_indices != nullptr &&
            (std::getenv("OPENRAC_TRACE_SLY_SKIN_PACKET_PLAYER") == nullptr ||
             trace_player_skin_packet)) {
            static u32 skin_packet_notices = 0;
            if (skin_packet_notices++ < 96u) {
                const u32 slots_address = *reinterpret_cast<const volatile u32*>(part_mesh + 0x1Cu);
                const volatile u16* slots = slots_address >= 0x00010000u &&
                        static_cast<u64>(slots_address) + 16u <= 0x02000000ull
                    ? reinterpret_cast<const volatile u16*>(slots_address) : nullptr;
                const std::int32_t skin_word_count = skin_address >= 0x00010000u &&
                        static_cast<u64>(skin_address) + 0x48u <= 0x02000000ull
                    ? *reinterpret_cast<const volatile std::int32_t*>(skin_address + 0x40u) : 0;
                const u32 skin_words_address = skin_address >= 0x00010000u &&
                        static_cast<u64>(skin_address) + 0x48u <= 0x02000000ull
                    ? *reinterpret_cast<const volatile u32*>(skin_address + 0x44u) : 0u;
                std::fprintf(stderr,
                             "[native sly skin packet] request=%#010x owner=%#010x source=%#010x kind=%u mesh=%u bones=%u bone_array=%#010x indices=",
                             request_address, owner_address, source_mesh, kind, i,
                             bone_count, bone_array_address);
                for (u32 joint = 0; joint < bone_count; ++joint)
                    std::fprintf(stderr, "%s%u", joint == 0u ? "" : ",", bone_indices[joint]);
                std::fprintf(stderr,
                             " slots=%#010x bone_slot=%u skin_slot=%u skin_words=%d@%#010x weight_count=%u weights=%zu key_indices=",
                             slots_address, slots != nullptr ? slots[2] : 0xFFFFu,
                             slots != nullptr ? slots[3] : 0xFFFFu,
                             skin_word_count, skin_words_address,
                             mesh.vertex_weight_count, mesh.vertex_weights.size());
                for (std::size_t key = 0; key < mesh.morph_keys.size(); ++key)
                    std::fprintf(stderr, "%s%u", key == 0u ? "" : ",",
                                 mesh.morph_keys[key]);
                std::fprintf(stderr, " skin_data=");
                if (skin_word_count > 0 && skin_word_count <= 256 &&
                    skin_words_address >= 0x00010000u &&
                    static_cast<u64>(skin_words_address) +
                            static_cast<u64>(skin_word_count) * sizeof(u32) <= 0x02000000ull) {
                    const volatile u32* words = reinterpret_cast<const volatile u32*>(skin_words_address);
                    const u32 word_count = static_cast<u32>(std::min(skin_word_count, 16));
                    for (u32 word = 0; word < word_count; ++word)
                        std::fprintf(stderr, "%s%08x", word == 0u ? "" : ",", words[word]);
                    if (word_count < static_cast<u32>(skin_word_count)) std::fprintf(stderr, ",...");
                }
                std::fprintf(stderr, " weights=");
                const std::size_t weight_sample = std::min<std::size_t>(
                mesh.vertex_weights.size(), 12u);
                for (std::size_t weight = 0; weight < weight_sample; ++weight)
                    std::fprintf(stderr, "%s%g", weight == 0u ? "" : ",",
                                 mesh.vertex_weights[weight]);
                std::fputc('\n', stderr);
            }
        }
        if (trace_this_owner && bone_indices != nullptr &&
            attachment_count != 0u && attachment_items >= 0x00010000u &&
            attachment_items < 0x02000000u) {
            for (u32 joint = 0; joint < bone_count; ++joint) {
                const u32 bone_index = bone_indices[joint];
                if (bone_index >= attachment_count || bone_index >= traced_bones.size() ||
                    traced_bones[bone_index])
                    continue;
                traced_bones[bone_index] = true;
                const u64 entry_address = static_cast<u64>(attachment_items) +
                    static_cast<u64>(bone_index) * 0x90u;
                if (entry_address < 0x00010000ull ||
                    entry_address + 0x90u > 0x02000000ull) {
                    std::fprintf(stderr,
                                 "[native sly attachment] index=%u entry=%#010llx outside EE RAM\n",
                                 bone_index,
                                 static_cast<unsigned long long>(entry_address));
                    continue;
                }
                const u32 entry = static_cast<u32>(entry_address);
                const u32 target = *reinterpret_cast<const volatile u32*>(entry);
                const u64 key_address = static_cast<u64>(attachment_keys) +
                    static_cast<u64>(bone_index) * 4u;
                const u32 key = key_address >= 0x00010000ull &&
                        key_address + sizeof(u32) <= 0x02000000ull
                    ? *reinterpret_cast<const volatile u32*>(
                          static_cast<u32>(key_address))
                    : 0u;
                std::fprintf(stderr,
                             "[native sly attachment] index=%u key=%u entry=%#010x target=%#010x\n",
                             bone_index, key, entry, target);
                for (u32 matrix_kind = 0; matrix_kind < 2u; ++matrix_kind) {
                    const char* matrix_name = matrix_kind == 0u ? "local" : "final";
                    const u32 matrix_address = entry + (matrix_kind == 0u ? 0x10u : 0x50u);
                    const volatile float* values =
                        reinterpret_cast<const volatile float*>(matrix_address);
                    float row_length[3]{};
                    float row_dot[3]{};
                    float matrix_max = 0.0f;
                    bool matrix_finite = true;
                    for (u32 row = 0; row < 3u; ++row) {
                        for (u32 column = 0; column < 3u; ++column) {
                            const float value = values[row * 4u + column];
                            matrix_finite = matrix_finite && std::isfinite(value);
                            matrix_max = std::max(matrix_max, std::fabs(value));
                            row_length[row] += value * value;
                        }
                        row_length[row] = std::sqrt(row_length[row]);
                    }
                    for (u32 row = 0; row < 3u; ++row) {
                        const u32 other = (row + 1u) % 3u;
                        for (u32 column = 0; column < 3u; ++column)
                            row_dot[row] += values[row * 4u + column] *
                                            values[other * 4u + column];
                    }
                    const float determinant =
                        values[0] * (values[5] * values[10] - values[6] * values[9]) -
                        values[1] * (values[4] * values[10] - values[6] * values[8]) +
                        values[2] * (values[4] * values[9] - values[5] * values[8]);
                    std::fprintf(stderr, "[native sly attachment %s] index=%u m=",
                                 matrix_name, bone_index);
                    for (u32 element = 0; element < 16u; ++element)
                        std::fprintf(stderr, "%s%g", element == 0u ? "" : ",",
                                     static_cast<double>(values[element]));
                    std::fprintf(stderr,
                                 " basis_finite=%u row_len=%g,%g,%g row_dot=%g,%g,%g det=%g max=%g\n",
                                 matrix_finite ? 1u : 0u,
                                 row_length[0], row_length[1], row_length[2],
                                 row_dot[0], row_dot[1], row_dot[2],
                                 determinant, matrix_max);
                }
            }
        }
        const float* submitted_vertices = mesh.vertices.data();
        std::vector<float> host_morphed_vertices;
        if (std::getenv("OPENRAC_SLY_WORLD_MORPH") != nullptr &&
            mesh.morph_target_count > 0u && mesh.morph_deltas.size() ==
                static_cast<std::size_t>(mesh.morph_target_count) *
                    (mesh.vertices.size() / 9u) * 3u &&
            owner_address >= 0x00010000u &&
            static_cast<u64>(owner_address) + 0x274u <= 0x02000000ull) {
            const volatile std::int32_t* value_count_ptr =
                reinterpret_cast<const volatile std::int32_t*>(owner_address + 0x26Cu);
            const volatile u32* values_ptr =
                reinterpret_cast<const volatile u32*>(owner_address + 0x270u);
            const std::int32_t value_count = *value_count_ptr;
            const u32 values_address = *values_ptr;
            if (value_count >= static_cast<std::int32_t>(mesh.morph_target_count) &&
                value_count <= 256 && values_address >= 0x00010000u &&
                static_cast<u64>(values_address) +
                    static_cast<u64>(mesh.morph_target_count) * sizeof(float) <=
                    0x02000000ull) {
                const volatile float* channel_weights =
                    reinterpret_cast<const volatile float*>(values_address);
                host_morphed_vertices = mesh.vertices;
                bool valid_morph = true;
                const std::size_t vertex_count = mesh.vertices.size() / 9u;
                for (u32 channel = 0; channel < mesh.morph_target_count; ++channel) {
                    const float weight = channel_weights[channel];
                    if (!std::isfinite(weight)) {
                        valid_morph = false;
                        break;
                    }
                    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
                        const std::size_t delta_base =
                            (static_cast<std::size_t>(channel) * vertex_count + vertex) * 3u;
                        for (u32 axis = 0; axis < 3u; ++axis) {
                            float& position = host_morphed_vertices[vertex * 9u + axis];
                            position += mesh.morph_deltas[delta_base + axis] * weight;
                            if (!std::isfinite(position)) valid_morph = false;
                        }
                        if (!valid_morph) break;
                    }
                    if (!valid_morph) break;
                }
                if (valid_morph) {
                    submitted_vertices = host_morphed_vertices.data();
                    static u32 morph_notices = 0;
                    if (morph_notices++ < 12u)
                        std::fprintf(stderr,
                                     "[native sly host morph] request=%#010x owner=%#010x source=%#010x mesh=%u vertices=%zu channels=%u weights=",
                                     request_address, owner_address, source_mesh, i,
                                     vertex_count, mesh.morph_target_count);
                    if (morph_notices <= 12u) {
                        for (u32 channel = 0; channel < mesh.morph_target_count; ++channel)
                            std::fprintf(stderr, "%s%g", channel == 0u ? "" : ",",
                                         static_cast<double>(channel_weights[channel]));
                        std::fputc('\n', stderr);
                    }
                }
            }
        }
        std::vector<float> host_skinned_vertices;
        if (std::getenv("OPENRAC_SLY_WORLD_SKIN") != nullptr &&
            bone_indices != nullptr && bone_count > 0u && bone_count <= 4u &&
            mesh.vertex_weight_count > 0u && mesh.vertex_weight_count <= 4u &&
            mesh.vertex_weights.size() ==
                (mesh.vertices.size() / 9u) * mesh.vertex_weight_count &&
            attachment_count != 0u && bone_array_address >= 0x00010000u &&
            bone_array_address < 0x02000000u) {
            const u32 influence_count = std::min(bone_count, mesh.vertex_weight_count);
            host_skinned_vertices.assign(submitted_vertices,
                                         submitted_vertices + mesh.vertices.size());
            bool all_vertices_skinned = true;
            for (std::size_t vertex = 0; vertex < mesh.vertices.size() / 9u; ++vertex) {
                const float* position = submitted_vertices + vertex * 9u;
                float result[3]{};
                float total_weight = 0.0f;
                for (u32 influence = 0; influence < influence_count; ++influence) {
                    const float weight = mesh.vertex_weights[
                        vertex * mesh.vertex_weight_count + influence];
                    const u32 palette_index = bone_indices[influence];
                    if (!std::isfinite(weight) || weight < 0.0f ||
                        palette_index >= attachment_count) {
                        all_vertices_skinned = false;
                        break;
                    }
                    const u64 matrix_address = static_cast<u64>(bone_array_address) +
                        static_cast<u64>(palette_index) * 0x90u + 0x50u;
                    if (matrix_address < 0x00010000ull ||
                        matrix_address + 16u * sizeof(float) > 0x02000000ull) {
                        all_vertices_skinned = false;
                        break;
                    }
                    const volatile float* matrix =
                        reinterpret_cast<const volatile float*>(
                            static_cast<u32>(matrix_address));
                    float transformed[3]{};
                    for (u32 axis = 0; axis < 3u; ++axis) {
                        transformed[axis] = position[0] * matrix[axis] +
                            position[1] * matrix[4u + axis] +
                            position[2] * matrix[8u + axis] + matrix[12u + axis];
                    }
                    if (!std::isfinite(transformed[0]) ||
                        !std::isfinite(transformed[1]) ||
                        !std::isfinite(transformed[2])) {
                        all_vertices_skinned = false;
                        break;
                    }
                    for (u32 axis = 0; axis < 3u; ++axis)
                        result[axis] += transformed[axis] * weight;
                    total_weight += weight;
                }
                if (!all_vertices_skinned || !std::isfinite(total_weight) ||
                    total_weight <= 0.0f) {
                    all_vertices_skinned = false;
                    break;
                }
                for (u32 axis = 0; axis < 3u; ++axis)
                    host_skinned_vertices[vertex * 9u + axis] = result[axis];
            }
            if (all_vertices_skinned) {
                submitted_vertices = host_skinned_vertices.data();
                static u32 skin_notices = 0;
                if (skin_notices++ < 12u)
                    std::fprintf(stderr,
                                 "[native sly host skin] request=%#010x owner=%#010x source=%#010x mesh=%u vertices=%zu bones=%u weights=%u first=%g,%g,%g->%g,%g,%g\n",
                                 request_address, owner_address, source_mesh, i,
                                 mesh.vertices.size() / 9u, bone_count,
                                 mesh.vertex_weight_count,
                                 static_cast<double>(mesh.vertices[0]),
                                 static_cast<double>(mesh.vertices[1]),
                                 static_cast<double>(mesh.vertices[2]),
                                 static_cast<double>(host_skinned_vertices[0]),
                                 static_cast<double>(host_skinned_vertices[1]),
                                 static_cast<double>(host_skinned_vertices[2]));
            }
        }
        for (std::size_t pass = 0; pass < mesh.materials.size(); ++pass) {
            const NativeSlyMaterial& material = mesh.materials[pass];
            const u64 instance = is_render_target
                ? ((((static_cast<u64>(owner_address) << 32u) ^
                     (static_cast<u64>(part_address) * 0x9E3779B185EBCA87ull) ^
                     (static_cast<u64>(i) << 8u)) & ~0xFFull) | pass)
                : ((static_cast<u64>(request_address) << 16u) |
                   (static_cast<u64>(i) << 8u) | pass);
            if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr && notices++ < 32u)
                std::fprintf(stderr,
                             "[native sly mesh submit] source=%#010x kind=%u packet=%#010x request=%#010x mesh=%u pass=%zu/%zu vertices=%zu tex0=%#llx alpha=%s%#llx\n",
                             source_mesh, mesh.kind, packet_address, request_address, i, pass,
                             mesh.materials.size(), mesh.vertices.size() / 9u,
                             static_cast<unsigned long long>(material.tex0),
                             material.alpha_valid ? "" : "missing/",
                             static_cast<unsigned long long>(material.alpha));
            if (getenv("OPENRAC_NO_WORLD_MESH_QUEUE") != nullptr) continue;
            openrac_native_sly_world_mesh_submit(
                world, instance, submitted_vertices,
                static_cast<u32>(mesh.vertices.size() / 9u), request_address, source_mesh,
                target_tbp, static_cast<int>(target_width), static_cast<int>(target_height),
                is_render_target ? 1 : 0,
                mesh.vertex_indices.size() == mesh.vertices.size() / 9u
                    ? mesh.vertex_indices.data() : nullptr,
                model, view_projection, material.tex0, material.tex1,
                material.clamp, material.alpha, material.alpha_valid ? 1 : 0,
                material.test);
        }
    }
}

extern "C" void sly1_native_trace_model_lighting(
    u32 request_address, u32 mode, u32 strongest_mode, u32 flagged_count,
    u32 selected_count, u32 lights_address, u32 part_address) {
    if (std::getenv("OPENRAC_TRACE_NATIVE_LIGHTS") == nullptr) return;
    static u32 notices = 0;
    if (notices++ >= 48u || request_address == 0 || part_address == 0) return;
    const volatile u32* part = reinterpret_cast<const volatile u32*>(part_address);
    std::fprintf(stderr,
                 "[native model lights] request=%#010x part=%#010x mode=%u strongest=%u flagged=%u selected=%u lit=%u skinned=%u list=%#010x\n",
                 request_address, part_address, mode, strongest_mode,
                 flagged_count, selected_count, part[0x18u / 4u],
                 part[0x28u / 4u], lights_address);
    if (mode == 0u || mode == 0xFFFFFFFFu || selected_count == 0u ||
        selected_count > 128u || lights_address == 0u)
        return;
    const volatile u32* lights = reinterpret_cast<const volatile u32*>(lights_address);
    const u32 bounded_count = std::min(selected_count, 8u);
    for (u32 i = 0; i < bounded_count; ++i) {
        const u32 address = lights[i];
        if (!native_guest_range_readable(address, 0x388u)) continue;
        const volatile float* direction = reinterpret_cast<const volatile float*>(address + 0x140u);
        const volatile float* volume = reinterpret_cast<const volatile float*>(address + 0x370u);
        const u32 kind = *reinterpret_cast<const volatile u32*>(address + 0x2D0u);
        const float intensity = *reinterpret_cast<const volatile float*>(address + 0x334u);
        std::fprintf(stderr,
                     "[native model light] request=%#010x index=%u address=%#010x kind=%u dir=%g,%g,%g intensity=%g volume0=%g,%g,%g,%g\n",
                     request_address, i, address, kind,
                     static_cast<double>(direction[0]), static_cast<double>(direction[1]),
                     static_cast<double>(direction[2]), static_cast<double>(intensity),
                     static_cast<double>(volume[0]), static_cast<double>(volume[1]),
                     static_cast<double>(volume[2]), static_cast<double>(volume[3]));
    }
}

extern "C" void sly1_native_trace_model_part_packet_range(
    u32 request, u32 part, u32 begin, u32 end, u32 meshes, u32 strips,
    u32 flags, u32 lit, u32 skinned) {
    const char* setting = std::getenv("OPENRAC_TRACE_MODEL_PART_PACKET_ADDRESS");
    if (setting == nullptr || request == 0 || part == 0) return;
    char* parse_end = nullptr;
    const unsigned long parsed = std::strtoul(setting, &parse_end, 0);
    if (parse_end == setting || *parse_end != '\0' || parsed > 0xFFFFFFFFul) return;
    const u32 target = static_cast<u32>(parsed);
    const u32 range_bytes = end - begin;
    const u32 target_offset = target - begin;
    if (range_bytes == 0 || range_bytes > 0x01000000u ||
        target_offset >= range_bytes)
        return;
    std::fprintf(stderr,
                 "[native model part packet range] request=%#010x part=%#010x range=[%#010x,%#010x) bytes=%u target=%#010x target_offset=%#x meshes=%u strips=%u flags=%#x lit=%u skinned=%u\n",
                 request, part, begin, end, range_bytes, target, target_offset,
                 meshes, strips, flags, lit, skinned);
}

extern "C" void sly1_native_trace_model_part_item_packet_range(
    u32 request, u32 part, u32 begin, u32 end, u32 index, u32 kind,
    u32 count, u32 flags, u32) {
    const char* setting = std::getenv("OPENRAC_TRACE_MODEL_PART_PACKET_ADDRESS");
    if (setting == nullptr || request == 0 || part == 0) return;
    char* parse_end = nullptr;
    const unsigned long parsed = std::strtoul(setting, &parse_end, 0);
    if (parse_end == setting || *parse_end != '\0' || parsed > 0xFFFFFFFFul) return;
    const u32 target = static_cast<u32>(parsed);
    const u32 range_bytes = end - begin;
    const u32 target_offset = target - begin;
    if (range_bytes == 0 || range_bytes > 0x01000000u ||
        target_offset >= range_bytes)
        return;
    std::fprintf(stderr,
                 "[native model part item packet range] request=%#010x part=%#010x kind=%s index=%u/%u range=[%#010x,%#010x) bytes=%u target=%#010x target_offset=%#x flags=%#x\n",
                 request, part, kind == 0 ? "mesh" : "strip", index, count,
                 begin, end, range_bytes, target, target_offset, flags);
}

extern "C" void sly1_native_trace_model_build(u32 mesh_address, u32 vecs_address,
                                                 u32 uvs_address, u32 vertex_count,
                                                 u32 refs_address, u32 header_address) {
    if (!g_startup_trace_enabled || mesh_address == 0 ||
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_BUILDS") == nullptr)
        return;
    const volatile u8* mesh = reinterpret_cast<const volatile u8*>(mesh_address);
    const u32 kind = mesh[0];
    if (kind != 6u || vertex_count == 0 || vertex_count > 4096u) return;

    const u32 strip_count = *reinterpret_cast<const volatile u32*>(mesh_address + 0x18u);
    const u32 sources_address = *reinterpret_cast<const volatile u32*>(mesh_address + 0x1Cu);
    const u32 header_count = header_address != 0
        ? *reinterpret_cast<const volatile u32*>(header_address + 0x2Cu) : 0;
    static u32 sequence = 0;
    const u32 sample = __atomic_fetch_add(&sequence, 1u, __ATOMIC_RELAXED);
    if (sample >= 12u) return;

    const volatile u8* refs = reinterpret_cast<const volatile u8*>(refs_address);
    u32 max_position = 0;
    u32 max_uv = 0;
    bool has_uv = false;
    for (u32 i = 0; i < vertex_count; ++i) {
        if (refs[i * 4u] > max_position) max_position = refs[i * 4u];
        if (refs[i * 4u + 2u] != 0xFFu) {
            has_uv = true;
            if (refs[i * 4u + 2u] > max_uv) max_uv = refs[i * 4u + 2u];
        }
    }
    std::fprintf(stderr,
                 "[native model build] sample=%u kind=6 vertices=%u strips=%u strip_header_count=%u mesh=%#010x vecs=%#010x uvs=%#010x refs=%#010x max_position=%u max_uv=%u\n",
                 sample, vertex_count, strip_count, header_count, mesh_address,
                 vecs_address, uvs_address, refs_address, max_position, has_uv ? max_uv : 0u);

    char path[128];
    auto dump = [&](const char* suffix, const void* data, u32 bytes) {
        std::snprintf(path, sizeof(path), "/tmp/sly-kind6-build-%u-%s.bin", sample, suffix);
        FILE* file = std::fopen(path, "wb");
        if (file == nullptr) return;
        const std::size_t written = std::fwrite(data, 1, bytes, file);
        std::fclose(file);
        std::fprintf(stderr, "[native model build dump] sample=%u part=%s bytes=%u written=%zu path=%s\n",
                     sample, suffix, bytes, written, path);
    };
    dump("refs", reinterpret_cast<const void*>(refs_address), vertex_count * 4u);
    dump("positions", reinterpret_cast<const void*>(vecs_address), (max_position + 1u) * 12u);
    if (has_uv && uvs_address != 0)
        dump("uvs", reinterpret_cast<const void*>(uvs_address), (max_uv + 1u) * 8u);

    if (sources_address == 0 || strip_count > 8u) return;
    const volatile u32* sources = reinterpret_cast<const volatile u32*>(sources_address);
    for (u32 i = 0; i < strip_count; ++i) {
        const u32 stride = sources[i * 2u];
        const u32 base = sources[i * 2u + 1u];
        const u64 byte_count = static_cast<u64>(stride) * 16u;
        bool mapped = false;
        if (stride != 0 && stride <= 1024u && byte_count <= 16384u &&
            base != 0 && byte_count <= 0x100000000ull - base) {
            const long page_size = ::sysconf(_SC_PAGESIZE);
            if (page_size > 0) {
                const u32 mask = static_cast<u32>(page_size - 1);
                const u32 page_begin = base & ~mask;
                const u64 page_end = (static_cast<u64>(base) + byte_count + mask) & ~static_cast<u64>(mask);
                const u64 page_count = (page_end - page_begin) / static_cast<u32>(page_size);
                unsigned char resident[8]{};
                mapped = page_count > 0 && page_count <= 8 &&
                    ::mincore(reinterpret_cast<void*>(page_begin),
                              static_cast<std::size_t>(page_count) * page_size,
                              resident) == 0;
            }
        }
        std::fprintf(stderr,
                     "[native model strip] sample=%u strip=%u stride_qwc=%u base=%#010x mapped=%u\n",
                     sample, i, stride, base, mapped ? 1u : 0u);
        if (mapped) {
            dump(("strip-" + std::to_string(i)).c_str(),
                 reinterpret_cast<const void*>(base), static_cast<u32>(byte_count));
        }
    }
}

extern "C" void sly1_native_trace_world_draw_lists(u32 world_address,
                                                      u32 list60_head,
                                                      u32 list78_head,
                                                      u32 list60_offset,
                                                      u32 list78_offset,
                                                      u32 current_world) {
    const bool trace_all_worlds = std::getenv("OPENRAC_TRACE_ALL_WORLD_DRAW") != nullptr;
    if ((!g_startup_trace_enabled && !trace_all_worlds) || world_address == 0 ||
        world_address != current_world ||
        (!trace_all_worlds && world_address != __atomic_load_n(
            &g_native_trace_target_world, __ATOMIC_RELAXED)))
        return;

    static u32 sample_count = 0;
    const u32 index = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (index < 8u || (index & 63u) == 0) {
        std::fprintf(stderr,
                     "[native world draw lists] sample=%u world=%#010x list60=%#010x offset=%u list78=%#010x offset=%u\n",
                     index, world_address, list60_head, list60_offset,
                     list78_head, list78_offset);
    }
    if (index >= 2u) return;
    const u32 heads[2] = {list60_head, list78_head};
    const u32 offsets[2] = {list60_offset, list78_offset};
    const char* names[2] = {"list60", "list78"};
    for (u32 list_index = 0; list_index < 2u; ++list_index) {
        u32 node_address = heads[list_index];
        for (u32 node_index = 0; node_index < 6u && node_address != 0; ++node_index) {
            const volatile u32* node = reinterpret_cast<const volatile u32*>(node_address);
            const u32 descriptor_address = node[0];
            const volatile u32* descriptor =
                reinterpret_cast<const volatile u32*>(descriptor_address);
            const u32 callback = descriptor[40];
            const u32 method24 = descriptor[0xA0u / 4u];
            const u32 method25 = descriptor[0xA4u / 4u];
            const u32 method26 = descriptor[0xA8u / 4u];
            const u32 next = *reinterpret_cast<const volatile u32*>(node_address + offsets[list_index]);
            std::fprintf(stderr,
                         "[native world draw node] list=%s node=%u address=%#010x descriptor=%#010x class=%u id=%u world=%#010x draw40=%#010x m24=%#010x m25=%#010x m26=%#010x next=%#010x\n",
                         names[list_index], node_index, node_address,
                         descriptor_address, descriptor[1], node[2], node[5],
                         callback, method24, method25, method26, next);
            node_address = next;
        }
    }
}

extern "C" void sly1_native_trace_deferred_requests(u32 world_address,
                                                       u32 queued_count,
                                                       u32 sorted_count,
                                                       u32 sorted_list,
                                                       u32 model_draw_callback) {
    const bool trace_all_worlds = std::getenv("OPENRAC_TRACE_ALL_WORLD_DRAW") != nullptr;
    if ((!g_startup_trace_enabled && !trace_all_worlds) || world_address == 0 ||
        sorted_list == 0 || (!trace_all_worlds && world_address != __atomic_load_n(
            &g_native_trace_target_world, __ATOMIC_RELAXED)))
        return;

    static u32 sample_count = 0;
    const u32 sample = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (!(sample < 4u || (sample & 63u) == 0)) return;
    std::fprintf(stderr,
                 "[native deferred requests] sample=%u world=%#010x queued=%u sorted=%u",
                 sample, world_address, queued_count, sorted_count);
    const volatile u32* entries = reinterpret_cast<const volatile u32*>(sorted_list);
    const u32 limit = sorted_count < 12u ? sorted_count : 12u;
    for (u32 i = 0; i < limit; ++i) {
        const u32 request_address = entries[i];
        if (request_address == 0) continue;
        const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
        std::fprintf(stderr, " [%u]=%#010x,arg=%#010x,group=%u",
                     i, request[0], request[0x60 / 4], request[0x08 / 4]);
        if (request[0] == model_draw_callback) {
            const u32 object_address = request[0x60 / 4];
            if (object_address >= 0x01000000u && object_address < 0x02000000u) {
                const volatile u32* object = reinterpret_cast<const volatile u32*>(object_address);
                std::fprintf(stderr,
                             ",model_parts=%u,parts=%#010x,range=%#010x,packet_sizes=%u/%u",
                             object[0x238 / 4], object[0x23C / 4], object[0x2D0 / 4],
                             object[0x2D8 / 4], object[0x2E0 / 4]);
            }
        }
    }
    std::fputc('\n', stderr);
}

extern "C" void sly1_native_trace_draw_object(u32 world_address,
                                                u32 object_address,
                                                u32 part_count,
                                                u32 parts_address) {
    const bool trace_all_worlds = std::getenv("OPENRAC_TRACE_ALL_WORLD_DRAW") != nullptr;
    if ((!g_startup_trace_enabled && !trace_all_worlds) || world_address == 0 ||
        object_address == 0 || (!trace_all_worlds && world_address != __atomic_load_n(
            &g_native_trace_target_world, __ATOMIC_RELAXED)))
        return;
    static u32 sample_count = 0;
    const u32 sample = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (sample >= 16u) return;
    std::fprintf(stderr,
                 "[native draw object] sample=%u world=%#010x object=%#010x parts=%u array=%#010x",
                 sample, world_address, object_address, part_count, parts_address);
    if (parts_address != 0 && part_count > 0) {
        const volatile u32* part = reinterpret_cast<const volatile u32*>(parts_address);
        std::fprintf(stderr, " first_pos=%#x,%#x,%#x,%#x radius=%#x kind=%u matrix=%#010x",
                     part[0], part[1], part[2], part[3], part[4], part[5], part[0x68 / 4]);
    }
    std::fputc('\n', stderr);
}

extern "C" void sly1_native_trace_detached_mesh_packet(u32 mesh, u32 kind,
                                                          u32 packet,
                                                          u32 qwc,
                                                          u32 start,
                                                          u32 destination) {
    if (std::getenv("OPENRAC_TRACE_VIF_WRITES") == nullptr ||
        (destination < 0x018d0000u || destination >= 0x018e0000u))
        return;
    static std::atomic<u32> notice_count{0};
    if (notice_count.fetch_add(1, std::memory_order_relaxed) >= 32u) return;
    std::fprintf(stderr,
                 "[native detached mesh packet] mesh=%#010x kind=%u packet=%u qwc=%u start=%#010x destination=%#010x\n",
                 mesh, kind, packet, qwc, start, destination);
}

extern "C" void sly1_native_trace_strip_reserve(u32 mesh, u32 stride,
                                                    u32 offset,
                                                    u32 writer) {
    if (std::getenv("OPENRAC_TRACE_VIF_WRITES") == nullptr ||
        (stride != 7u && offset != 0x1D4u))
        return;
    static std::atomic<u32> target_notice_count{0};
    static std::atomic<u32> sample_notice_count{0};
    const u32 notice = offset == 0x1D4u
        ? target_notice_count.fetch_add(1, std::memory_order_relaxed)
        : sample_notice_count.fetch_add(1, std::memory_order_relaxed);
    if (notice >= 64u) return;
    std::fprintf(stderr,
                 "[native strip reserve] mesh=%#010x stride=%u vu=%#x writer=%#010x\n",
                 mesh, stride, offset, writer);
}

#else
extern "C" void sly1_native_trace_model_part(u32, u32, u32) {}
extern "C" void sly1_native_trace_placed_object_parts(u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_detached_mesh_packet(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_strip_reserve(u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_placed_object_load(u32, u32, u32) {}
extern "C" void sly1_native_trace_placed_object_draw(u32, u32, u32) {}
extern "C" void sly1_native_trace_pose_rotation(u32, u32, u32, u32, u32, float, float, float) {}
extern "C" void sly1_native_trace_model_hook(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_queue_model_parts(u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_queue_model_result(u32, u32, u32, u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_model_cull(u32, u32, u32, u32, float, float, float, float, float, u32) {}
extern "C" void sly1_native_trace_spr_block(u32, u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_model_build(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_capture_model_mesh(u32, u32, u32, u32, u32, u32, u32, u32,
                                                u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_set_morph_vertex_count(u32) {}
extern "C" void sly1_native_bind_model_mesh(u32, u32) {}
extern "C" void sly1_native_submit_model_part(u32, u32, u32) {}
extern "C" void sly1_native_trace_model_lighting(u32, u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_model_part_packet_range(u32, u32, u32, u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_model_part_item_packet_range(u32, u32, u32, u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_note_world(u32) {}
extern "C" void sly1_native_trace_world_draw_lists(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_deferred_requests(u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_draw_object(u32, u32, u32, u32) {}
#endif

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_semaphore_event(const char* event, int identifier, int count,
                                  unsigned int caller) {
    if (!g_startup_trace_enabled ||
        !__atomic_load_n(&g_native_semaphore_trace_active, __ATOMIC_RELAXED))
        return;
    const u32 index = __atomic_fetch_add(&g_semaphore_trace_event_count, 1u,
                                         __ATOMIC_RELAXED);
    static std::atomic<u32> frame_ready_events{0};
    const bool render_frame_semaphore = identifier == 4 || identifier == 5;
    const u32 frame_event = render_frame_semaphore
        ? frame_ready_events.fetch_add(1u, std::memory_order_relaxed) : 0u;
    const bool trace_frame_done = identifier == 5 &&
        std::getenv("OPENRAC_TRACE_FRAME_DONE_SEMA") != nullptr;
    if (index >= 64 && !trace_frame_done &&
        (!render_frame_semaphore || (frame_event >= 16u && frame_event % 256u != 0u)))
        return;
    std::uint64_t frame_done_gap_ms = 0;
    if (trace_frame_done && event[0] == 's') {
        static std::atomic<std::uint64_t> previous_signal_ms{0};
        const std::uint64_t now = SDL_GetTicks();
        const std::uint64_t previous = previous_signal_ms.exchange(
            now, std::memory_order_relaxed);
        if (previous != 0) frame_done_gap_ms = now - previous;
    }
    std::fprintf(stderr,
                 "[native semaphore] event=%u %s id=%d count=%d caller=%08x frame_done_gap_ms=%llu\n",
                 index, event, identifier, count, caller,
                 static_cast<unsigned long long>(frame_done_gap_ms));
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_null_ramp(void* ramp, void* out, float x, float y, float z,
                             u32 caller) {
    if (!g_startup_trace_enabled) return;
    union FloatBits { float value; u32 bits; };
    write_hex("native host32: null ramp caller ", (int)caller);
    write_hex("native host32: null ramp profile ", (int)ramp);
    write_hex("native host32: null ramp output ", (int)out);
    write_hex("native host32: null ramp x bits ", (int)FloatBits{x}.bits);
    write_hex("native host32: null ramp y bits ", (int)FloatBits{y}.bits);
    write_hex("native host32: null ramp z bits ", (int)FloatBits{z}.bits);
}

void native_pc_sample_handler(int, siginfo_t*, void* context) {
    const auto* state = static_cast<const ucontext_t*>(context);
#if defined(__i386__)
    g_native_pc_sample_pc = state->uc_mcontext.gregs[REG_EIP];
    g_native_pc_sample_sp = state->uc_mcontext.gregs[REG_ESP];
    g_native_pc_sample_bp = state->uc_mcontext.gregs[REG_EBP];
#elif defined(__x86_64__)
    g_native_pc_sample_pc = state->uc_mcontext.gregs[REG_RIP];
    g_native_pc_sample_sp = state->uc_mcontext.gregs[REG_RSP];
    g_native_pc_sample_bp = state->uc_mcontext.gregs[REG_RBP];
#endif
    const u32 sp = (u32)g_native_pc_sample_sp;
    if (sp >= 0xC0000000u && sp < 0xF0000000u) {
        const volatile u32* stack = reinterpret_cast<const volatile u32*>(sp);
        for (u32 i = 0; i < 8; ++i)
            g_native_pc_sample_stack[i] = (sig_atomic_t)stack[i];
    }
    ++g_native_pc_sample_sequence;
}

void install_pc_sampler() {
    struct sigaction action{};
    action.sa_sigaction = native_pc_sample_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR2, &action, nullptr);
}

void trace_game_thread_pc(pthread_t thread, unsigned sample) {
    const int signal_result = pthread_kill(thread, SIGUSR2);
    SDL_Delay(1);
    const u32 pc = (u32)g_native_pc_sample_pc;
    const u32 sp = (u32)g_native_pc_sample_sp;
    const u32 bp = (u32)g_native_pc_sample_bp;
    u32 retail_function = 0;
    u32 native_function = 0;
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address != 0 && entry.native_address <= pc &&
            entry.native_address > native_function) {
            native_function = entry.native_address;
            retail_function = entry.retail_address;
        }
    }
    const u32 offset = pc >= native_function ? pc - native_function : 0;
    std::fprintf(stderr,
                 "[native game thread] sample=%u signal=%d sequence=%d pc=%#010x sp=%#010x bp=%#010x stack=%#010x,%#010x,%#010x,%#010x,%#010x,%#010x,%#010x,%#010x preceding_retail_mapping=%#010x native_mapping=%#010x offset_from_mapping=%#x\n",
                 sample, signal_result, (int)g_native_pc_sample_sequence,
                 pc, sp, bp,
                 (u32)g_native_pc_sample_stack[0],
                 (u32)g_native_pc_sample_stack[1],
                 (u32)g_native_pc_sample_stack[2],
                 (u32)g_native_pc_sample_stack[3],
                 (u32)g_native_pc_sample_stack[4],
                 (u32)g_native_pc_sample_stack[5],
                 (u32)g_native_pc_sample_stack[6],
                 (u32)g_native_pc_sample_stack[7],
                 retail_function, native_function, offset);
}

volatile u32 g_native_frame_stage = 0;
volatile u32 g_native_frame_count = 0;
volatile u32 g_native_game_thread_tid = 0;
volatile u32 g_native_world = 0;
volatile u32 g_native_ui_state = 0;
volatile u32 g_native_ui_stack_count = 0;
volatile u32 g_native_ui_resume_state = 0;
volatile u32 g_native_world_load_request = 0;
volatile u32 g_native_movie_queued = 0;
volatile u32 g_native_active_ui = 0;
volatile u32 g_native_text_ui = 0;
volatile u32 g_native_pad_status = 0;
volatile u32 g_native_pad_mode = 0;
volatile u32 g_native_pad_flags = 0;

volatile u32 g_native_pause_open = 0;
volatile u32 g_native_pause_page = 0;
volatile u32 g_native_pause_mode = 0;
volatile u32 g_native_fade_mode = 0;
volatile u32 g_native_stack_top = 0;
volatile u32 g_native_stack_top_vt = 0;
volatile u32 g_native_stack_top_mode = 0;
volatile u32 g_native_stack_top_text = 0;
volatile u32 g_native_pad_routing = 0;
volatile u32 g_native_pad_select = 0;
volatile u32 g_native_blank_pad_status = 0;
volatile u32 g_native_card_state = 0;
volatile u32 g_native_card_target = 0;
volatile u32 g_native_card_display = 0;
volatile u32 g_native_card_progress = 0;
volatile u32 g_native_memory_gate = 0;
volatile u32 g_native_element_state = 0;
volatile u32 g_native_card_thread = 0;
volatile u32 g_native_card_job = 0;
volatile u32 g_native_card_result = 0;
volatile u32 g_native_card_semaphore = 0;
volatile u32 g_native_elapsed_bits = 0;
volatile u32 g_native_card_state_time_bits = 0;
volatile u32 g_native_card_display_time_bits = 0;

bool probe_disc_signature(u32 offset) {
    u8 signature[6]{};
    const int bytes = sly1_i386_syscall6(180, g_disc_fd, (int)signature,
                                         sizeof(signature), (int)offset, 0, 0);
    return bytes == (int)sizeof(signature) && signature[0] == 1 &&
           signature[1] == 'C' && signature[2] == 'D' &&
           signature[3] == '0' && signature[4] == '0' && signature[5] == '1';
}

bool open_disc_image(const char* path) {
    constexpr int kOpenLargeFile = 0x8000;
    g_disc_fd = syscall3(5, (int)path, kOpenLargeFile, 0);
    if (g_disc_fd < 0) { write_text("native host32: ISO open failed\n"); return false; }
    u64 end = 0;
    const int seek_result = sly1_i386_syscall6(140, g_disc_fd, 0, 0,
                                               (int)&end, 2, 0);
    if (seek_result != 0 || end == 0) { write_text("native host32: ISO size query failed\n"); return false; }

    constexpr u32 kPvdSector = 16;
    if (probe_disc_signature(kPvdSector * 2048)) {
        g_disc_stride = 2048;
        g_disc_payload = 0;
    } else if (probe_disc_signature(kPvdSector * 2352 + 16)) {
        g_disc_stride = 2352;
        g_disc_payload = 16;
    } else {
        write_text("native host32: ISO PVD probe failed\n");
        return false;
    }
    g_disc_sector_count = (u32)divide_u64_u32(end, g_disc_stride);
    return g_disc_sector_count != 0;
}

bool load_retail_image(const char* path) {
    const int fd = syscall3(5, (int)path, 0, 0);
    if (fd < 0) { write_text("native host32: cannot open ELF\n"); return false; }
    const int length = syscall3(19, fd, 0, 2);
    if (length < 52) { write_text("native host32: invalid ELF size\n"); syscall1(6, fd); return false; }
    const int image = sly1_i386_syscall6(192, 0, length, kProtRead, kMapPrivate, fd, 0);
    syscall1(6, fd);
    if (syscall_failed(image)) { write_hex("native host32: mmap2 failed ", image); return false; }
    const auto* bytes = (const u8*)image;
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 1 || bytes[5] != 1 || read16(bytes + 18) != 8) {
        write_text("native host32: expected 32-bit little-endian MIPS ELF\n");
        return false;
    }
    const u32 phoff = read32(bytes + 28);
    const u16 phentsize = read16(bytes + 42), phnum = read16(bytes + 44);
    if (phentsize < 32 || (u64)phoff + (u64)phentsize * phnum > (u32)length) {
        write_text("native host32: invalid ELF program headers\n");
        return false;
    }
    bool mapped_any = false;
    const u32 guest_ram_base = 0x00010000u;
    const u32 guest_ram_size = 0x01ff0000u;
    const int guest_ram = sly1_i386_syscall6(
        192, (int)guest_ram_base, (int)guest_ram_size, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(guest_ram) || (u32)guest_ram != guest_ram_base) {
        write_text("native host32: retail EE RAM address range unavailable\n");
        return false;
    }
    const int scratchpad = sly1_i386_syscall6(
        192, 0x70000000, 0x4000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(scratchpad) || (u32)scratchpad != 0x70000000u) {
        write_text("native host32: scratchpad address range unavailable\n");
        return false;
    }
    // Back the GS privileged register aperture with native shadow RAM. Sly's
    // matching display setup writes PMODE/SMODE2/DISPFB/DISPLAY/BGCOLOR here;
    // native GIF packets are consumed by the SDL renderer separately.
    const int gs_registers = sly1_i386_syscall6(
        192, 0x12000000, 0x2000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(gs_registers) || (u32)gs_registers != 0x12000000u) {
        write_text("native host32: GS register shadow address unavailable\n");
        return false;
    }
    *(volatile u32*)0x12001000u = 0;
    // Back the EE I/O register aperture used by the reusable native VIF1
    // runtime and by Sly's direct VIF0 status accesses.
    const int ee_mmio = sly1_i386_syscall6(
        192, 0x10000000, 0x10000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(ee_mmio) || (u32)ee_mmio != 0x10000000u) {
        write_text("native host32: EE I/O register aperture unavailable\n");
        return false;
    }
    // VU1 data and micro memory occupy separate windows within this aperture.
    const int vu1_memory = sly1_i386_syscall6(
        192, 0x11000000, 0x10000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(vu1_memory) || (u32)vu1_memory != 0x11000000u) {
        write_text("native host32: VU1 memory aperture unavailable\n");
        return false;
    }
    *(volatile u32*)0x10003010u = 0;
    for (u16 i = 0; i < phnum; ++i) {
        const u8* p = bytes + phoff + (u32)i * phentsize;
        if (read32(p) != 1) continue;
        const u32 offset = read32(p + 4), address = read32(p + 8);
        const u32 file_size = read32(p + 16), memory_size = read32(p + 20);
        if (file_size > memory_size || (u64)offset + file_size > (u32)length ||
            (address & (kPage-1)) != (offset & (kPage-1))) {
            write_text("native host32: invalid ELF load segment\n");
            return false;
        }
        if (memory_size == 0) continue;
        // PS2 executable images commonly carry the ELF header as a writable
        // PT_LOAD at virtual address zero. It is file metadata, not EE RAM;
        // the native process deliberately leaves the null page unmapped.
        if (address == 0 && file_size == 0x1000 && memory_size == 0x1000) continue;
        if (address < guest_ram_base || address >= guest_ram_base + guest_ram_size ||
            memory_size > guest_ram_base + guest_ram_size - address) {
            write_text("native host32: ELF segment lies outside mapped EE RAM\n");
            return false;
        }
        for (u32 n = 0; n < file_size; ++n) ((u8*)address)[n] = bytes[offset+n];
        for (u32 n = file_size; n < memory_size; ++n) ((u8*)address)[n] = 0;
        mapped_any = true;
    }
    if (!mapped_any) { write_text("native host32: ELF has no load segments\n"); return false; }

    const u32 shoff = read32(bytes + 32);
    const u16 shentsize = read16(bytes + 46), shnum = read16(bytes + 48);
    if (shnum == 0 || shentsize < 40 || (u64)shoff + (u64)shentsize * shnum > (u32)length) {
        write_text("native host32: ELF has no usable section table\n");
        return false;
    }
    for (u16 i = 0; i < shnum; ++i) {
        const u8* s = bytes + shoff + (u32)i * shentsize;
        const u32 type = read32(s + 4), flags = read32(s + 8);
        const u32 address = read32(s + 12), offset = read32(s + 16), size = read32(s + 20);
        if (!(flags & 2) || (flags & 4) || type != 1 || size == 0) continue;
        if (address == 0) continue; // .elf_header is not part of the running game image.
        if ((u64)offset + size > (u32)length) {
            write_text("native host32: data section outside ELF\n");
            return false;
        }
        u32* words = (u32*)address;
        for (u32 j = 0; j < size / 4; ++j) {
            u32 first = 0, last = sly1::sly1_native_function_address_count;
            while (first < last) {
                const u32 middle = first + (last - first) / 2;
                if (sly1::sly1_native_function_addresses[middle].retail_address < words[j]) first = middle + 1;
                else last = middle;
            }
            if (first < sly1::sly1_native_function_address_count &&
                sly1::sly1_native_function_addresses[first].retail_address == words[j] &&
                sly1::sly1_native_function_addresses[first].native_address != 0) {
                words[j] = sly1::sly1_native_function_addresses[first].native_address;
            }
        }
    }
    syscall2(91, image, (int)length);
    return true;
}

} // namespace

extern "C" void sly1_native_prepare_thread_fault_trace() {
    if (g_startup_trace_enabled) {
        install_fault_trace();
        write_hex("native host32: worker fault trace ready ", 1);
    }
}

extern "C" u32 sly1_native_resolve_function(u32 address) {
    return resolve_native(address);
}

extern "C" int sly1_native_host32_main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        write_text("usage: sly1_host32 <Sly 1 NTSC-U ELF> <ISO or BIN> [--trace]\n");
        return 2;
    }
    g_startup_trace_enabled = argc == 4 && argv[3][0] == '-' && argv[3][1] == '-' &&
                              argv[3][2] == 't';
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (g_startup_trace_enabled) install_fault_trace();
#endif
    if (!load_retail_image(argv[1])) return 1;
    if (!open_disc_image(argv[2])) {
        write_text("native host32: cannot open ISO 9660 disc image\n");
        return 1;
    }
#if defined(OPENRAC_SLY1_WITH_SDL)
    make_game_constants_writable();
#endif
    write_text("native host32: retail data and ISO mapped; starting compiled Sly frame path\n");
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (!openrac_native_window_create()) {
        write_text("native host32: SDL3 renderer startup failed\n");
        return 1;
    }
    const bool trace_graphics = [] {
        const char* value = std::getenv("OPENRAC_STARTUP_TRACE");
        return value != nullptr && value[0] != '0';
    }();
    if (trace_graphics) install_pc_sampler();
    std::atomic<bool> game_finished{false};
    std::thread game_thread([&] {
        g_native_game_thread_tid = (u32)syscall3(224, 0, 0, 0);
        if (g_startup_trace_enabled) install_fault_trace();
        func_00185758((int)*(volatile u32*)0x0027B380u, (int)0x0027B384u);
        game_finished.store(true, std::memory_order_release);
    });
    using NativeInterruptHandler = int (*)(int);
    const auto handle_vblank = reinterpret_cast<NativeInterruptHandler>(
        resolve_native(0x001682C0u));
    if (handle_vblank == nullptr) {
        write_text("native host32: retail VBlank handler is not linked\n");
        game_thread.detach();
        return 1;
    }
    unsigned trace_frames = 0;
    unsigned semaphore_trace_after_frame = 0;
    if (trace_graphics) {
        const char* trace_semaphores_from_start =
            std::getenv("OPENRAC_TRACE_SEMAPHORES_FROM_START");
        const char* delay = std::getenv("OPENRAC_TEST_PRESS_SPACE_AFTER_FRAMES");
        if (trace_semaphores_from_start != nullptr &&
            trace_semaphores_from_start[0] != '0') {
            g_native_semaphore_trace_active = 1;
        } else if (delay != nullptr && delay[0] != '\0' && delay[0] != '0') {
            char* end = nullptr;
            const unsigned long after = std::strtoul(delay, &end, 10);
            if (end != delay && *end == '\0')
                semaphore_trace_after_frame = static_cast<unsigned>(after);
        }
        if (semaphore_trace_after_frame == 0)
            g_native_semaphore_trace_active = 1;
    }
    openrac::platform::FramePacer presenter(
        openrac::platform::field_rate(openrac::platform::VideoStandard::Ntsc));
    for (;;) {
        openrac_native_window_present();
        if (trace_graphics) ++trace_frames;
        // Keep the simulated display interrupt at the game's NTSC field rate;
        // the window is deliberately not tied to the monitor's refresh rate.
        const int elapsed_fields = presenter.wait();
        for (int field = 0; field < elapsed_fields; ++field) {
            // GS CSR.FIELD is the only status bit read by the Sly display path.
            __atomic_fetch_xor((u32*)0x12001000u, 1u << 13, __ATOMIC_RELAXED);
            // Wake the retail VBlank path once for each elapsed display field.
            handle_vblank(2);
        }
        const int quit = openrac_native_window_poll_input();
        if (trace_graphics &&
            trace_frames >= semaphore_trace_after_frame)
            g_native_semaphore_trace_active = 1;
        if (quit) {
            break;
        }
        if (trace_graphics && (trace_frames % 60u) == 0) {
            trace_game_thread_pc(game_thread.native_handle(), trace_frames / 60u);
            std::fprintf(stderr, "[native graphics] dma=%llu accepted=%llu rejected=%llu bytes=%llu\n",
                         openrac_native_renderer_dma_submit_count(),
                         openrac_native_renderer_dma_accept_count(),
                         openrac_native_renderer_dma_reject_count(),
                         openrac_native_renderer_dma_bytes());
            std::fprintf(stderr,
                         "[native startup] entry_returned=%u callback=%u iop_stage=%u cd_reads=%u sectors=%u game_frames=%u game_tid=%u frame_stage=%u world=%#x ui_state=%u ui_stack=%u resume=%d load=%u movie=%u visible_ui=%u text_ui=%u pad_status=%u pad_mode=%u pad_flags=%#x pad_route=%u pad_select=%u blank_pad_status=%u pause_open=%u pause_page=%d pause_mode=%u fade=%u stack_top=%#x top_vt=%#x top_mode=%u top_text=%u card_state=%d card_target=%d card_display=%u card_progress=%d memory_gate=%u element_state=%u card_thread=%u card_job=%#x card_result=%d card_semaphore=%u game_elapsed_bits=%#x card_state_time_bits=%#x card_display_time_bits=%#x\n",
                         game_finished.load(std::memory_order_acquire) ? 1u : 0u,
                         sly1_native_startup_callback_index,
                         sly1_native_iop_boot_stage,
                         g_disc_read_count, g_disc_sectors_read,
                         __atomic_load_n(&g_native_frame_count, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_game_thread_tid, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_frame_stage, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_world, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_ui_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_ui_stack_count, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_ui_resume_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_world_load_request, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_movie_queued, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_active_ui, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_text_ui, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_status, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_flags, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_routing, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_select, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_blank_pad_status, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pause_open, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_pause_page, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pause_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_fade_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_vt, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_text, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_state, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_target, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_display, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_progress, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_memory_gate, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_element_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_thread, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_job, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_result, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_semaphore, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_elapsed_bits, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_state_time_bits, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_display_time_bits, __ATOMIC_RELAXED));
            std::fprintf(stderr, "[native critical] owner=%d depth=%u spins=%u\n",
                         sly1_native_critical_owner(), sly1_native_critical_depth(),
                         sly1_native_critical_spins());
        }
        SDL_Delay(16);
    }
    game_thread.detach();
    openrac_native_window_destroy();
    // Native guest worker services may still be running. Exit the process as
    // a unit rather than racing their use of C++ globals during CRT teardown.
    syscall1(1, 0);
    __builtin_unreachable();
#else
    func_00185758((int)*(volatile u32*)0x0027B380u,
                  (int)0x0027B384u);
    return 0;
#endif
}

extern "C" int sly1_host_cd_read(u32 sector, u32 count, u32 destination) {
    ++g_disc_read_count;
    g_disc_sectors_read += count;
    if (g_disc_fd < 0 || sector > g_disc_sector_count ||
        count > g_disc_sector_count - sector) {
        g_disc_read_error = -1;
        return 0;
    }
    for (u32 i = 0; i < count; ++i) {
        const u64 offset = (u64)(sector + i) * g_disc_stride + g_disc_payload;
        const int bytes = sly1_i386_syscall6(180, g_disc_fd,
                                             (int)(destination + i * 2048), 2048,
                                             (int)offset, (int)(offset >> 32), 0);
        if (bytes != 2048) {
            g_disc_read_error = bytes < 0 ? bytes : -1;
            return 0;
        }
    }
    g_disc_read_error = 0;
    return 1;
}

#if defined(OPENRAC_SLY1_WITH_SDL)
namespace {
bool read_exact_fd(int fd, u8* output, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        const ssize_t count = ::read(fd, output + done, size - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        done += static_cast<std::size_t>(count);
    }
    return true;
}

bool write_all_fd(int fd, const u8* input, std::size_t size) {
    std::size_t done = 0;
    while (done < size) {
        const ssize_t count = ::write(fd, input + done, size - done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        done += static_cast<std::size_t>(count);
    }
    return true;
}

bool wait_child_success(pid_t child) {
    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool probe_movie(const char* path, int& width, int& height,
                 int& fps_num, int& fps_den) {
    int pipe_fds[2];
    if (::pipe(pipe_fds) != 0) return false;
    const pid_t child = ::fork();
    if (child == 0) {
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        char* argv[] = {
            const_cast<char*>("ffprobe"), const_cast<char*>("-v"),
            const_cast<char*>("error"), const_cast<char*>("-select_streams"),
            const_cast<char*>("v:0"), const_cast<char*>("-show_entries"),
            const_cast<char*>("stream=width,height,r_frame_rate"),
            const_cast<char*>("-of"), const_cast<char*>("csv=p=0"),
            const_cast<char*>(path), nullptr};
        ::execvp(argv[0], argv);
        ::_exit(127);
    }
    ::close(pipe_fds[1]);
    if (child < 0) {
        ::close(pipe_fds[0]);
        return false;
    }
    char output[128]{};
    std::size_t used = 0;
    while (used + 1u < sizeof(output)) {
        const ssize_t count = ::read(pipe_fds[0], output + used,
                                     sizeof(output) - used - 1u);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += static_cast<std::size_t>(count);
    }
    ::close(pipe_fds[0]);
    if (!wait_child_success(child)) return false;
    return std::sscanf(output, "%d,%d,%d/%d", &width, &height,
                       &fps_num, &fps_den) == 4 && width > 0 && height > 0 &&
           fps_num > 0 && fps_den > 0;
}
} // namespace

extern "C" int sly1_native_play_movie(u32 lsn, u32 bytes, u32 resource_id) {
    if (std::getenv("OPENRAC_NATIVE_FFMPEG_MOVIES") == nullptr ||
        resource_id != 0x344u || bytes == 0u || bytes > 128u * 1024u * 1024u)
        return 0;
    const u32 sectors = (bytes + 2047u) / 2048u;
    std::vector<u8> disc_data(static_cast<std::size_t>(sectors) * 2048u);
    if (g_disc_stride == 0 ||
        !sly1_host_cd_read(lsn, sectors, static_cast<u32>(
            reinterpret_cast<std::uintptr_t>(disc_data.data())))) {
        std::fprintf(stderr, "[native movie ffmpeg] disc read failed lsn=%#x bytes=%#x\n",
                     lsn, bytes);
        return 0;
    }

    char path[] = "/tmp/open-thievius-movie-XXXXXX";
    const int movie_fd = ::mkstemp(path);
    if (movie_fd < 0) return 0;
    const bool stored = write_all_fd(movie_fd, disc_data.data(), bytes);
    ::close(movie_fd);
    if (!stored) {
        ::unlink(path);
        return 0;
    }

    int width = 0, height = 0, fps_num = 0, fps_den = 0;
    if (!probe_movie(path, width, height, fps_num, fps_den) ||
        width > 4096 || height > 4096 ||
        static_cast<std::uint64_t>(width) * height * 4u > 64u * 1024u * 1024u) {
        std::fprintf(stderr, "[native movie ffmpeg] unsupported PSS lsn=%#x\n", lsn);
        ::unlink(path);
        return 0;
    }

    int pipe_fds[2];
    if (::pipe(pipe_fds) != 0) {
        ::unlink(path);
        return 0;
    }
    const pid_t decoder = ::fork();
    if (decoder == 0) {
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        char* argv[] = {
            const_cast<char*>("ffmpeg"), const_cast<char*>("-nostdin"),
            const_cast<char*>("-v"), const_cast<char*>("error"),
            const_cast<char*>("-i"), path, const_cast<char*>("-map"),
            const_cast<char*>("0:v:0"), const_cast<char*>("-an"),
            const_cast<char*>("-sn"), const_cast<char*>("-dn"),
            const_cast<char*>("-fps_mode"), const_cast<char*>("passthrough"),
            const_cast<char*>("-f"), const_cast<char*>("rawvideo"),
            const_cast<char*>("-pix_fmt"), const_cast<char*>("rgba"),
            const_cast<char*>("pipe:1"), nullptr};
        ::execvp(argv[0], argv);
        ::_exit(127);
    }
    ::close(pipe_fds[1]);
    if (decoder < 0) {
        ::close(pipe_fds[0]);
        ::unlink(path);
        return 0;
    }

    if (std::getenv("OPENRAC_TRACE_MOVIE_BACKFILL") != nullptr) {
        std::fprintf(stderr,
                     "[native movie ffmpeg] play id=%#x lsn=%#x bytes=%#x "
                     "size=%dx%d fps=%d/%d\n",
                     resource_id, lsn, bytes, width, height, fps_num, fps_den);
    }
    const std::size_t frame_bytes = static_cast<std::size_t>(width) * height * 4u;
    std::vector<u8> frame(frame_bytes);
    const auto frame_step = std::chrono::microseconds(
        static_cast<std::int64_t>(1000000ll * fps_den / fps_num));
    auto next_frame = std::chrono::steady_clock::now();
    u32 frames = 0;
    while (read_exact_fd(pipe_fds[0], frame.data(), frame.size())) {
        openrac_native_movie_frame(frame.data(), width, height);
        ++frames;
        next_frame += frame_step;
        std::this_thread::sleep_until(next_frame);
    }
    ::close(pipe_fds[0]);
    openrac_native_movie_end();
    const bool decoded = wait_child_success(decoder);
    ::unlink(path);
    if (std::getenv("OPENRAC_TRACE_MOVIE_BACKFILL") != nullptr) {
        std::fprintf(stderr, "[native movie ffmpeg] finished frames=%u decoder_ok=%u\n",
                     frames, decoded ? 1u : 0u);
    }
    return decoded && frames != 0u ? 1 : 0;
}
#endif

extern "C" int sly1_native_decomp_movies_enabled() {
    const char* setting = std::getenv("OPENRAC_NATIVE_DECOMP_MOVIES");
    return setting != nullptr && setting[0] != '\0' && setting[0] != '0';
}

extern "C" __attribute__((noreturn)) void sly1_missing_function(
    u32 address, u32 caller, u32 argument0, u32 argument1, u32 argument2) {
    write_hex("native host32: missing compiled function at ", (int)address);
    write_hex("native host32: called from ", (int)caller);
    write_hex("native host32: first argument ", (int)argument0);
    write_hex("native host32: second argument ", (int)argument1);
    write_hex("native host32: third argument ", (int)argument2);
    if (address == 0x00162890u && argument1 >= 0x10000000u &&
        argument1 < 0xF0000000u) {
        if (argument0 >= 0x00100000u && argument0 < 0x02100000u) {
            const auto* block = reinterpret_cast<volatile const u32*>(argument0);
            write_hex("native host32: GLOBSET list count ", (int)block[0]);
            write_hex("native host32: GLOBSET record count ", (int)block[3]);
            write_hex("native host32: GLOBSET pose count ", (int)block[16]);
            if (block[3] != 0 && block[4] >= 0x00100000u &&
                block[4] < 0x02100000u) {
                for (u32 i = 0; i < block[3] && i < 6; ++i) {
                    const auto* record = reinterpret_cast<volatile const u32*>(
                        block[4] + i * 0x70u);
                    write_hex("native host32: GLOB flags ", (int)record[26]);
                    write_hex("native host32: GLOB nested count ", (int)record[22]);
                    if (record[22] != 0 && record[23] >= 0x00100000u &&
                        record[23] < 0x02100000u) {
                        const auto* nested = reinterpret_cast<volatile const u32*>(record[23]);
                        for (u32 j = 0; j < record[22] && j < 3; ++j) {
                            const auto* entry = nested + j * 20;
                            write_hex("native host32: nested type ", (int)entry[5]);
                            write_hex("native host32: nested reference ", (int)entry[6]);
                            write_hex("native host32: nested reference count ", (int)entry[9]);
                        }
                    }
                }
            }
        }
        const auto* reader = reinterpret_cast<volatile const u32*>(argument1);
        const u32 cursor = reader[6];
        write_hex("native host32: reader flags ", (int)reader[0]);
        write_hex("native host32: reader mode ", (int)reader[1]);
        write_hex("native host32: reader end ", (int)reader[2]);
        write_hex("native host32: reader start ", (int)reader[3]);
        write_hex("native host32: reader cursor ", (int)cursor);
        write_hex("native host32: reader remaining ", (int)reader[7]);
        write_hex("native host32: reader next ", (int)reader[8]);
        write_hex("native host32: reader next count ", (int)reader[9]);
        write_hex("native host32: reader f28 ", (int)reader[10]);
        write_hex("native host32: reader f2c ", (int)reader[11]);
        if (cursor >= 0x10000000u && cursor < 0xF0000000u) {
            const auto* bytes = reinterpret_cast<volatile const u8*>(cursor);
            for (u32 i = 0; i < 16; ++i)
                write_hex("native host32: reader byte ", bytes[i]);
        }
    }
    if (address == 0x001841D0u && argument0 >= 0x00100000u &&
        argument0 < 0x02100000u) {
        const auto* camera = reinterpret_cast<volatile const u32*>(argument0);
        static const u32 offsets[] = {
            0x2D0u, 0x2D4u, 0x2E0u, 0x2E4u, 0x2E8u, 0x2ECu,
            0x2F0u, 0x2F4u, 0x2F8u, 0x2FCu, 0x300u, 0x304u,
            0x320u, 0x324u, 0x328u, 0x32Cu};
        for (u32 i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
            const u32 offset = offsets[i];
            write_hex("native host32: C51+offset ", (int)offset);
            write_hex("native host32: C51 value ", (int)camera[offset / 4]);
        }
    }
    syscall1(1, 127);
    __builtin_unreachable();
}

extern "C" void sly1_port_trace_reader(u32 stage, void* reader_ptr) {
    if (!g_startup_trace_enabled) return;
    const auto* reader = reinterpret_cast<volatile const u32*>(reader_ptr);
    write_hex("native host32: GLOBSET reader stage ", (int)stage);
    write_hex("native host32: GLOBSET reader cursor ", (int)reader[6]);
    write_hex("native host32: GLOBSET reader remaining ", (int)reader[7]);
    if (stage == 0xb4) {
        const auto* bytes = reinterpret_cast<volatile const u8*>(reader[6]);
        for (u32 i = 0; i < 16; ++i)
            write_hex("native host32: scene next byte ", bytes[i]);
    }
}

extern "C" void sly1_native_trace_loader_stage(u32 stage, u32 value,
                                                   u32 reader_ptr) {
    if (!g_startup_trace_enabled || reader_ptr == 0) return;
    // Keep the high volume object field reads out of the log. These points
    // bracket the scene record parsers and identify their decoded values.
    if (stage != 3 && stage != 4 && (stage < 21 || stage > 28) &&
        (stage < 29 || stage > 33)) return;
    const auto* reader = reinterpret_cast<volatile const u32*>(reader_ptr);
    write_hex("native host32: loader stage ", (int)stage);
    write_hex("native host32: loader value ", (int)value);
    write_hex("native host32: loader cursor ", (int)reader[6]);
    write_hex("native host32: loader remaining ", (int)reader[7]);
    if (stage == 3 || (stage >= 29 && stage <= 33) ||
        stage == 21 || stage == 22 || stage == 23 || stage == 24) {
        const auto* bytes = reinterpret_cast<volatile const u8*>(reader[6]);
        write_hex("native host32: loader next halfword ",
                  (int)((u32)bytes[0] | ((u32)bytes[1] << 8)));
    }
}

extern "C" void sly1_native_trace_movie_call(
    u32 self, u32 key, u32 key_lsn, u32 key_bytes,
    u32 key_word_2, u32 key_word_6,
    u32 queued, u32 queued_next) {
    static u32 events = 0;
    if (getenv("OPENRAC_TRACE_MOVIE_BACKFILL") == nullptr || events >= 32u)
        return;
    std::fprintf(stderr,
                 "[native movie backfill] call=%u self=%08x key=%08x "
                 "lsn=%08x bytes=%08x id=%08x/%08x queued=%08x next=%08x "
                 "iop_disc=%u iop_read_pending=%u iop_packet_busy=%u "
                 "host_cd_reads=%u host_cd_sectors=%u\n",
                 events++, self, key, key_lsn, key_bytes, key_word_2,
                 key_word_6, queued, queued_next,
                 *reinterpret_cast<volatile u32*>(0x0025C34Cu),
                 *reinterpret_cast<volatile u32*>(0x0025C354u),
                 *reinterpret_cast<volatile u32*>(0x00260DC0u),
                 g_disc_read_count, g_disc_sectors_read);
}

extern "C" void sly1_native_trace_cd_read(u32 sector, u32 count,
                                            u32 mode, u32 result) {
    static u32 events = 0;
    if (getenv("OPENRAC_TRACE_MOVIE_BACKFILL") == nullptr || events >= 64u)
        return;
    std::fprintf(stderr,
                 "[native cd read] event=%u lsn=%08x sectors=%u mode=%06x "
                 "accepted=%u\n",
                 events++, sector, count, mode, result);
}

extern "C" void sly1_port_trace_nested_counts(
    u32 count0, u32 count1, u32 count2, u32 count3, u32 count4) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: nested count 0 ", (int)count0);
    write_hex("native host32: nested count 1 ", (int)count1);
    write_hex("native host32: nested count 2 ", (int)count2);
    write_hex("native host32: nested count 3 ", (int)count3);
    write_hex("native host32: nested count 4 ", (int)count4);
}

extern "C" void sly1_port_trace_value(u32 tag, u32 value) {
    if (!g_startup_trace_enabled && (tag & 0xFFFF0000u) != 0x1F280000u)
        return;
    write_hex("native host32: trace tag ", (int)tag);
    write_hex("native host32: trace value ", (int)value);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_oscillator_probe(u32 tag, u32 value) {
    write_hex("native host probe tag ", (int)tag);
    write_hex("native host probe value ", (int)value);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_script_host_call(u32 function, u32 self, u32 count) {
    if (!g_startup_trace_enabled || self < 0x00100000u || self >= 0x12000000u)
        return;
    const auto* object = reinterpret_cast<volatile const u32*>(self);
    const u32 descriptor = object[0];
    if (descriptor < 0x00100000u || descriptor >= 0x12000000u) return;
    const auto* head = reinterpret_cast<volatile const u32*>(descriptor);
    if (head[1] != 0x57u) return;
    write_hex("native host32: notifier script host function ", (int)function);
    write_hex("native host32: notifier script host self ", (int)self);
    write_hex("native host32: notifier script host argument count ", (int)count);
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address == function) {
            write_hex("native host32: notifier script host retail function ",
                      (int)entry.retail_address);
            break;
        }
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_script_builtin(u32 function, u32 count, u32 args, u32 scope) {
    if (!g_startup_trace_enabled) return;
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address != function) continue;
        if (entry.retail_address != 0x0012c7d8u &&
            entry.retail_address != 0x00101a90u) return;
        write_hex("native host32: animation script builtin target ",
                  (int)entry.retail_address);
        write_hex("native host32: animation script builtin count ", (int)count);
        write_hex("native host32: animation script builtin args ", (int)args);
        write_hex("native host32: animation script builtin scope ", (int)scope);
        return;
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_startup(u32 index, u32 phase, u32 callback) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: startup callback index ", (int)index);
    write_hex("native host32: startup callback phase ", (int)phase);
    if (callback != 0)
        write_hex("native host32: startup callback address ", (int)callback);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_thread_service(u32 service, u32 result, u32 a0, u32 a1) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: thread service ", (int)service);
    write_hex("native host32: thread result ", (int)result);
    write_hex("native host32: thread arg0 ", (int)a0);
    write_hex("native host32: thread arg1 ", (int)a1);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_resource_state(u32 self, u32 pending, u32 world) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: resource self ", (int)self);
    write_hex("native host32: resource pending ", (int)pending);
    write_hex("native host32: current world ", (int)world);
    if (self >= 0x08048000u && self < 0x09000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(self);
        for (u32 i = 0; i < 13; ++i)
            write_hex("native host32: resource field ", (int)words[i]);
    }
    if (pending >= 0x08048000u && pending < 0x09000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(pending);
        for (u32 i = 0; i < 8; ++i)
            write_hex("native host32: pending blob word ", (int)words[i]);
    }
    if (world >= 0x08048000u && world < 0x09000000u)
        write_hex("native host32: current world state ",
                  (int)*(volatile const u32*)(world + 0x1D7Cu));
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_group_child(u32 type, u32 object, u32 descriptor, u32 read_method) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: group child type ", (int)type);
    write_hex("native host32: group child object ", (int)object);
    write_hex("native host32: group child descriptor ", (int)descriptor);
    write_hex("native host32: group child read method ", (int)read_method);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_path_curve(u32 kind, u32 curve, u32 vtable, u32 read_method) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: path curve kind ", (int)kind);
    write_hex("native host32: path curve object ", (int)curve);
    write_hex("native host32: path curve vtable ", (int)vtable);
    write_hex("native host32: path curve reader ", (int)read_method);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_capped_list(u32 stage, u32 count, u32 list, u32 stream) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: capped-list stage ", (int)stage);
    write_hex("native host32: capped-list count ", (int)count);
    write_hex("native host32: capped-list object ", (int)list);
    write_hex("native host32: capped-list stream ", (int)stream);
    if (stream >= 0x00100000u && stream < 0x12000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(stream);
        write_hex("native host32: capped-list cursor ", (int)words[6]);
        write_hex("native host32: capped-list remaining ", (int)words[7]);
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_c02_state(u32 tag, u32 self) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: C02 trace point ", (int)tag);
    write_hex("native host32: C02 object ", (int)self);
    if (self < 0x00010000u || self >= 0x12000000u) return;
    const auto* words = reinterpret_cast<volatile const u32*>(self);
    write_hex("native host32: C02 descriptor ", (int)words[0]);
    write_hex("native host32: C02 parent ", (int)words[6]);
    write_hex("native host32: C02 world ", (int)words[5]);
    write_hex("native host32: C02 root ", (int)words[20]);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_c02_list(u32 tag, u32 list, u32 item, u32 result) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: C02 list trace point ", (int)tag);
    write_hex("native host32: C02 list address ", (int)list);
    write_hex("native host32: C02 list result ", (int)result);
    if (list < 0x00010000u || list >= 0x12000000u ||
        item < 0x00010000u || item >= 0x12000000u) return;
    const auto* fields = reinterpret_cast<volatile const u32*>(list);
    const u32 offset = fields[2];
    write_hex("native host32: C02 list head ", (int)fields[0]);
    write_hex("native host32: C02 list tail ", (int)fields[1]);
    write_hex("native host32: C02 list offset ", (int)offset);
    if (offset < 0x100000u) {
        const auto* link = reinterpret_cast<volatile const u32*>(item + offset);
        write_hex("native host32: C02 item next ", (int)link[0]);
        write_hex("native host32: C02 item prev ", (int)link[1]);
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_sorter(u32 stage, u32 world, u32 axis, u32 value) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: sorter stage ", (int)stage);
    write_hex("native host32: sorter world ", (int)world);
    write_hex("native host32: sorter axis ", (int)axis);
    write_hex("native host32: sorter value ", (int)value);
}

// Opt-in trace of CreateTypedRecord calls during resource loading. Logs the
// class index, object id and world to diagnose why the Splash notifier (class
// index 0x57) is never instantiated.
static volatile u32 g_first_notifier = 0;
extern "C" u32 first_notifier_object() { return g_first_notifier; }
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_create_typed_record(u32 index, u32 id, u32 world, u32 script_index) {
    if (getenv("OPENRAC_TRACE_CREATE_RECORD") == nullptr) return;
    static volatile u32 notices = 0;
    const u32 n = __atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED);
    if (n < 4096u)
        write_hex("native host32: create record ", (int)((index << 16) | (id & 0xFFFFu)));
    if (index == 0x88u && script_index != 0xFFFFFFFFu)
        write_hex("  graph script index ", (int)script_index);
    if (index == 0x57u) {
        write_hex("  notifier object ", (int)id);
        if (__atomic_load_n(&g_first_notifier, __ATOMIC_RELAXED) == 0u)
            __atomic_store_n(&g_first_notifier, id, __ATOMIC_RELAXED);
    }
}

extern "C" u32 first_notifier_object();
// Retail CreateStateWalker(StateGraph*, void*, int, int).
extern "C" void* retail_create_state_walker(void*, void*, int, int)
    asm("_Z13func_001B6290P10StateGraphPvii");

// Opt-in drive of the Splash countdown notifier through its OWN retail state
// setter (descriptor method m2F at +0xCC), called from RunMainLoop on the game
// thread so the call has the right context. This reproduces the retail
// state-2 -> state-6 / event-0x263 transition the graph needs, without forcing
// any field.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_drive_splash_notifier() {
    if (getenv("OPENRAC_DRIVE_SPLASH_NOTIFIER") == nullptr) return;
    static volatile u32 driven = 0;
    if (__atomic_load_n(&driven, __ATOMIC_RELAXED) != 0u) return;
    const u32 obj = first_notifier_object();
    if (obj < 0x00010000u || obj >= 0x02000000u) return;
    const u32 graph = *reinterpret_cast<volatile u32*>(obj + 0x2E4u);
    const int flag_index = *reinterpret_cast<volatile int*>(obj + 0x37Cu);
    u32 graph_state_count = 0xFFFFFFFFu;
    u32 graph_edge_count = 0xFFFFFFFFu;
    u32 graph_armed = 0xFFFFFFFFu;
    if (graph >= 0x00010000u && graph < 0x02000000u) {
        graph_state_count = *reinterpret_cast<volatile u32*>(graph + 0x34u);
        graph_edge_count = *reinterpret_cast<volatile u32*>(graph + 0x3Cu);
        graph_armed = *reinterpret_cast<volatile u32*>(graph + 0x44u);
    }
    std::fprintf(stderr,
                 "[native splash notifier] object=%#010x state=%d initial=%d idle_event=%d flag_index=%d graph=%#010x graph_states=%u graph_edges=%u graph_armed=%u host=%#010x timer_disabled=%d timer_enabled=%#010x armed=%d time_min=%g time_max=%g time_left=%g\n",
                 obj,
                 *reinterpret_cast<volatile int*>(obj + 0x2D0u),
                 *reinterpret_cast<volatile int*>(obj + 0x2D8u),
                 *reinterpret_cast<volatile int*>(obj + 0x2DCu),
                 flag_index, graph, graph_state_count, graph_edge_count,
                 graph_armed,
                 *reinterpret_cast<volatile u32*>(obj + 0x2E8u),
                 *reinterpret_cast<volatile int*>(obj + 0x334u),
                 *reinterpret_cast<volatile u32*>(obj + 0x338u),
                 *reinterpret_cast<volatile int*>(obj + 0x340u),
                 static_cast<double>(*reinterpret_cast<volatile float*>(obj + 0x390u)),
                 static_cast<double>(*reinterpret_cast<volatile float*>(obj + 0x394u)),
                 static_cast<double>(*reinterpret_cast<volatile float*>(obj + 0x398u)));
    if (graph < 0x00010000u || graph >= 0x02000000u) return;
    __atomic_store_n(&driven, 1u, __ATOMIC_RELAXED);
    // Retail SetStateNotifierState(state 6 -> event 0x263) with host==0 creates
    // a StateWalker on the notifier's graph at event 0x263. Call that retail
    // function directly via its exact symbol and store it as the notifier's
    // host, so the graph has a walker targeting 0x263 instead of idling on
    // its self-edge at 0x261. No graph/field values are fabricated.
    write_hex("  graph scriptScope ", (int)*(volatile u32*)(graph + 0x2Cu));
    write_hex("  graph id ", (int)*(volatile u32*)(graph + 8u));
    void* walker = retail_create_state_walker((void*)graph, (void*)obj, 0x263, 0);
    if (walker != nullptr)
        *(volatile u32*)(obj + 0x2E8u) = (u32)(uintptr_t)walker; // host
}

// An explicit level boot for the SDL build (see compile_decomp_host32.py).
// Normal launches follow the retail Splash, memory-card, and title flow.
// OPENRAC_DEFAULT_LEVEL (or legacy OPENRAC_BOOT_LEVEL) opts into the proven
// QueueWorldLoad path; key 0x0100 is the catalog's "A Stealthy Approach".
// OPENRAC_NO_DEFAULT_LEVEL remains a compatibility switch for older launch
// commands that requested the retail Splash-only path.
extern "C" u32 sly1_native_boot_level_key_for_sdl(void) {
    if (getenv("OPENRAC_NO_DEFAULT_LEVEL") != nullptr) return 0;
    const char* value = getenv("OPENRAC_DEFAULT_LEVEL");
    if (value == nullptr) value = getenv("OPENRAC_BOOT_LEVEL");
    if (value == nullptr || *value == '\0') return 0u;
    char* end = nullptr;
    const unsigned long parsed = strtoul(value, &end, 0);
    return (end != value && *end == '\0' && parsed <= 0xFFFFFFFFul)
               ? (u32)parsed : 0x0100u;
}
extern "C" u32 sly1_keyed_entries_for_sdl(void) {
    extern u8 D_00247AF0[]; // g_keyedEntries (46 x 0x2C)
    return (u32)(uintptr_t)D_00247AF0;
}

// Opt-in trace of the authoritative bone matrix the VIF unpack sends.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_bone_unpack(u32 skin, u32 matrix_addr) {
    if (getenv("OPENRAC_TRACE_BONE_UNPACK") == nullptr) return;
    // Time-series mode: log bone idx 5's matrix max with a frame counter to
    // test whether it compounds over frames (a per-frame multiply bug).
    if (getenv("OPENRAC_BONE_TIMESERIES") != nullptr) {
        static volatile u32 ts = 0;
        if (matrix_addr != 0x01876c20u) return;  // bone idx 5
        const u32 n = __atomic_fetch_add(&ts, 1u, __ATOMIC_RELAXED);
        if (n % 20u != 0u || n > 400u) return;
        const volatile float* v = reinterpret_cast<const volatile float*>(matrix_addr);
        float mx = 0.0f;
        for (u32 i = 0; i < 16u; ++i) mx = std::max(mx, std::fabs(v[i]));
        std::fprintf(stderr, "[bone-series] n=%u max=%g\n", n, (double)mx);
        return;
    }
    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) >= 16u) return;
    if (matrix_addr < 0x00010000u || matrix_addr >= 0x02000000u) return;
    const volatile float* v = reinterpret_cast<const volatile float*>(matrix_addr);
    float mx = 0.0f;
    for (u32 i = 0; i < 16u; ++i) mx = std::max(mx, std::fabs(v[i]));
    // With DUMP_BONE_ALL, dump the full 16-float matrix of every unpack.
    if (getenv("OPENRAC_DUMP_BONE_ALL") != nullptr) {
        const int count = *reinterpret_cast<const volatile int*>(skin + 0x40u);
        const u32 bones = *reinterpret_cast<const volatile u32*>(skin + 0x04u);
        u32 index = bones != 0 && matrix_addr >= bones
            ? (matrix_addr - bones) / 0x90u : 0xFFFFFFFFu;
        std::fprintf(stderr, "[bone-unpack] skin=%#010x bones=%#010x count=%d idx=%u addr=%#010x max=%g m=",
                     skin, bones, count, index, matrix_addr, (double)mx);
        for (u32 i = 0; i < 16u; ++i)
            std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", (double)v[i]);
        std::fputc('\n', stderr);
    } else {
        std::fprintf(stderr, "[bone-unpack] skin=%#010x addr=%#010x max=%g\n",
                     skin, matrix_addr, (double)mx);
    }
}

// Opt-in trace of ComputePlacedObjectWorldFrame rotation inputs/output.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_world_frame(u32 self, u32 rot_addr, u32 m110_addr, u32 parent, u32 pos_addr) {
    if (getenv("OPENRAC_TRACE_WORLD_FRAME") == nullptr) return;
    static volatile u32 notices = 0;
    auto valid_matrix = [](u32 addr) -> bool {
        return addr >= 0x00010000u && addr <= 0x01FFFFC0u;
    };
    auto matrix_max = [&](u32 addr) -> float {
        if (!valid_matrix(addr)) return 0.0f;
        const volatile float* v = reinterpret_cast<const volatile float*>(addr);
        float m = 0.0f;
        for (u32 row = 0; row < 3u; ++row)
            for (u32 col = 0; col < 3u; ++col)
                m = std::max(m, std::fabs(v[row * 4u + col]));
        return m;
    };
    auto degenerate = [&](u32 addr) -> bool {
        if (!valid_matrix(addr)) return false;
        const volatile float* m = reinterpret_cast<const volatile float*>(addr);
        float norm[3] = {};
        for (u32 row = 0; row < 3u; ++row) {
            for (u32 col = 0; col < 3u; ++col)
                norm[row] += m[row * 4u + col] * m[row * 4u + col];
            norm[row] = std::sqrt(norm[row]);
            if (!std::isfinite(norm[row]) || norm[row] < 0.25f || norm[row] > 4.0f)
                return true;
        }
        for (u32 a = 0; a < 3u; ++a) {
            for (u32 b = a + 1u; b < 3u; ++b) {
                float dot = 0.0f;
                for (u32 col = 0; col < 3u; ++col)
                    dot += m[a * 4u + col] * m[b * 4u + col];
                if (!std::isfinite(dot) || std::fabs(dot) > norm[a] * norm[b] * 0.25f)
                    return true;
            }
        }
        return false;
    };
    const u32 parent_matrix = parent >= 0x00010000u && parent <= 0x01FFFEF0u
        ? parent + 0x110u : 0u;
    const float local_max = matrix_max(rot_addr);
    const float parent_max = matrix_max(parent_matrix);
    const float output_max = matrix_max(m110_addr);
    // Ignore zero-filled objects during world construction; they are not poses
    // and would otherwise consume the bounded diagnostic before the rig loads.
    if (local_max < 1.0e-6f && output_max < 1.0e-6f)
        return;
    if (local_max <= 4.0f && parent_max <= 4.0f && output_max <= 4.0f &&
        !degenerate(rot_addr) && !degenerate(parent_matrix) && !degenerate(m110_addr))
        return;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) >= 64u) return;
    std::fprintf(stderr,
        "[world-frame] self=%#010x parent=%#010x local_max=%g parent_max=%g output_max=%g local_pos=%#010x world_pos=%#010x\n",
        self, parent, (double)local_max, (double)parent_max, (double)output_max,
        pos_addr, self <= 0x01FFFEC0u ? self + 0x140u : 0u);
    auto dump_rotation = [&](const char* name, u32 addr) {
        if (!valid_matrix(addr)) return;
        const volatile float* m = reinterpret_cast<const volatile float*>(addr);
        std::fprintf(stderr, "[world-frame %s] self=%#010x r=", name, self);
        for (u32 row = 0; row < 3u; ++row)
            for (u32 col = 0; col < 3u; ++col)
                std::fprintf(stderr, "%s%g", (row == 0u && col == 0u) ? "" : ",",
                             (double)m[row * 4u + col]);
        std::fputc('\n', stderr);
    };
    dump_rotation("local", rot_addr);
    dump_rotation("parent", parent_matrix);
    dump_rotation("output", m110_addr);
}

// Opt-in trace of the unscaleByParent division: a near-zero parent matrix
// diagonal would blow up the attachment rotation.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_unscale(u32 self, u32 parent, float d0, float d1, float d2) {
    if (getenv("OPENRAC_TRACE_UNSCALE") == nullptr) return;
    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) >= 20u) return;
    std::fprintf(stderr,
        "[unscale] self=%#010x parent=%#010x diag=%g,%g,%g inv=%g,%g,%g\n",
        self, parent, (double)d0, (double)d1, (double)d2,
        1.0 / (double)d0, 1.0 / (double)d1, 1.0 / (double)d2);
}

// Opt-in trace of the attachment-frame matrix composition. Dumps the owner's
// inverse (e) and the local pose (q10) that MultiplyMatrices combines into the
// final bone matrix, so we can find where extreme values enter.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_attachment_compose(u32 self, u32 el, u32 e_addr, u32 q10_addr) {
    if (getenv("OPENRAC_TRACE_ATTACH_COMPOSE") == nullptr) return;
    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) >= 24u) return;
    auto dump = [](const char* name, u32 addr) {
        if (addr < 0x00010000u || addr >= 0x02000000u) return;
        const volatile float* v = reinterpret_cast<const volatile float*>(addr);
        std::fprintf(stderr, "[attach-compose] %s ", name);
        float mx = 0.0f;
        for (u32 i = 0; i < 16u; ++i) mx = std::max(mx, std::fabs(v[i]));
        for (u32 i = 0; i < 16u; ++i)
            std::fprintf(stderr, "%s%g", i == 0 ? "" : ",", static_cast<double>(v[i]));
        std::fprintf(stderr, " max=%g\n", static_cast<double>(mx));
    };
    std::fprintf(stderr, "[attach-compose] --- self=%#010x el=%#010x ---\n", self, el);
    dump("e", e_addr);
    dump("q10", q10_addr);
    // Owner transform v140 (position) / m110 (rotation), and the attachment
    // object's sub140/sub110, are at known offsets from self and el.
    if (self >= 0x00010000u && self < 0x02000000u) {
        dump("self.v140", self + 0x140u);
        dump("self.m110", self + 0x110u);
    }
    if (el >= 0x00010000u && el < 0x02000000u) {
        const u32 obj = *reinterpret_cast<const volatile u32*>(el);
        if (obj >= 0x00010000u && obj < 0x02000000u) {
            dump("obj.sub140", obj + 0x140u);
            dump("obj.sub110", obj + 0x110u);
            const u32 vt = *reinterpret_cast<const volatile u32*>(obj);
            if (vt >= 0x00010000u && vt < 0x10000000u) {
                // A FramedObject's vt is a ClassDescriptor: id at +4, flags at +8.
                static volatile u32 vt_notices = 0;
                if (__atomic_fetch_add(&vt_notices, 1u, __ATOMIC_RELAXED) < 4u) {
                    const int class_id = *reinterpret_cast<const volatile int*>(vt + 4u);
                    const int class_flags = *reinterpret_cast<const volatile int*>(vt + 8u);
                    const u32 f = *reinterpret_cast<const volatile u32*>(vt + 0xC0u);
                    std::fprintf(stderr,
                        "[attach-compose] obj=%#010x vt=%#010x class=%d flags=%#x f=%#010x\n",
                        obj, vt, class_id, class_flags, f);
                }
            }
        }
    }
}

// Opt-in trace of PollPadInput: does the pad state machine poll, and does
// injected input land in the decoded buttons?
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_poll_pad(u32 self, u32 buttons, u32 res) {
    if (getenv("OPENRAC_TRACE_TURN_HEADING") == nullptr) return;
    static volatile u32 last_buttons = 0xFFFFFFFFu;
    if (buttons != last_buttons) {
        last_buttons = buttons;
        write_hex("native host32: poll pad self ", (int)self);
        write_hex("  poll pad buttons ", (int)buttons);
        write_hex("  poll pad res ", (int)res);
    }
}

// Expose menu state to the host poll loop for the opt-in state-driven menu
// driver (the state globals live in this translation unit's anonymous namespace).
extern "C" u32 sly1_native_menu_page_and_state(void) {
    const u32 page = __atomic_load_n(&g_native_pause_page, __ATOMIC_RELAXED);
    const u32 ui = __atomic_load_n(&g_native_ui_state, __ATOMIC_RELAXED);
    const u32 open = __atomic_load_n(&g_native_pause_open, __ATOMIC_RELAXED);
    return (page & 0xFFu) | ((ui & 0xFFu) << 8u) | ((open & 0xFFu) << 16u);
}

// Opt-in trace of the pad buttons seen by ControlPlayer.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_player_pad_buttons(u32 buttons, u32 buffer_ptr, u32 buffer_bytes) {
    if (getenv("OPENRAC_TRACE_TURN_HEADING") == nullptr) return;
    static volatile u32 last_buttons = 0xFFFFFFFFu;
    static volatile u32 last_bytes = 0xFFFFFFFFu;
    if (buttons != last_buttons || buffer_bytes != last_bytes) {
        last_buttons = buttons;
        last_bytes = buffer_bytes;
        write_hex("native host32: player pad buttons ", (int)buttons);
        write_hex("  pad buffer ptr ", (int)buffer_ptr);
        write_hex("  pad buffer bytes ", (int)buffer_bytes);
    }
}

// Opt-in trace of ControlPlayer args.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_control_player(u32 self, u32 pad, u32 dt_bits) {
    if (getenv("OPENRAC_TRACE_TURN_HEADING") == nullptr) return;
    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) < 12u) {
        write_hex("native host32: control player self ", (int)self);
        write_hex("  control player pad ", (int)pad);
        write_hex("  control player dt ", (int)dt_bits);
    }
}

// Opt-in trace of ReadTurningColliderStickHeading pointers.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_turn_heading(u32 self, u32 pad) {
    if (getenv("OPENRAC_TRACE_TURN_HEADING") == nullptr) return;
    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) < 12u) {
        write_hex("native host32: turn heading self ", (int)self);
        write_hex("  turn heading pad ", (int)pad);
    }
}

// Opt-in trace of QueuePlacedObjectModelParts: count objects and drawn parts.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_model_parts_enter(u32 self, u32 partCount, u32 world) {
    if (getenv("OPENRAC_TRACE_MODEL_PARTS") == nullptr) return;
    static volatile u32 objects = 0, zero_parts = 0;
    const u32 n = __atomic_add_fetch(&objects, 1u, __ATOMIC_RELAXED);
    if (partCount == 0u) __atomic_add_fetch(&zero_parts, 1u, __ATOMIC_RELAXED);
    if (n <= 40u) {
        write_hex("native host32: queue_model_parts self ", (int)self);
        write_hex("  parts ", (int)partCount);
        write_hex("  world ", (int)world);
    }
    if (n == 300u) {
        write_hex("native host32: queue_model_parts OBJECTS ", (int)n);
        write_hex("  zero_part_objects ", (int)__atomic_load_n(&zero_parts, __ATOMIC_RELAXED));
    }
}
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_model_draw_result(u32 self, u32 drawn) {
    if (getenv("OPENRAC_TRACE_MODEL_PARTS") == nullptr) return;
    static volatile u32 draws = 0;
    const u32 n = __atomic_add_fetch(&draws, 1u, __ATOMIC_RELAXED);
    if (n <= 40u || drawn > 0u) {
        write_hex("native host32: model_draw_result self ", (int)self);
        write_hex("  drawn ", (int)drawn);
    }
}

// Opt-in trace of QueuePlacedObjectDraw: histogram of objects with/without a
// draw range and their part counts.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_queue_draw(u32 self, u32 drawRange, u32 partCount, u32 world) {
    if (getenv("OPENRAC_TRACE_QUEUE_DRAW") == nullptr) return;
    static volatile u32 with_range = 0, without_range = 0;
    static volatile u32 with_parts = 0, total = 0;
    const u32 t = __atomic_add_fetch(&total, 1u, __ATOMIC_RELAXED);
    if (drawRange != 0u) __atomic_add_fetch(&with_range, 1u, __ATOMIC_RELAXED);
    else __atomic_add_fetch(&without_range, 1u, __ATOMIC_RELAXED);
    if (partCount > 0u) __atomic_add_fetch(&with_parts, 1u, __ATOMIC_RELAXED);
    if (t <= 60u || (t % 200u == 0u)) {
        write_hex("native host32: queue_draw self ", (int)self);
        write_hex("  drawRange ", (int)drawRange);
        write_hex("  partCount ", (int)partCount);
        write_hex("  world ", (int)world);
    }
    if (t == 300u) {
        write_hex("native host32: queue_draw TOTAL ", (int)total);
        write_hex("  with_range ", (int)__atomic_load_n(&with_range, __ATOMIC_RELAXED));
        write_hex("  without_range ", (int)__atomic_load_n(&without_range, __ATOMIC_RELAXED));
        write_hex("  with_parts ", (int)__atomic_load_n(&with_parts, __ATOMIC_RELAXED));
    }
}

// Opt-in stage marker inside RunPendingWorldLoad.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_world_load_stage(u32 stage) {
    if (getenv("OPENRAC_TRACE_WORLD_LOAD") == nullptr) return;
    write_hex("native host32: world load stage ", (int)stage);
}

// One-shot forced-level boot, called from the game-thread frame-stage hook.
// Queues the matching catalog key through the retail QueueWorldLoad so the
// normal RunPendingWorldLoad path loads the level.
extern "C" void retail_queue_world_load(void* request, void* key, int, int, int)
    asm("_Z13func_001E4E80P16WorldLoadRequestP13WorldKeyBlockiii");
extern "C" void sly1_native_force_level_boot() {
    const u32 wanted = sly1_native_boot_level_key_for_sdl();
    if (wanted == 0) return;
    static volatile u32 done = 0;
    if (__atomic_load_n(&done, __ATOMIC_RELAXED) != 0u) return;
    __atomic_store_n(&done, 1u, __ATOMIC_RELAXED);
    extern u8 D_00247AF0[];
    extern u8 D_00275AF0[]; // g_worldLoadRequest
    const u32 table = (u32)(uintptr_t)D_00247AF0;
    for (u32 i = 0; i < 46u; ++i) {
        const u32 entry = table + i * 0x2Cu;
        if (*(volatile u32*)(entry + 0x20u) != wanted) continue;
        write_hex("native host32: forcing level entry ", (int)i);
        retail_queue_world_load((void*)D_00275AF0, (void*)entry, 0, 0, 0);
        return;
    }
    write_hex("native host32: level key not in catalog ", (int)wanted);
}

// Opt-in trace of script events dispatched to the Splash state graph.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_graph_event(u32 key, u32 scope, u32 list) {
    if (getenv("OPENRAC_TRACE_GRAPH_EVENT") == nullptr) return;

    static volatile u32 notices = 0;
    if (__atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED) < 64u)
        write_hex("native host32: graph event ", (int)((key << 16) | (list & 0xFFFFu))),
        write_hex("  graph scope ", (int)scope);
}

// Opt-in trace of the Splash countdown notifier's per-frame update.
extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_notifier_timer(u32 self, u32 state, u32 enabled, u32 time_left) {
    if (getenv("OPENRAC_TRACE_NOTIFIER_TIMER") == nullptr) return;
    static volatile u32 notices = 0;
    const u32 n = __atomic_fetch_add(&notices, 1u, __ATOMIC_RELAXED);
    if (n < 12u || (self == first_notifier_object() && n < 4096u))
        write_hex("native host32: notifier timer ", (int)((state << 24) | (enabled << 16) | (time_left & 0xFFFFu)));
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_frame_stage(u32 stage) {
    __atomic_store_n(&g_native_frame_stage, stage, __ATOMIC_RELAXED);
    // Only an explicit level key queues a direct world load.
    if (stage == 2)
        sly1_native_force_level_boot();
    if (stage == 8) {
        const u32 frame = __atomic_add_fetch(&g_native_frame_count, 1u, __ATOMIC_RELAXED);
        native_trace_sly_asset_tables(stage, frame);
        // Opt-in camera trace: does the camera position/view change with pad input?
        if (getenv("OPENRAC_TRACE_CAMERA") != nullptr && (frame % 20u) == 0u) {
            extern u32 D_00261990; // g_camera
            const u32 cam = D_00261990;
            if (cam >= 0x00010000u && cam < 0x02000000u) {
                const volatile float* pos = reinterpret_cast<const volatile float*>(cam + 0x40u);
                const volatile float* vp = reinterpret_cast<const volatile float*>(cam + 0x100u);
                std::fprintf(stderr, "[cam-trace] f=%u pos=%.1f,%.1f,%.1f vp0=%.4f,%.4f,%.4f,%.4f\n",
                             frame, (double)pos[0], (double)pos[1], (double)pos[2],
                             (double)vp[0], (double)vp[1], (double)vp[2], (double)vp[3]);
            }
        }
        // Port-only area-mask correction for explicit direct-level probes:
        // with no usable value tree, LookupValueTreeLeaf returns the all-areas
        // sentinel and QueueModelPartDraws culls most level parts. A mask of
        // 0x1 reveals the measured level area. Normal retail startup does not
        // apply this; OPENRAC_FIX_AREA_MASK can override the probe value (0
        // disables the correction).
        if (sly1_native_boot_level_key_for_sdl() != 0u) {
            u32 wanted_mask = 0x1u;
            if (const char* mask_setting = getenv("OPENRAC_FIX_AREA_MASK"))
                wanted_mask = (u32)strtoul(mask_setting, nullptr, 0);
            extern u32 D_00261990; // g_camera pointer
            const u32 cam = D_00261990;
            if (cam >= 0x00010000u && cam < 0x02000000u)
                *(volatile u32*)(cam + 0x21Cu) = wanted_mask;
        }
        static u32 player_trace_done = 0;
        const u32 trace_player = getenv("OPENRAC_TRACE_PLAYER") != nullptr;
        const u32 active_world = __atomic_load_n(&g_native_world, __ATOMIC_RELAXED);
        const u32 active_ui = __atomic_load_n(&g_native_ui_state, __ATOMIC_RELAXED);
        const u32 player = D_00262E10;
        if (trace_player && player_trace_done == 0 && frame >= 20u &&
            active_ui == 3u && active_world != 0u &&
            (player >= 0x00010000u || frame >= 120u)) {
            player_trace_done = 1;
            write_hex("native host32: player trace frame ", (int)frame);
            write_hex("  active world ", (int)active_world);
            write_hex("  active ui ", (int)active_ui);
            write_hex("native host32: g_player ", (int)player);
            if (player >= 0x00010000u && player < 0x02000000u) {
                const u32 desc = *(volatile u32*)player;
                write_hex("  player desc ", (int)desc);
                if (desc >= 0x00010000u && desc < 0x10000000u) {
                    write_hex("  player class ", (int)*(volatile u32*)(desc + 4u));
                    write_hex("  player id ", (int)*(volatile u32*)(player + 8u));
                    write_hex("  player world ", (int)*(volatile u32*)(player + 0x14u));
                    // pos (Vec4) at +0x100, raw float bits
                    write_hex("  player pos.x ", (int)*(volatile u32*)(player + 0x100u));
                    write_hex("  player pos.y ", (int)*(volatile u32*)(player + 0x104u));
                    write_hex("  player pos.z ", (int)*(volatile u32*)(player + 0x108u));
                    write_hex("  player drawPartCount ", (int)*(volatile u32*)(player + 0x238u));
                    write_hex("  player drawParts ", (int)*(volatile u32*)(player + 0x23Cu));
                    write_hex("  player partStates ", (int)*(volatile u32*)(player + 0x240u));
            const u32 linked_visuals[] = {
                *(volatile u32*)(player + 0x610u),
                *(volatile u32*)(player + 0x614u),
                *(volatile u32*)(player + 0x618u),
                *(volatile u32*)(player + 0x243Cu),
                *(volatile u32*)(player + 0x24F8u),
                *(volatile u32*)(player + 0x23D0u),
                *(volatile u32*)(player + 0x23D4u),
            };
            const char* linked_names[] = {
                "  player linkPartA", "  player linkPartB", "  player f618",
                "  player textureObject", "  player rigObject",
                "  player f23D0", "  player f23D4",
            };
            for (u32 i = 0; i < sizeof(linked_visuals) / sizeof(linked_visuals[0]); ++i) {
                const u32 linked = linked_visuals[i];
                write_hex(linked_names[i], (int)linked);
                if (linked >= 0x00010000u && linked < 0x02000000u) {
                    const u32 linked_desc = *(volatile u32*)linked;
                    if (linked_desc >= 0x00010000u && linked_desc < 0x10000000u) {
                        write_hex("    linked class", (int)*(volatile u32*)(linked_desc + 4u));
                        write_hex("    linked id", (int)*(volatile u32*)(linked + 8u));
                        write_hex("    linked drawPartCount", (int)*(volatile u32*)(linked + 0x238u));
                        write_hex("    linked drawParts", (int)*(volatile u32*)(linked + 0x23Cu));
                        write_hex("    linked partStates", (int)*(volatile u32*)(linked + 0x240u));
                    }
                }
            }
            const auto trace_children = [](const char* label, u32 root) {
                if (root < 0x00010000u || root >= 0x02000000u) return;
                write_text(label);
                u32 child = *(volatile u32*)(root + 0x34u);
                for (u32 i = 0; i < 64u && child >= 0x00010000u &&
                     child < 0x02000000u; ++i) {
                    write_hex("    child", (int)child);
                    const u32 child_desc = *(volatile u32*)child;
                    if (child_desc < 0x00010000u || child_desc >= 0x10000000u) break;
                    write_hex("      child class", (int)*(volatile u32*)(child_desc + 4u));
                    write_hex("      child id", (int)*(volatile u32*)(child + 8u));
                    if (*(volatile u32*)(child_desc + 0xCu) >= 0x244u) {
                        write_hex("      child drawPartCount", (int)*(volatile u32*)(child + 0x238u));
                        write_hex("      child drawParts", (int)*(volatile u32*)(child + 0x23Cu));
                    }
                    child = *(volatile u32*)(child + 0x1Cu);
                }
            };
            const auto trace_model_tree = [](u32 root) {
                u32 queue[256] = {};
                u32 depths[256] = {};
                u32 count = 1;
                queue[0] = root;
                for (u32 cursor = 0; cursor < count; ++cursor) {
                    const u32 object = queue[cursor];
                    if (object < 0x00010000u || object >= 0x02000000u) continue;
                    const u32 desc = *(volatile u32*)object;
                    if (desc < 0x00010000u || desc >= 0x10000000u) continue;
                    const u32 class_id = *(volatile u32*)(desc + 4u);
                    const u32 object_size = *(volatile u32*)(desc + 0xCu);
                    if (object_size < 0x2D0u) continue;
                    const u32 part_count = *(volatile u32*)(object + 0x238u);
                    if (part_count != 0u) {
                        write_hex("native host32: player tree model object", (int)object);
                        write_hex("  tree depth", (int)depths[cursor]);
                        write_hex("  tree class", (int)class_id);
                        write_hex("  tree id", (int)*(volatile u32*)(object + 8u));
                        write_hex("  tree part count", (int)part_count);
                        write_hex("  tree parts", (int)*(volatile u32*)(object + 0x23Cu));
                        write_hex("  tree states", (int)*(volatile u32*)(object + 0x240u));
                    }
                    u32 child = *(volatile u32*)(object + 0x34u);
                    for (u32 sibling = 0; sibling < 64u &&
                         child >= 0x00010000u && child < 0x02000000u &&
                         count < 256u; ++sibling) {
                        queue[count] = child;
                        depths[count] = depths[cursor] + 1u;
                        ++count;
                        child = *(volatile u32*)(child + 0x1Cu);
                    }
                }
                write_hex("native host32: player tree objects", (int)count);
            };
            trace_children("native host32: player direct children", player);
            trace_model_tree(player);
                }
            }
        }
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_frame_state(u32 world, u32 state, u32 count, u32 resume,
                              u32 load_request, u32 movie, u32 visible_ui,
                              u32 text_ui, u32 pad_status, u32 pad_mode,
                              u32 pad_flags, u32 pause_open, u32 pause_page,
                              u32 pause_mode, u32 fade_mode, u32 stack_top,
                              u32 stack_top_vt, u32 stack_top_mode,
                              u32 stack_top_text, u32 pad_routing,
                              u32 pad_select, u32 blank_pad_status) {
    __atomic_store_n(&g_native_world, world, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_state, state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_stack_count, count, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_resume_state, resume, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_world_load_request, load_request, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_movie_queued, movie, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_active_ui, visible_ui, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_text_ui, text_ui, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_status, pad_status, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_mode, pad_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_flags, pad_flags, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_open, pause_open, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_page, pause_page, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_mode, pause_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_fade_mode, fade_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top, stack_top, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_vt, stack_top_vt, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_mode, stack_top_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_text, stack_top_text, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_routing, pad_routing, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_select, pad_select, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_blank_pad_status, blank_pad_status, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_gate(u32 card_state, u32 card_target,
                              u32 card_display, u32 card_progress,
                              u32 memory_gate, u32 element_state,
                              u32 card_thread, u32 card_job,
                              u32 card_result, u32 card_semaphore) {
    __atomic_store_n(&g_native_card_state, card_state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_target, card_target, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_display, card_display, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_progress, card_progress, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_memory_gate, memory_gate, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_element_state, element_state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_thread, card_thread, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_job, card_job, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_result, card_result, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_semaphore, card_semaphore, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_timing(float elapsed, float state_time,
                                float display_time) {
    union FloatBits { float f; u32 u; };
    const FloatBits elapsed_bits{elapsed};
    const FloatBits state_bits{state_time};
    const FloatBits display_bits{display_time};
    __atomic_store_n(&g_native_elapsed_bits, elapsed_bits.u, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_state_time_bits, state_bits.u, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_display_time_bits, display_bits.u, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_update(u32 stage, u32 state, u32 target,
                                u32 display, float elapsed,
                                float state_time, u32 pause_open,
                                int pause_page) {
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (!g_startup_trace_enabled) return;
    static u32 last_state = 0xffffffffu;
    static u32 last_target = 0xffffffffu;
    static u32 last_display = 0xffffffffu;
    static u32 last_page = 0x7fffffffu;
    static u32 last_open = 0xffffffffu;
    static bool threshold_reported = false;
    static u32 calls = 0;
    ++calls;
    const bool state_changed = state != last_state || target != last_target ||
                               display != last_display;
    const bool page_changed = pause_page != last_page || pause_open != last_open;
    const bool threshold = elapsed - state_time >= 0.3f;
    if ((stage == 0 && (state_changed || (threshold && !threshold_reported) ||
                        (calls % 120u) == 1)) ||
        (stage == 1 && page_changed)) {
        std::fprintf(stderr,
                     "[native memory update] stage=%u state=%d target=%d display=%u elapsed=%.3f state_time=%.3f threshold=%u pause_open=%u page=%d\n",
                     stage, (int)state, (int)target, display,
                     (double)elapsed, (double)state_time, threshold ? 1u : 0u,
                     pause_open, pause_page);
    }
    if (state_changed) {
        last_state = state;
        last_target = target;
        last_display = display;
    }
    if (page_changed) {
        last_page = (u32)pause_page;
        last_open = pause_open;
    }
    if (stage == 0 && threshold) threshold_reported = true;
#else
    (void)stage;
    (void)state;
    (void)target;
    (void)display;
    (void)elapsed;
    (void)state_time;
    (void)pause_open;
    (void)pause_page;
#endif
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_display_setup(u32 width, u32 height, u32 context,
                                u64 scissor1, u64 scissor2) {
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (std::getenv("OPENRAC_TRACE_DIRECT") != nullptr)
        std::fprintf(stderr,
                     "[native display setup] width=%u height=%u context=%#x scissor1=%#llx scissor2=%#llx\n",
                     width, height, context,
                     static_cast<unsigned long long>(scissor1),
                     static_cast<unsigned long long>(scissor2));
#else
    (void)width; (void)height; (void)context; (void)scissor1; (void)scissor2;
#endif
}

#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_startup_callback(unsigned int index) {
    sly1_native_startup_callback_index = index + 1;
#if defined(OPENRAC_SLY1_WITH_SDL)
    const char* trace = std::getenv("OPENRAC_STARTUP_TRACE");
    if (trace != nullptr && trace[0] != '0')
        std::fprintf(stderr, "[native startup] callback=%u iop_stage=%u\n",
                     index + 1, sly1_native_iop_boot_stage);
#endif
}

extern "C" int sly1_native_host32_iop_boot(void) {
    if (g_disc_sector_count == 0) return 0;
    D_002613F0 = 0x14u;
    return 1;
}

extern "C" int sly1_native_host32_iop_initialize(int mode) {
    (void)mode;
    return g_disc_sector_count != 0;
}

extern "C" int sly1_native_host32_iop_services_start(int mode) {
    (void)mode;
    if (g_disc_sector_count == 0) return 0;
    *(volatile u32*)0x0027AE80u = 1u;
    return 1;
}

int main(int argc, char** argv) {
#if defined(OPENRAC_SLY1_WITH_SDL)
    // The mesh-submit path intermittently corrupts glibc tcache metadata; the
    // next malloc then aborts with "unaligned tcache chunk detected". Verified
    // over many runs: disabling tcache entirely makes the launcher stable.
    // glibc reads GLIBC_TUNABLES only at process start, so re-exec once with it
    // set before any game state is touched. OPENRAC_NO_TCACHE_FIX opts out.
    if (getenv("OPENRAC_TCACHE_REEXEC") == nullptr &&
        getenv("OPENRAC_NO_TCACHE_FIX") == nullptr) {
        setenv("GLIBC_TUNABLES", "glibc.malloc.tcache_count=0", 1);
        setenv("OPENRAC_TCACHE_REEXEC", "1", 1);
        execv(argv[0], argv);
        // If execv fails, continue with the current (tcache-enabled) libc.
    }
#endif
    sly1_native_host32_crt_initialized = 1;
    return sly1_native_host32_main(argc, argv);
}
#endif

#if !defined(OPENRAC_SLY1_WITH_SDL)
// Host32 diagnostic shims: SDL builds bind these services to the native
// runtime. The freestanding launcher has no input or renderer, but still
// needs to execute startup far enough to inspect resource loading.
extern "C" void sly1_native_host32_wait_frame(void) {}
extern "C" int sly1_native_host32_iop_boot(void) {
    if (g_disc_sector_count == 0) return 0;
    D_002613F0 = 0x14u;
    sly1_native_iop_boot_stage = 1;
    return 1;
}

extern "C" int sly1_native_host32_iop_initialize(int mode) {
    (void)mode;
    sly1_native_iop_boot_stage = 2;
    return g_disc_sector_count != 0;
}

extern "C" int sly1_native_host32_iop_services_start(int mode) {
    (void)mode;
    if (g_disc_sector_count == 0) return 0;
    *(volatile u32*)0x0027AE80u = 1u;
    sly1_native_iop_boot_stage = 3;
    return 1;
}

extern "C" void sly1_native_startup_callback(unsigned int index) {
    sly1_native_startup_callback_index = index + 1;
}

extern "C" unsigned long long openrac_guest_kernel_syscall(
    unsigned int service, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long) {
    // Host32 runs startup on one thread; the shared runtime's GS mask calls
    // therefore need no locking service here.
    (void)service;
    return 0;
}

extern "C" void openrac_native_renderer_submit_gif_stream(u32) {}
extern "C" int openrac_native_pad_read(int, unsigned char*, unsigned int) {
    return 0;
}
#endif
