/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only StateWalker listener dispatch, kept separate from decomp source. */
#include "common.h"

typedef struct Sly1StateWalkerCall {
    u32 function;
    u32 object;
    u32 event;
    u32 argument;
    struct Sly1StateWalkerCall* next;
} Sly1StateWalkerCall;

typedef struct Sly1StateWalker {
    u8 pad00[0x1C];
    u32 object;
    u8 pad20[0x1C];
    Sly1StateWalkerCall* calls;
} Sly1StateWalker;

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);

void func_001B6D78(Sly1StateWalker* walker, s32 event, s32 argument)
{
    u32 object = walker->object;
    if (object != 0) {
        u32 descriptor = *(u32*)object;
        u32 callback = descriptor != 0 ? *(u32*)(descriptor + 0x48u) : 0;
        if (callback != 0)
            (void)sly1_native_call_regs4(callback, object, (u32)event,
                                         (u32)argument, 0);
    }

    for (Sly1StateWalkerCall* call = walker->calls; call != 0;
         call = call->next) {
        if (call->function != 0)
            (void)sly1_native_call_regs4(call->function, call->object,
                                         (u32)event, (u32)argument, 0);
    }
}
