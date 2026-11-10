/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port override: queue the retail request and synchronously service it. */
#include "common.h"

void func_001E4EA8(void* state);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

void func_001E4E80(void* block, s32 a, s32 b, s32 c, s32 d) {
    u8* bytes = (u8*)block;
    if (block == 0 || *(s32*)bytes != 0) return;
    *(s32*)(bytes + 0x00) = 1;
    *(s32*)(bytes + 0x04) = a;
    *(s32*)(bytes + 0x08) = b;
    *(s32*)(bytes + 0x0C) = c;
    *(s32*)(bytes + 0x10) = d;
    sly1_native_loader_stage(7, 0, (u32)block);
    /* The port's loader is synchronous, so dispatch this queued job now. */
    func_001E4EA8(block);
}
