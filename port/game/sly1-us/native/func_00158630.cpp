// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for the handwritten VU0 ComputeChainedWorldFrame.
// Keep this separate from sly1-decomp so the routine can be replaced when its
// retail implementation is decompiled into portable C.
#include "common.h"

namespace {

constexpr u32 kTransformPosition = 0x40u;
constexpr u32 kTransformRotation = 0x50u;
constexpr u32 kObjectDescriptor = 0x00u;
constexpr u32 kObjectParent = 0x18u;
constexpr u32 kDescriptorId = 0x04u;
constexpr u32 kMaxTransformChain = 128u;

bool is_transform_object(const u8* object) {
    if (object == nullptr) return false;
    const u8* descriptor = *reinterpret_cast<u8* const*>(object + kObjectDescriptor);
    if (descriptor == nullptr) return false;
    const s32 id = *reinterpret_cast<const s32*>(descriptor + kDescriptorId);
    // These are TransformObject and the classes laid out from it in the US
    // class table. Other BaseObject subclasses do not have fields at +0x40.
    return id == 0x73 || id == 0x74 || id == 0x75 ||
           (id >= 0x77 && id <= 0x7A);
}

void rotate_row(const f32* vector, const f32* matrix, f32* out) {
    for (u32 column = 0; column < 3u; ++column) {
        out[column] = vector[0] * matrix[column] +
                      vector[1] * matrix[4u + column] +
                      vector[2] * matrix[8u + column];
    }
}

void multiply_rotation(const f32* local, const f32* parent, f32* out) {
    for (u32 row = 0; row < 3u; ++row) {
        rotate_row(local + row * 4u, parent, out + row * 4u);
        out[row * 4u + 3u] = 0.0f;
    }
}

}  // namespace

void func_00158630(void* object_arg, void* position_arg, void* rotation_arg) {
    auto* object = static_cast<u8*>(object_arg);
    auto* position = static_cast<f32*>(position_arg);
    auto* rotation = static_cast<f32*>(rotation_arg);
    if (object == nullptr || position == nullptr || rotation == nullptr) return;

    const u8* chain[kMaxTransformChain];
    u32 count = 0;
    for (const u8* current = object;
         current != nullptr && count < kMaxTransformChain && is_transform_object(current);
         current = *reinterpret_cast<u8* const*>(current + kObjectParent)) {
        chain[count++] = current;
    }

    f32 world_position[3] = {0.0f, 0.0f, 0.0f};
    f32 world_rotation[12] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    while (count != 0u) {
        const u8* current = chain[--count];
        const f32* local_position = reinterpret_cast<const f32*>(current + kTransformPosition);
        const f32* local_rotation = reinterpret_cast<const f32*>(current + kTransformRotation);
        f32 moved[3];
        rotate_row(local_position, world_rotation, moved);
        for (u32 axis = 0; axis < 3u; ++axis)
            world_position[axis] += moved[axis];

        f32 composed[12];
        multiply_rotation(local_rotation, world_rotation, composed);
        for (u32 lane = 0; lane < 12u; ++lane)
            world_rotation[lane] = composed[lane];
    }

    position[0] = world_position[0];
    position[1] = world_position[1];
    position[2] = world_position[2];
    position[3] = 1.0f;
    for (u32 lane = 0; lane < 12u; ++lane)
        rotation[lane] = world_rotation[lane];
}
