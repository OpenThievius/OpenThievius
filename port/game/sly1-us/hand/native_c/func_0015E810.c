// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only loader request setup translated from the NTSC-U retail routine.
#include "common.h"

void func_00152438(void* self, s32 value);
void func_001F6950(s32 interrupt, void* handler, s32 mode);
void func_001F6DA8(s32 interrupt);
void func_001F6A20(s32 thread_id, void* argument);

struct LoaderSemaParam { u32 attr; s32 maximum; s32 initial; };
int func_001F6B20(struct LoaderSemaParam* params);

void func_0015E810(void) {
    u32 request = 0x00262320u;
    u32 list_0 = 0x002C3B00u;
    u32 list_1 = 0x003C3B00u;
    u32 list_2 = 0x005C3B00u;

    for (u32 i = 0; i < 2; ++i) {
        *((u32*)(request + 4u)) = list_0;
        *((u32*)(request + 12u)) = list_1;
        *((u32*)(request + 16u)) = list_2;
        func_00152438((void*)(request + 0x20u), 0x630);
        func_00152438((void*)(request + 0x2Cu), 0x1090);
        func_00152438((void*)(request + 0x38u), 0xB8);
        request += 0x48u;
        list_0 += 0x80000u;
        list_1 += 0x100000u;
        list_2 += 0x10600u;
    }

    func_00152438((void*)0x002622E0u, 0x630);
    func_00152438((void*)0x002622F0u, 0x1090);
    func_00152438((void*)0x00262300u, 0xB8);
    func_001F6950(5, (void*)0x0016E6A8u, 0);
    func_001F6DA8(5);
    struct LoaderSemaParam sema = {0, 2, 0};
    *((s32*)0x002623B0u) = func_001F6B20(&sema);
    sema.initial = 2;
    *((s32*)0x002623B4u) = func_001F6B20(&sema);

    for (u32 i = 0; i < 256; ++i) {
        u32 value = i;
        for (u32 bit = 0; bit < 8; ++bit)
            value = (value & 1u) ? ((value >> 1) ^ 0xEDB88320u) : (value >> 1);
        *((u32*)(0x002C3B00u + i * 4u)) = value;
    }

    func_001F6A20(*((s32*)0x0027595Cu), (void*)0);
}
