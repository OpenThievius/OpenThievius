/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only LoadStateGraph backfill. The canonical unit currently fails the
 * C host frontend on unrelated header layout conflicts. Preserve its stream
 * reads, but guard a missing per-class stream callback before dispatch. */
#include "common.h"

s32 func_00137B90(void* stream);
s32 func_00137CC0(void* stream);
s32 func_00137D10(void* stream);
s32 func_00137D78(void* stream);
f32 func_00137DF8(void* stream);
void func_00152438(void* list, s32 link_offset);
void* func_0018D500(s32 size);
s32 func_001B63F8(void* self, s32 key);
void func_0011EC40(void* self, void* stream);
void* func_0011E580(s32 kind, void* world, void* parent, s32 b, s32 c);
void func_00182ED8(void* object);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

void func_001B5F50(void* object, void* stream) {
    u8* self = (u8*)object;
    s32 count;
    s32 i;
    s32 j;
    u32 entries;
    u32 edges;

    func_00152438(self + 0x48, 8);
    count = func_00137B90(stream);
    sly1_native_loader_stage(200u, (u32)count, (u32)stream);
    *(s32*)(self + 0x34) = count;
    entries = (u32)func_0018D500(count * 8);
    *(u32*)(self + 0x38) = entries;
    for (i = 0; i < count; ++i) {
        u8* entry = (u8*)entries + (u32)i * 8;
        *(s32*)(entry + 0) = func_00137D10(stream);
        *(s32*)(entry + 4) = func_00137D10(stream);
    }
    for (i = 0; i < count; ++i) {
        u8* entry = (u8*)entries + (u32)i * 8;
        if (*(s32*)(entry + 4) != -1)
            *(s32*)(entry + 4) = func_001B63F8(self, *(s32*)(entry + 4));
    }

    count = func_00137B90(stream);
    sly1_native_loader_stage(201u, (u32)count, (u32)stream);
    *(s32*)(self + 0x3C) = count;
    edges = (u32)func_0018D500(count * 0x14);
    *(u32*)(self + 0x40) = edges;
    for (j = 0; j < count; ++j) {
        u8* edge = (u8*)edges + (u32)j * 0x14;
        *(u32*)(edge + 0x00) = (u32)func_00137CC0(stream);
        *(s32*)(edge + 0x04) = func_001B63F8(self, func_00137D10(stream));
        *(s32*)(edge + 0x08) = func_001B63F8(self, func_00137D10(stream));
        *(u32*)(edge + 0x0C) = (u32)func_00137D78(stream);
        *(f32*)(edge + 0x10) = func_00137DF8(stream);
    }
    sly1_native_loader_stage(202u, (u32)count, (u32)stream);

    func_0011EC40(self, stream);
    sly1_native_loader_stage(203u, (u32)count, (u32)stream);
    {
        const s32 sentinel = func_00137D10(stream);
        sly1_native_loader_stage(204u, (u32)sentinel, (u32)stream);
    }
    for (j = 0; j < count; ++j) {
        u8* edge = (u8*)edges + (u32)j * 0x14;
        if (*(u32*)(edge + 0x00) != 0) {
            s32 kind = func_00137D10(stream);
            s32 b = func_00137D10(stream);
            s32 c = func_00137D10(stream);
            sly1_native_loader_stage(205u, (u32)kind, (u32)stream);
            sly1_native_loader_stage(206u, (u32)b, (u32)stream);
            sly1_native_loader_stage(207u, (u32)c, (u32)stream);
            u32 child = (u32)func_0011E580(kind, *(u32*)(self + 0x14),
                                            *(u32*)(self + 0x18), b, c);
            u32 descriptor;
            u32 load_callback;
            *(u32*)(edge + 0x00) = child;
            if (child < 0x10000u || child >= 0x02000000u) {
                sly1_native_loader_stage(156, kind, child);
                continue;
            }
            sly1_native_loader_stage(208u, child, (u32)stream);
            descriptor = *(u32*)child;
            load_callback = descriptor >= 0x10000u && descriptor < 0x02000000u
                ? *(u32*)(descriptor + 0x38) : 0;
            if (load_callback == 0) {
                /* Some class records have no stream loader in the native
                 * descriptor table. Keep the parsed object and skip only the
                 * absent virtual load call. */
                sly1_native_loader_stage(155, kind, child);
                continue;
            }
            (void)sly1_native_call_regs4(load_callback, child, (u32)stream, 0, 0);
            func_00182ED8((void*)child);
        }
    }
}
