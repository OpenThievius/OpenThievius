/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only synchronous replacement for Sly's VIF1 display-list submission. */
#include "sly1/host_abi.h"

extern "C" {
extern s32 D_002BE468;
void func_001F6C20(s32 unused);
void func_00202A80(s32 dma, void* source);
}

extern "C" void func_001686E8(s32 dma, void* source) {
    /* Native DMA completes before the host call returns, so the retail
     * interrupt wait has no event to wait for and must not run. */
    D_002BE468 = 1;
    func_001F6C20(0);
    func_00202A80(dma, source);
}

/* The decompilation exposes this entry with several C++ prototype spellings.
 * Route the signatures used by native callers through the same boundary. */
extern "C" void func_001686E8_i32(s32 dma, s32 source)
    __asm__("_Z13func_001686E8ii");
extern "C" void func_001686E8_i32(s32 dma, s32 source) {
    func_001686E8(dma, reinterpret_cast<void*>(static_cast<u32>(source)));
}

extern "C" void func_001686E8_i32p(s32 dma, s32* source)
    __asm__("_Z13func_001686E8iPi");
extern "C" void func_001686E8_i32p(s32 dma, s32* source) {
    func_001686E8(dma, source);
}

extern "C" void func_001686E8_method(u32 dma, void* source)
    __asm__("_Z13func_001686E8P11Self_1686E8Pv");
extern "C" void func_001686E8_method(u32 dma, void* source) {
    func_001686E8(static_cast<s32>(dma), source);
}
