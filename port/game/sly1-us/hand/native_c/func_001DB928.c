/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource header setup from retail func_001DB928. */
#include "common.h"

extern s32 D_00275718;
extern s32 D_0027571C;
extern s32 D_00275720;
extern s32 D_00275724;
extern s32 D_00275728;

s32 func_00137D78(void* reader);
void* func_0018D500(s32 size);

void func_001DB928(s32 changed, void* reader)
{
    u8* stream = (u8*)reader;
    s32 amount;
    s32 revision;
    s32 buffer;

    if (*(s32*)(stream + 4) == 1) {
        if (changed == 0) {
            func_00137D78(reader);
            *(s32*)(stream + 0x14) -= D_00275728;
        }
        amount = func_00137D78(reader);
        revision = func_00137D78(reader);
    } else {
        amount = func_00137D78(reader);
        revision = func_00137D78(reader);
        if (revision != D_00275724) changed = 1;
        if (changed == 0) {
            s32 remaining = *(s32*)(stream + 0x14) - D_00275728;
            *(s32*)(stream + 0x14) = remaining > 0 ? remaining : 0;
        }
    }

    buffer = (s32)func_0018D500(amount);
    D_00275724 = revision;
    D_00275720 = amount;
    D_00275718 = buffer;
    D_0027571C = changed;
}
