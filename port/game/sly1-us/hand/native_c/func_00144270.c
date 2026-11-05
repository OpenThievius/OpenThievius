/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill copied from the current NTSC-U decompilation. */
#include "common.h"

extern void* D_00275710;
void func_00144008(void* object);
void func_001EEBB0(void* context, void* input, s32* output);

void func_00144270(void* object) {
    u8* bytes = (u8*)object;
    void* root;
    u32 input[4];
    s32 output;

    func_00144008(object);
    root = D_00275710 != 0 ? *(void**)D_00275710 : 0;
    if (root == 0) return;

    __builtin_memcpy(input, bytes + 0x40, sizeof(input));
    func_001EEBB0(*(void**)((u8*)root + 0x1EC4), input, &output);
    if (output != 0) *(s32*)(bytes + 0x21C) = output;
}
