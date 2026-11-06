/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only state notifier constructor translated from US retail. */
#include "common.h"

void func_00182C18(void* object);

void func_001DFD90(void* object)
{
    u8* self = (u8*)object;
    const u8* defaults = (const u8*)0x002583D0u;

    func_00182C18(object);
    __builtin_memcpy(self + 0x50, defaults, 16);
    __builtin_memcpy(self + 0x60, defaults + 16, 16);
    __builtin_memcpy(self + 0x70, defaults + 32, 16);
    *(s32*)(self + 0x88) = 30;
    *(f32*)(self + 0x8C) = 25.0f;
}
