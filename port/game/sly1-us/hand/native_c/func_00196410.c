/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only object-group stream reader, separate from matching Sly C. */
#include "common.h"

void func_00152438(void* list, s32 element_size);
void func_00152470(void* list, void* item);
s32 func_00152720(void* list);
void func_00137EB0(void* reader, void* destination);
void func_00137E70(void* reader, void* destination);
s32 func_00137B90(void* reader);
s32 func_00137D10(void* reader);
u32 func_00137C40(void* reader);
s32 func_00137CC0(void* reader);
f32 func_00137DF8(void* reader);
void* func_0011E580(s32 id, void* world, void* parameter, s32 key, s32 flags);
void* func_001C1390(s32 id, void* world);
void func_0011EC40(void* object, void* reader);
void func_0011ECB8(void* world, void* object, void* reader);
void* func_0018D5C0(s32 size);
u32 sly1_native_temp_alloc(s32 size);
void sly1_native_temp_begin(void);
void sly1_native_temp_end(void);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

static u32 call_method(u32 descriptor, u32 offset, u32 object, u32 argument)
{
    u32 method = descriptor != 0 ? *(u32*)(descriptor + offset) : 0;
    if (method == 0) return 0;
    return sly1_native_call_regs4(method, object, argument, 0, 0);
}

void func_00196410(void* self, void* reader)
{
    u8* object = (u8*)self;
    u8* stream = (u8*)reader;
    const u32 world = *(u32*)(object + 0x14);
    const u32 descriptor = *(u32*)object;
    const u32 list = (u32)object + 0x34;
    s32 child_count;
    s32 relationship_count;
    u32* children;
    u8* frame;

    sly1_native_loader_stage(90, (u32)self, (u32)reader);
    sly1_native_temp_begin();
    children = (u32*)sly1_native_temp_alloc(256 * 4);
    frame = (u8*)sly1_native_temp_alloc(16);
    if (children == 0 || frame == 0) {
        sly1_native_temp_end();
        return;
    }

    func_00152438(object + 0x2D0, 8);
    func_00137EB0(stream, object + 0xD0);
    func_00137E70(stream, object + 0x100);
    child_count = func_00137B90(stream);
    sly1_native_loader_stage(92, (u32)child_count, (u32)reader);
    if (descriptor != 0) (void)call_method(descriptor, 0x58, (u32)self, 0);

    for (s32 i = 0; i < child_count; ++i) {
        s32 id = func_00137D10(stream);
        sly1_native_loader_stage(120, (u32)id, (u32)reader);
        u32 child;
        if (id == -1) {
            s32 key = func_00137D10(stream);
            sly1_native_loader_stage(121, (u32)key, (u32)reader);
            child = sly1_native_call_regs4(0x001DC208u, world, (u32)key, 0, 0);
        } else {
            s32 key = func_00137D10(stream);
            sly1_native_loader_stage(121, (u32)key, (u32)reader);
            s32 flags = func_00137D10(stream);
            sly1_native_loader_stage(122, (u32)flags, (u32)reader);
            child = (u32)func_0011E580(id, (void*)world, 0, key, flags);
            if (child != 0) {
                sly1_native_loader_stage(123, *(u32*)(child + 8), (u32)reader);
                u32 child_descriptor = *(u32*)child;
                sly1_native_loader_stage(
                    124, child_descriptor != 0 ? *(u32*)(child_descriptor + 0x38) : 0,
                    (u32)reader);
                (void)call_method(child_descriptor, 0x38, child, (u32)reader);
                sly1_native_loader_stage(
                    125, child_descriptor != 0 ? *(u32*)(child_descriptor + 0x38) : 0,
                    (u32)reader);
            }
            s32 link_id = func_00137D10(stream);
            sly1_native_loader_stage(104, (u32)link_id, (u32)reader);
            (void)sly1_native_call_regs4(0x001DC150u, world, child, (u32)link_id, 0);
        }

        if (child != 0) {
            u32 cell = (u32)func_0018D5C0(16);
            if (cell != 0) {
                *(u32*)(cell + 0) = child;
                *(u32*)(cell + 4) = *(u32*)(object + 8);
                *(u32*)(child + 0x30) = cell;
                func_00152470(object + 0x2D0, (void*)cell);
            }
        }
        children[i] = child;
    }
    sly1_native_loader_stage(93, (u32)child_count, (u32)reader);
    if (child_count > 0) sly1_native_loader_stage(100, children[0], (u32)reader);

    relationship_count = func_00137B90(stream);
    sly1_native_loader_stage(94, (u32)relationship_count, (u32)reader);
    for (s32 j = 0; j < relationship_count; ++j) {
        const f32 large_distance = 1.0e10f;
        u32 target = 0;
        s32 id = func_00137D10(stream);
        sly1_native_loader_stage(97, (u32)id, (u32)reader);
        for (s32 i = 0; i < child_count; ++i) {
            u32 child = children[i];
            if (child == 0) continue;
            if (*(s32*)(child + 8) == id) {
                target = child;
            } else {
                u32 child_descriptor = *(u32*)child;
                if (child_descriptor != 0 &&
                    (*(u32*)(child_descriptor + 8) & 1u) != 0) {
                    target = sly1_native_call_regs4(
                        0x0015A418u, world, 0x101u, (u32)id, child);
                }
            }
            if (target != 0) break;
        }

        s32 resource_id = func_00137D10(stream);
        if (resource_id != -1) {
            u32 resource = (u32)func_001C1390(resource_id, (void*)world);
            if (target != 0) {
                u32 old_resource = *(u32*)(target + 0x2C);
                if (old_resource != 0)
                    (void)sly1_native_call_regs4(0x0011A338u, resource,
                                                  old_resource, 0, 0);
                *(u32*)(target + 0x2C) = resource;
            }
        }

        u32 word = func_00137C40(stream);
        if (target != 0) {
            u32 target_descriptor = *(u32*)target;
            if (target_descriptor != 0 &&
                (*(u32*)(target_descriptor + 8) & 1u) != 0)
                *(u32*)(target + 0x88) = word;
        }

        s32 mode = func_00137CC0(stream);
        if (mode == 2) {
            f32 distance = func_00137DF8(stream);
            if (target != 0) {
                u32 flags = *(u32*)(target + 0x2C8);
                *(u32*)(target + 0x2C8) = (flags & 0xCFFFFFFFu) | 0x20000000u;
                *(f32*)(target + 0x80) = distance == 3.402823466e+38f
                    ? large_distance : distance;
            }
        }
        sly1_native_loader_stage(98, target, (u32)reader);
        if (target != 0) func_0011EC40((void*)target, reader);
        sly1_native_loader_stage(99, target, (u32)reader);
    }
    sly1_native_loader_stage(95, (u32)relationship_count, (u32)reader);

    for (s32 i = 0; i < child_count; ++i) {
        u32 child = children[i];
        if (child == 0) continue;
        u32 child_descriptor = *(u32*)child;
        (void)call_method(child_descriptor, 0x68, child, (u32)self);
        (void)call_method(child_descriptor, 0x64, child, *(u32*)(object + 0x18));
    }

    if (child_count == 1) {
        func_0011ECB8((void*)world, self, reader);
        if (func_00152720((void*)list) == 0) {
            const u32 previous_frame = *(u32*)0x0027B314u;
            const u32 link_offset = *(u32*)(list + 8);
            u32 child = children[0];
            u32 item = *(u32*)list;
            *(u32*)(frame + 0) = list;
            *(u32*)(frame + 4) = previous_frame;
            *(u32*)(frame + 8) = link_offset;
            *(u32*)(frame + 12) = item != 0 ? item + link_offset : 0;
            *(u32*)0x0027B314u = (u32)frame;
            while (item != 0) {
                u32 item_descriptor = *(u32*)item;
                (void)call_method(item_descriptor, 0x64, item, child);
                item = *(u32*)(frame + 12);
                *(u32*)(frame + 12) = item != 0 ? item + link_offset : 0;
            }
            *(u32*)0x0027B314u = previous_frame;
        }
    }
    sly1_native_loader_stage(96, (u32)child_count, (u32)reader);

    if (descriptor != 0) {
        (void)call_method(descriptor, 0x1C, (u32)self, 0);
        (void)call_method(descriptor, 0x18, (u32)self, 0);
    }
    sly1_native_loader_stage(91, (u32)self, (u32)reader);
    sly1_native_temp_end();
}
