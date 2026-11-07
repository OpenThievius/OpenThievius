/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only C for async startup objects absent from matching decomp C. */
#include "common.h"

s32 func_001E2268(s32 attributes, s32 option);
void func_001F5C0C(void* destination, s32 value, s32 byte_count);
int func_001F6A10(u32 parameters);
int func_001F6A20(s32 thread, u32 argument);
int func_001F6B40(s32 semaphore);
int func_001F6B60(s32 semaphore);
void func_0018B720(void* object);
void func_0018C178(void* object, s32 value);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_0018B600(void)
{
    u8* queue = (u8*)0x00269990u;
    const s32 semaphore = *(s32*)(queue + 0x00);
    u32 callback;
    u32 argument;
    s32 result;

    (void)func_001F6B60(semaphore);
    callback = *(u32*)(queue + 0x08);
    argument = *(u32*)(queue + 0x0C);
    result = (s32)sly1_native_call_regs4(callback, argument, 0, 0, 0);
    *(s32*)(queue + 0x10) = result;
    *(s32*)0x00275960u = -1;
    *(s32*)(queue + 0x04) = -1;
    *(u32*)(queue + 0x08) = 0;
    *(u32*)(queue + 0x0C) = 0;
    (void)func_001F6B40(semaphore);
}

void func_0018B5C0(void* object)
{
    u8* bytes = (u8*)object;

    *(s32*)(bytes + 0x00) = func_001E2268(1, 1);
    *(s32*)(bytes + 0x04) = -1;
    *(s32*)(bytes + 0x10) = 0x22;
}

void func_0018BD58(void* object, s32 first, s32 second)
{
    u8* bytes = (u8*)object;

    (void)func_001F5C0C(bytes, 0, 0x4F50);
    *(s32*)(bytes + 0x14) = 0;
    *(s32*)(bytes + 0x0C) = -1;
    func_0018C178(object, 0);
    *(s32*)(bytes + 0x04) = first;
    *(s32*)(bytes + 0x08) = second;
    *(s32*)(bytes + 0x4F44) = 0;
    *(s32*)(bytes + 0x4F48) = 0;
}

void func_0018B670(void* object, void* callback, void* argument)
{
    u8 parameters[0x30];
    u8* bytes = (u8*)object;
    s32 thread;

    (void)func_001F5C0C(parameters, 0, sizeof(parameters));
    func_0018B720(object);
    *(u32*)(bytes + 0x08) = (u32)callback;
    *(u32*)(bytes + 0x0C) = (u32)argument;
    *(u32*)(bytes + 0x10) = 0x20;

    *(u32*)(parameters + 0x04) = 0x0018B600u;
    *(u32*)(parameters + 0x08) = 0x002BEA10u;
    *(u32*)(parameters + 0x0C) = 0x4000u;
    *(u32*)(parameters + 0x10) = 0x002832F0u;
    *(u32*)(parameters + 0x14) = 3u;
    thread = func_001F6A10((u32)parameters);
    *(s32*)(bytes + 0x04) = thread;
    *(s32*)0x00275960u = thread;
    (void)func_001F6A20(thread, (u32)object);
}
