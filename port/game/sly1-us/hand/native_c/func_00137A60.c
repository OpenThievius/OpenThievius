/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only byte reader backfill for Sly's buffered resource streams. */
#include "common.h"

void func_001378A0(void* self);
void func_00137820(void* self);
void func_0018D800(void* destination, void* source, s32 size);

void func_00137A60(void* self, s32 size, void* destination) {
    u8* object = (u8*)self;
    u8* output = (u8*)destination;

    if (*(s32*)(object + 0x1C) < 0 || size <= 0) return;

    while (size > 0) {
        s32 available = *(s32*)(object + 0x1C);
        if (available == 0) {
            if ((*(u32*)object & 4u) != 0) {
                func_001378A0(self);
            } else {
                func_00137820(self);
                available = *(s32*)(object + 0x24);
                *(s32*)(object + 0x1C) = available;
                *(u32*)(object + 0x18) = *(u32*)(object + 0x20);
                *(u32*)(object + 0x20) += (u32)available;
                *(s32*)(object + 0x24) = 0;
            }

            available = *(s32*)(object + 0x1C);
            if (available == 0) {
                *(s32*)(object + 0x1C) = -1;
                return;
            }
        }

        s32 copied = size < available ? size : available;
        if (output != 0) {
            func_0018D800(output, (void*)*(u32*)(object + 0x18), copied);
            output += copied;
        }

        size -= copied;
        *(u32*)(object + 0x18) += (u32)copied;
        *(s32*)(object + 0x1C) -= copied;
    }
}
