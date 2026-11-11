/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only render drain needed before world memory is recycled. */
#include "common.h"

s32 func_001F6B60(s32 semaphore);
int openrac_lib_sly1_signal_sema(u32 semaphore);
void func_0015ED50(void* frame);
void func_001686E8(s32 dma, void* source);
void func_0015F320(void);
void func_001683E8(void);
void func_0015EFE8(void* frame);

void func_00191EB0(void) {
    *(s32*)0x00269BA4u = 0;
}

void func_0015EF88(void* frame, s32 flag) {
    if (frame == 0) return;
    func_0015ED50(frame);
    func_001686E8(*(s32*)0x00261F18u,
                  (void*)(u32)*(u32*)((u8*)frame + 0x0Cu));
    if (flag != 0) {
        *(volatile s32*)0x10003010u = 0;
        func_001686E8(*(s32*)0x00261F1Cu,
                      (void*)(u32)*(u32*)((u8*)frame + 0x04u));
    }
}

void func_0015F210(s32 flag) {
    volatile s32* render_waiting = (volatile s32*)0x002623B8u;
    void* drawing_frame = (void*)(u32)*(u32*)0x0026231Cu;
    u32 frame;
    s32 index;

    for (index = 0; index < 2; ++index)
        (void)func_001F6B60(*(s32*)0x002623B4u);
    for (index = 0; index < 2; ++index)
        (void)openrac_lib_sly1_signal_sema(*(u32*)0x002623B4u);
    while (*render_waiting == 0) {
    }

    if (flag != 0 && drawing_frame != 0) {
        for (index = 0; index < 3; ++index) {
            func_0015EF88(drawing_frame, 1);
            func_0015F320();
            func_001683E8();
        }
        *(s32*)0x002626CCu = 3;
    }

    for (frame = 0x00262320u; frame < 0x002623B0u; frame += 0x48u)
        func_0015EFE8((void*)frame);
    func_00191EB0();
    *(u32*)0x0026231Cu = 0;
}
