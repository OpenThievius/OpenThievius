// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only worker loop for Sly's queued sector and graphics requests.
#include "common.h"

void func_0015EDB8(void);
void func_0015ED50(void* request);
void func_001684E8(void);
void func_001686E8(s32 channel, void* source);
void func_0015F320(void);
void func_00202C38(s32 channel, s32 a, s32 b);
int openrac_lib_sly1_signal_sema(u32 semaphore);
void func_001F6B60(s32 semaphore);
void func_001683E8(void);

void func_0015EE80(void) {
    func_0015EDB8();
    for (;;) {
        *((s32*)0x002623B8u) = 1;
        func_001F6B60(*((s32*)0x002623B0u));
        *((s32*)0x002623B8u) = 0;

        const u32 index = *((u32*)0x00262314u);
        const u32 next = index + 1u;
        *((u32*)0x00262314u) = next < 2u ? next : 0u;
        const u32 request = 0x00262320u + index * 72u;
        *((u32*)0x0026231Cu) = request;

        func_0015ED50((void*)request);
        func_001684E8();
        func_001686E8(*((s32*)0x00261F18u), (void*)*((u32*)(request + 0x0Cu)));
        *((u32*)0x10003010u) = 0;
        func_001686E8(*((s32*)0x00261F1Cu), (void*)*((u32*)(request + 0x04u)));
        func_0015F320();
        func_00202C38(*((s32*)0x00261F18u), 0, 0);
        (void)openrac_lib_sly1_signal_sema(*((u32*)0x002623B4u));
        func_001683E8();
        *((u32*)0x002626CCu) = 1;
    }
}
