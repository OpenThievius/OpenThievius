/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only scene batch/VIF preparation, translated from NTSC-U retail code. */
#include "common.h"

typedef struct SceneGroupNative {
    u32 first_object;
    s32 object_count;
    u32 key_flags;
    u16 kind;
    u16 output_index;
} SceneGroupNative;

typedef char check_SceneGroupNative[
    sizeof(SceneGroupNative) == 0x10 ? 1 : -1];

extern void func_001EE318(void* writer);
extern void func_00152980(void* writer, u32 qwords);
extern void func_00152B38(void* writer);
extern void func_001EE5C8(void* writer, void* unused1, s32 size,
                          void* unused3, s32* out_size, u32* out_cursor);
extern void func_00152D30(void* writer);
extern void func_00152AE0(void* writer, u32 dma_channel);
extern void func_0018D608(void);
extern void func_0018D6F0(void);

static s32 compare_objects(u32 left, u32 right) {
    s32 difference;
    difference = *(s32*)(left + 0x304u) - *(s32*)(right + 0x304u);
    if (difference != 0) return difference;
    difference = *(s32*)(left + 0x308u) - *(s32*)(right + 0x308u);
    if (difference != 0) return difference;
    difference = (s32)((*(u64*)(left + 0x2C8u) >> 38) & 1u) -
                 (s32)((*(u64*)(right + 0x2C8u) >> 38) & 1u);
    if (difference != 0) return difference;
    return *(s32*)(left + 0x2D0u) - *(s32*)(right + 0x2D0u);
}

static void sort_scene_objects(u32* objects, u32 count) {
    for (u32 i = 1; i < count; ++i) {
        const u32 value = objects[i];
        u32 position = i;
        while (position > 0 && compare_objects(objects[position - 1u], value) > 0) {
            objects[position] = objects[position - 1u];
            --position;
        }
        objects[position] = value;
    }
}

void func_00184DE8(void* world_object) {
    u32* objects = (u32*)0x00264268u;
    SceneGroupNative* groups = (SceneGroupNative*)0x00264670u;
    const u32 world = (u32)world_object;
    u32 object = *(u32*)(world + 0x1BE8u);
    u32 object_count = 0;
    u32 group_count = 0;
    u32 output_vectors = 0;
    s32 has_nonzero_resource = 0;
    u32* writer_words;
    s32 output_bytes = 0;
    u32 output_cursor = 0;
    __attribute__((aligned(16))) u8 writer[0x30];

    while (object != 0 && object_count < 0x100u) {
        objects[object_count++] = object;
        object = *(u32*)(object + 0x410u);
    }
    sort_scene_objects(objects, object_count);

    *(s32*)0x00264668u = 0;
    *(s32*)0x00264264u = 0;
    for (u32 i = 0; i < object_count; ++i) {
        const u32 current = objects[i];
        const s32 resource = *(s32*)(current + 0x304u);
        const s32 resource_group = *(s32*)(current + 0x308u);
        const s32 transform_kind = *(s32*)(current + 0x2D0u);
        const u64 flags = *(u64*)(current + 0x2C8u);
        u32 key_flags = (resource == 0) ? 1u : 0u;
        const u16 kind = (u16)(transform_kind ^ 3);
        if (resource_group != 0) key_flags |= 2u;
        if ((flags & (0x8000ull << 23)) != 0) key_flags |= 4u;

        u32 group_index = 0;
        while (group_index < group_count) {
            if (groups[group_index].object_count < 0x10 &&
                groups[group_index].kind == kind &&
                groups[group_index].key_flags == key_flags)
                break;
            ++group_index;
        }
        if (group_index == group_count && group_count < 0x10u) {
            SceneGroupNative* group = &groups[group_count++];
            group->first_object = current;
            group->object_count = 1;
            group->key_flags = key_flags;
            group->kind = kind;
            group->output_index = (u16)output_vectors;
            if (resource != 0) has_nonzero_resource = 1;
        } else if (group_index < group_count) {
            ++groups[group_index].object_count;
        }

        if (kind == 1u) ++output_vectors;
        else if (kind == 2u) output_vectors += 6u;
    }
    *(s32*)0x00264668u = (s32)group_count;
    *(s32*)0x00264264u = has_nonzero_resource;

    func_001EE318(writer);
    func_0018D608();
    func_00152980(writer, 0x102u);
    func_00152B38(writer);
    func_001EE5C8(writer, (void*)12u, (s32)output_vectors, 0,
                  &output_bytes, &output_cursor);

    if (output_cursor != 0) {
        for (u32 i = 0; i < object_count; ++i) {
            const u32 current = objects[i];
            const s32 transform_kind = *(s32*)(current + 0x2D0u);
            if (transform_kind == 1) {
                const u8* source = (const u8*)(current + 0x370u);
                for (u32 qword = 0; qword < 6; ++qword) {
                    for (u32 word = 0; word < 4; ++word)
                        *(u32*)(output_cursor + qword * 16u + word * 4u) =
                            *(const u32*)(source + qword * 16u + word * 4u);
                }
                output_cursor += 0x60u;
            } else {
                *(f32*)(output_cursor + 0u) =
                    -*(f32*)(current + 0x140u);
                *(f32*)(output_cursor + 4u) =
                    -*(f32*)(current + 0x144u);
                *(f32*)(output_cursor + 8u) =
                    -*(f32*)(current + 0x148u);
                *(f32*)(output_cursor + 12u) =
                    *(f32*)(current + 0x334u);
                output_cursor += 0x10u;
            }
        }
    }

    func_00152D30(writer);
    func_00152AE0(writer, *(u32*)0x00261F14u);
    func_0018D6F0();
}

