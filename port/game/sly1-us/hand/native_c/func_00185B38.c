// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for the frame system's callback initialization.
#include "common.h"

typedef struct FrameSetup {
    s32 value[5];
    u32 words[4];
} FrameSetup;

typedef struct FrameCounter {
    u32* ptr;
    s32 scaled;
    s32 size;
    s32 value;
    s32 repeat;
} FrameCounter;

typedef struct FrameQueue {
    s32 value[5];
} FrameQueue;

void* func_00193210(FrameSetup* self, u32* first, u32* second, u32* third, u32* fourth);
void func_00191E70(s32 value);
void func_00193270(FrameQueue* state);
void func_00193288(FrameCounter* state, s32 value);
void func_00193300(FrameQueue* state);
void func_00191EB0(void);

void func_00185B38(void) {
    u8 storage[0x70];
    FrameSetup* setup = (FrameSetup*)storage;
    FrameCounter* counter = (FrameCounter*)storage;
    FrameQueue* queue = (FrameQueue*)storage;
    u32* callbacks = (u32*)0x002647C0u;

    *(u32*)(storage + 0x30) = 0x007F0000u;
    *(u32*)(storage + 0x40) = 0x003F3F3Fu;
    *(u32*)(storage + 0x50) = 0x00003F3Fu;
    *(u32*)(storage + 0x60) = 0x0000003Fu;
    func_00193210(setup, (u32*)(storage + 0x30), (u32*)(storage + 0x40),
                  (u32*)(storage + 0x50), (u32*)(storage + 0x60));
    func_00191E70(1);
    func_00193270(queue);

    s32 value = 0x1A;
    for (s32 i = 0; i < 29; ++i) {
        if (i >= 3) func_00193288(counter, value);

        void (*callback)(void) = (void (*)(void))callbacks[i];
        if (callback != 0) callback();
        --value;
    }

    func_00193288(counter, 0);
    func_00193300(queue);
    func_00191EB0();
}
