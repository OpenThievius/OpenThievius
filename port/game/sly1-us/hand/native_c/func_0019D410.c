/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only deferred draw-request queue reconstructed from NTSC-U retail. */
#include "common.h"

#define SLY1_DEFERRED_REQUEST_BASE 0x005E4700u
#define SLY1_DEFERRED_REQUEST_SIZE 0x80u
#define SLY1_DEFERRED_REQUEST_CAPACITY 0x400u

void func_0019D410(void* request_pointer)
{
    const u32 request = (u32)request_pointer;
    const u32 cursor_address = 0x0026AC10u;
    u32 cursor = *(u32*)cursor_address;
    const u32 end = SLY1_DEFERRED_REQUEST_BASE +
                    SLY1_DEFERRED_REQUEST_SIZE * SLY1_DEFERRED_REQUEST_CAPACITY;

    if (request == 0 || cursor < SLY1_DEFERRED_REQUEST_BASE || cursor >= end ||
        ((cursor - SLY1_DEFERRED_REQUEST_BASE) % SLY1_DEFERRED_REQUEST_SIZE) != 0)
        return;

    *(u32*)cursor_address = cursor + SLY1_DEFERRED_REQUEST_SIZE;
    __builtin_memcpy((void*)cursor, (const void*)request,
                     SLY1_DEFERRED_REQUEST_SIZE);
}
