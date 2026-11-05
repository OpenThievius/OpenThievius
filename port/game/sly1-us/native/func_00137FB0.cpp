// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only collision mesh reader. The retail routine uses VU0 macros to build
// triangle planes; this version reads the same US stream fields and computes
// those planes with scalar C so native world setup can use the mesh.
#include "common.h"

struct SlyMesh03 {
    f32 radius;
    s32 vertex_count;
    void* vertices;
    s32 face_count;
    void* faces;
    s32 edge_count;
    void* edges;
    void* face_vectors;
    f32* face_values;
};

extern "C" f32 func_00137DF8(void* reader);
extern "C" s32 func_00137BE0(void* reader);
extern "C" s32 func_00137B90(void* reader);
extern "C" void func_00137E70(void* reader, void* destination);
extern "C" void* func_0018D500(s32 bytes);
extern "C" void* func_0018D5C0(s32 bytes);

static void clear_bytes(void* destination, u32 size) {
    auto* bytes = static_cast<u8*>(destination);
    for (u32 i = 0; i < size; ++i) bytes[i] = 0;
}

static f32 dot3(const f32* a, const f32* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

extern "C" void func_00137FB0(void* reader, void* output) {
    auto* mesh = static_cast<SlyMesh03*>(output);
    mesh->radius = func_00137DF8(reader);
    mesh->vertex_count = func_00137BE0(reader);
    if (mesh->vertex_count < 0 || mesh->vertex_count > 65535) return;

    auto* vertices = static_cast<f32*>(func_0018D500(mesh->vertex_count * 16));
    mesh->vertices = vertices;
    clear_bytes(vertices, static_cast<u32>(mesh->vertex_count) * 16u);
    for (s32 i = 0; i < mesh->vertex_count; ++i)
        func_00137E70(reader, vertices + i * 4);

    mesh->face_count = func_00137BE0(reader);
    if (mesh->face_count < 0 || mesh->face_count > 65535) return;
    auto* faces = static_cast<u8*>(func_0018D5C0(mesh->face_count * 32));
    auto* face_vectors = static_cast<f32*>(func_0018D500(mesh->face_count * 16));
    auto* face_values = static_cast<f32*>(func_0018D500(mesh->face_count * 4));
    mesh->faces = faces;
    mesh->face_vectors = face_vectors;
    mesh->face_values = face_values;
    clear_bytes(faces, static_cast<u32>(mesh->face_count) * 32u);
    clear_bytes(face_vectors, static_cast<u32>(mesh->face_count) * 16u);
    clear_bytes(face_values, static_cast<u32>(mesh->face_count) * 4u);

    mesh->edge_count = func_00137BE0(reader);
    if (mesh->edge_count < 0 || mesh->edge_count > 65535) return;
    auto* edges = static_cast<u8*>(func_0018D5C0(mesh->edge_count * 16));
    mesh->edges = edges;
    clear_bytes(edges, static_cast<u32>(mesh->edge_count) * 16u);

    s32 edge_cursor = 0;
    s32* edge_tails = static_cast<s32*>(func_0018D500(mesh->face_count * 4));
    clear_bytes(edge_tails, static_cast<u32>(mesh->face_count) * 4u);

    for (s32 face_index = 0; face_index < mesh->face_count; ++face_index) {
        const u16 i0 = static_cast<u16>(func_00137BE0(reader));
        const u16 i1 = static_cast<u16>(func_00137BE0(reader));
        const u16 i2 = static_cast<u16>(func_00137BE0(reader));
        auto* face = faces + face_index * 32;
        if (i0 < mesh->vertex_count && i1 < mesh->vertex_count &&
            i2 < mesh->vertex_count) {
            const f32* p0 = vertices + i0 * 4;
            const f32* p1 = vertices + i1 * 4;
            const f32* p2 = vertices + i2 * 4;
            const f32 ax = p1[0] - p0[0];
            const f32 ay = p1[1] - p0[1];
            const f32 az = p1[2] - p0[2];
            const f32 bx = p2[0] - p0[0];
            const f32 by = p2[1] - p0[1];
            const f32 bz = p2[2] - p0[2];
            f32* normal = reinterpret_cast<f32*>(face);
            normal[0] = ay * bz - az * by;
            normal[1] = az * bx - ax * bz;
            normal[2] = ax * by - ay * bx;
            const f32 norm = __builtin_sqrtf(dot3(normal, normal));
            if (norm > 0.0f) {
                normal[0] /= norm;
                normal[1] /= norm;
                normal[2] /= norm;
            }
            *reinterpret_cast<f32*>(face + 0x10) = -dot3(normal, p0);
        }
        *reinterpret_cast<u16*>(face + 0x1E) = i0;

        face_values[face_index] = func_00137DF8(reader);
        func_00137E70(reader, face_vectors + face_index * 4);
        const s32 face_edges = func_00137B90(reader);
        for (s32 edge_in_face = 0; edge_in_face < face_edges; ++edge_in_face) {
            const u16 vertex_a = static_cast<u16>(func_00137BE0(reader));
            const u16 vertex_b = static_cast<u16>(func_00137BE0(reader));
            const u16 neighbor = static_cast<u16>(func_00137BE0(reader));
            if (edge_cursor >= mesh->edge_count) continue;

            auto* edge = edges + edge_cursor * 16;
            *reinterpret_cast<u16*>(edge + 0) = vertex_a;
            *reinterpret_cast<u16*>(edge + 2) = vertex_b;
            *reinterpret_cast<u16*>(edge + 4) = static_cast<u16>(face_index);
            *reinterpret_cast<u16*>(edge + 6) = neighbor;
            if (edge_tails[face_index] == 0) {
                *reinterpret_cast<u32*>(face + 0x14) = reinterpret_cast<u32>(edge);
            } else {
                *reinterpret_cast<u32*>(edge_tails[face_index] + 8) =
                    reinterpret_cast<u32>(edge);
            }
            edge_tails[face_index] = reinterpret_cast<s32>(edge);
            if (neighbor < mesh->face_count) {
                auto* neighbor_face = faces + neighbor * 32;
                *reinterpret_cast<u32*>(edge + 0x0C) =
                    *reinterpret_cast<u32*>(neighbor_face + 0x18);
                *reinterpret_cast<u32*>(neighbor_face + 0x18) =
                    reinterpret_cast<u32>(edge);
            }
            ++edge_cursor;
        }
    }
}
