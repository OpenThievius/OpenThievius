/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only state-link setup translated from US retail 0x00124DF8. */
#include "common.h"

void* func_0015A418(void* world, s32 mode, s32 id, void* owner);
void func_00182ED8(void* object);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_00124DF8(void* object, s32 count, void* links)
{
    u8* owner = (u8*)object;
    u8* link = (u8*)links;
    const u32 world = *(u32*)(owner + 0x14);

    if (count <= 0 || world == 0 || links == 0) return;

    for (s32 i = 0; i < count; ++i, link += 12) {
        const u32 flags = *(u32*)(link + 0);
        const s32 id = *(s32*)(link + 4);
        s32 mode = (flags & 1u) != 0 ? 5 : 1;
        if ((flags & 0x20u) != 0) mode |= 0x100;

        void* target = func_0015A418((void*)world, mode, id, object);
        if (target == 0) continue;

        if ((flags & 8u) == 0) {
            const s32 offset = *(s32*)(link + 8);
            *(u32*)(owner + offset) = (u32)target;
        }
        if ((flags & 4u) != 0) func_00182ED8(target);
        if ((flags & 0x10u) != 0)
            sly1_native_call_regs4(0x001832F0u, (u32)target,
                                   (u32)object, 0, 0);
    }
}
