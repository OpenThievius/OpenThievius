/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource image payload read from the retail loader path. */
#include "common.h"

extern s32 D_00275718;
extern s32 D_0027571C;
extern s32 D_00275720;
extern s32 D_00275728;

void func_00137B60(void* reader, s32 alignment);
void func_00137A60(void* reader, s32 size, void* destination);

void func_001DBA38(void* opaque) {
    u8* reader = (u8*)opaque;

    if (D_0027571C == 0) return;

    D_00275728 = *(s32*)(reader + 0x14u);
    func_00137B60(reader, 0x10);
    func_00137A60(reader, D_00275720, (void*)(u32)D_00275718);
}
