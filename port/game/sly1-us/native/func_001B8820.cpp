// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only structural mesh copy for the retail VU transform boundary.
// The native renderer currently preserves source geometry until it has a
// matching CPU transform and scene submission path.
#include "sly1/host_abi.h"

struct Mesh03Host {
    f32 radius;
    s32 vertex_count;
    void* vertices;
    s32 face_count;
    void* faces;
    u8 tail[0x10];
};

typedef u64 QuadB8820 __attribute__((mode(TI)));

extern "C" void func_001B8820(Mesh03Host* source, Mesh03Host* destination,
                                QuadB8820, void*) {
    if (source == nullptr || destination == nullptr) return;
    if (source->vertices != destination->vertices && source->vertex_count > 0 &&
        source->vertices != nullptr && destination->vertices != nullptr) {
        __builtin_memcpy(destination->vertices, source->vertices,
                         static_cast<u32>(source->vertex_count) * 16u);
    }
    if (source->faces != destination->faces && source->face_count > 0 &&
        source->faces != nullptr && destination->faces != nullptr) {
        __builtin_memcpy(destination->faces, source->faces,
                         static_cast<u32>(source->face_count) * 32u);
    }
    __builtin_memcpy(destination->tail, source->tail, sizeof(source->tail));
    destination->radius = source->radius;
    destination->vertex_count = source->vertex_count;
    destination->face_count = source->face_count;
}
