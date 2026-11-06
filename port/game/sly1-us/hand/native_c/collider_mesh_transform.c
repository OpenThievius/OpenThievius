/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scalar translation of the retail collider VU mesh transform. */
#include "common.h"

void func_00125138(void* object);

typedef struct Mesh03View {
    f32 radius;
    s32 vertex_count;
    u32 vertices;
    s32 face_count;
    u32 faces;
    s32 link_capacity;
    u32 links;
    u32 face_vectors;
} Mesh03View;

static f32 transform_component(const f32* matrix, const f32* vector,
                               const f32* translation, s32 lane,
                               s32 add_translation)
{
    f32 value = matrix[lane] * vector[0];
    value += matrix[4 + lane] * vector[1];
    value += matrix[8 + lane] * vector[2];
    if (add_translation) value += translation[lane];
    return value;
}

static void transform_mesh(const Mesh03View* source, Mesh03View* destination,
                           const f32* translation, const f32* matrix)
{
    s32 index;

    /* Retail skips the entire transform when the vertex arrays are shared. */
    if (source->vertices == destination->vertices) return;

    for (index = 0; index < source->vertex_count; ++index) {
        const f32* input = (const f32*)(source->vertices + (u32)index * 16u);
        f32* output = (f32*)(destination->vertices + (u32)index * 16u);
        const f32 x = input[0], y = input[1], z = input[2];
        const f32 vector[3] = {x, y, z};
        for (s32 lane = 0; lane < 3; ++lane)
            output[lane] = transform_component(matrix, vector, translation,
                                               lane, 1);
        output[3] = input[3] + translation[3];
    }

    for (index = 0; index < source->face_count; ++index) {
        const u32 source_face = source->faces + (u32)index * 32u;
        const u32 destination_face = destination->faces + (u32)index * 32u;
        const f32* input_normal = (const f32*)source_face;
        f32* output_normal = (f32*)destination_face;
        const f32 normal[3] = {input_normal[0], input_normal[1], input_normal[2]};
        const u16 vertex_index = *(u16*)(destination_face + 30u);

        for (s32 lane = 0; lane < 3; ++lane)
            output_normal[lane] = transform_component(matrix, normal,
                                                      translation, lane, 0);
        output_normal[3] = input_normal[3];

        if (vertex_index < (u32)destination->vertex_count) {
            const f32* point = (const f32*)(destination->vertices +
                                             (u32)vertex_index * 16u);
            *(f32*)(destination_face + 16u) = output_normal[0] * point[0] +
                                              output_normal[1] * point[1] +
                                              output_normal[2] * point[2];
        }

        if (source->face_vectors != 0 && destination->face_vectors != 0) {
            const f32* input_plane = (const f32*)(source->face_vectors +
                                                   (u32)index * 16u);
            f32* output_plane = (f32*)(destination->face_vectors +
                                       (u32)index * 16u);
            const f32 plane[3] = {input_plane[0], input_plane[1], input_plane[2]};
            for (s32 lane = 0; lane < 3; ++lane)
                output_plane[lane] = transform_component(matrix, plane,
                                                          translation, lane, 1);
            output_plane[3] = input_plane[3] + translation[3];
        }
    }
}

void func_001B8960(void* collider)
{
    u8* self = (u8*)collider;
    const f32* translation = (const f32*)(self + 0x140u);
    const f32* matrix = (const f32*)(self + 0x110u);

    func_00125138(collider);
    transform_mesh((const Mesh03View*)(self + 0x380u),
                   (Mesh03View*)(self + 0x3A4u), translation, matrix);
    transform_mesh((const Mesh03View*)(self + 0x4E8u),
                   (Mesh03View*)(self + 0x50Cu), translation, matrix);
}

/* Port-only translation of the retail previous-position callback. */
void func_001B9680(void* collider)
{
    u8* self = (u8*)collider;
    __builtin_memcpy(self + 0x370u, self + 0x140u, 16u);
}
