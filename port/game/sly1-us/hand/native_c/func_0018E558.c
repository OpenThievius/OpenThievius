// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only functional backfill for Sly's queued display worker.
#include "common.h"

extern u8 D_00269A00[];
extern s32 D_00261F1C;

s32 func_001F6B60(s32 semaphore);
void func_001686E8(s32 queue, s32 item);
void func_001684E8(void);
void func_001683E8(void);

void func_0018E558(void) {
    for (;;) {
        *(s32*)(D_00269A00 + 0x134) = 0;
        func_001F6B60(*(s32*)(D_00269A00 + 0x130));

        func_001686E8(D_00261F1C, *(s32*)(D_00269A00 + 0x114));
        *(s32*)(D_00269A00 + 0x134) = 1;
        func_001684E8();

        *(s32*)(D_00269A00 + 0x134) = 2;
        func_001686E8(D_00261F1C, *(s32*)(D_00269A00 + 0x118));
        func_001683E8();

        func_001686E8(D_00261F1C, *(s32*)(D_00269A00 + 0x118));
        func_001683E8();
    }
}
