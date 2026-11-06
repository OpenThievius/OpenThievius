/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only object lookup used by object-group relationship records. */
#include "common.h"

void* func_00159F20(void* world, s32 id);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

static u32 find_in_group(u32 group, s32 id, u32 depth)
{
    if (group == 0 || depth >= 16) return 0;

    const u32 descriptor = *(u32*)group;
    if (descriptor == 0 || (*(u32*)(descriptor + 8) & 0x100u) == 0) {
        return *(s32*)(group + 8) == id ? group : 0;
    }

    u32 cell = *(u32*)(group + 0x2D0u);
    while (cell != 0) {
        const u32 child = *(u32*)cell;
        if (child != 0 && *(s32*)(child + 8u) == id) return child;
        const u32 found = find_in_group(child, id, depth + 1u);
        if (found != 0) return found;
        cell = *(u32*)(cell + 8u);
    }
    return 0;
}

void* func_0015A418(void* world, s32 type, s32 id, void* owner)
{
    static u32 trace_count;
    (void)type;
    (void)owner;
    if (world == 0 || id < 0) return 0;

    const u32 bucket = (u32)func_00159F20(world, id);
    u32 object = *(u32*)bucket;
    if (trace_count < 3) {
        sly1_native_loader_stage(101, (u32)world, (u32)owner);
        sly1_native_loader_stage(102, bucket, (u32)owner);
        sly1_native_loader_stage(103, object, (u32)owner);
        ++trace_count;
    }
    while (object != 0) {
        /* The bucket can contain parent groups as well as direct matches. */
        const u32 found = find_in_group(object, id, 0);
        if (found != 0) return (void*)found;
        /* Hash buckets use each object's intrusive link at +0x0C. */
        object = *(u32*)(object + 0x0Cu);
    }
    return 0;
}
