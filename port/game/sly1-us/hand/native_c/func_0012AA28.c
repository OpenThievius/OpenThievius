/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only state notifier object lookup and setter from US retail. */
#include "common.h"

void* func_0015A418(void* world, s32 mode, s32 id, void* owner);
void* func_0011E580(s32 type, void* world, void* parameter,
                    s32 mode, s32 listener_id);

static void* state_object(void* object)
{
    u8* self = (u8*)object;
    const u32 world = *(u32*)(self + 0x14);
    void* found;
    if (world == 0) return 0;

    found = func_0015A418((void*)world, 0x102, 554, object);
    if (found == 0)
        found = func_0011E580(117, (void*)world, object, 554, -1);
    return found;
}

void func_0012AA28(void* object, s32 value)
{
    void* state = state_object(object);
    if (state != 0) *(s32*)((u8*)state + 0x90) = value;
}
