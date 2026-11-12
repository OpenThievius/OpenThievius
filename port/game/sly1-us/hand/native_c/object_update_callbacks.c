/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only copy of the current decomp C for placed-object callback dispatch. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_001261C0(void* object, void* camera, void* matrix) {
    u8* self = (u8*)object;
    u32 vtable = *(u32*)self;
    u32 m2c = vtable != 0 ? *(u32*)(vtable + 0xC0) : 0;
    if (m2c != 0)
        (void)sly1_native_call_regs4(m2c, (u32)object, 0, 0, 0);

    u32 record = *(u32*)(self + 0x224);
    if (record != 0 && ((*(u32*)(record + 0xB0) & 0x80u) != 0)) {
        if (matrix != 0) {
            *(u32*)(record + 0xA4) = 1;
            __builtin_memcpy((void*)(record + 0x60), matrix, 0x40);
        } else {
            *(u32*)(record + 0xA4) = 0;
        }
    }

    u32 m26 = vtable != 0 ? *(u32*)(vtable + 0xA8) : 0;
    sly1_native_trace_font(202u, (u32)object, (u32)camera, (u32)matrix, m26,
                           0u);
    if (m26 != 0)
        (void)sly1_native_call_regs4(m26, (u32)object, (u32)camera,
                                     (u32)matrix, 0);
}
