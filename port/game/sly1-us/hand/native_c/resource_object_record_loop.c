/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only checked version of the retail counted object-record loop. */
#include "common.h"

s32 func_00137D10(void* reader);
void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id);
s32 func_0011ECA0(s32 value);
void func_00182ED8(void* object);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);

void func_0011ECB8(void* owner, void* parameter, void* reader)
{
    s32 count = func_00137D10(reader);
    s32 i;

    if (count <= 0) return;

    for (i = 0; i < count; ++i) {
        s32 type = func_00137D10(reader);
        s32 mode = func_00137D10(reader);
        s32 listener_id = func_00137D10(reader);
        void* object;
        u32 descriptor;
        u32 method;
        u32 cursor_before;
        s32 index;

        object = func_0011E580(type, owner, parameter, mode, listener_id);
        if (object == 0) {
            sly1_native_trace_font(42u, (u32)owner, (u32)reader,
                *(u32*)((u8*)reader + 0x18), *(u32*)((u8*)reader + 0x1C),
                (u32)type);
            sly1_native_trace_font(41u, (u32)mode, (u32)listener_id,
                (u32)i, (u32)count, (u32)parameter);
            /* Match retail's no-op return from the missing reader callback,
             * while recording the malformed record for diagnosis. */
            (void)sly1_native_call_regs4(0, 0, (u32)reader, 0, 0);
        } else {
            descriptor = *(u32*)object;
            method = descriptor != 0 ? *(u32*)(descriptor + 0x38u) : 0;
            if (method == 0) {
                sly1_native_trace_font(42u, (u32)owner, (u32)reader,
                    *(u32*)((u8*)reader + 0x18), *(u32*)((u8*)reader + 0x1C),
                    (u32)type);
                sly1_native_trace_font(41u, (u32)object, descriptor, method,
                    (u32)i, (u32)count);
                (void)sly1_native_call_regs4(0, (u32)object,
                                             (u32)reader, 0, 0);
            } else {
                cursor_before = *(u32*)((u8*)reader + 0x18u);
                sly1_native_trace_font(81u, (u32)reader, cursor_before,
                    (u32)type, (u32)object, method);
                (void)sly1_native_call_regs4(method, (u32)object,
                                             (u32)reader, 0, 0);
                sly1_native_trace_font(82u, (u32)reader, cursor_before,
                    *(u32*)((u8*)reader + 0x18u),
                    *(u32*)((u8*)reader + 0x1Cu), (u32)type);
            }
        }

        index = func_0011ECA0(mode);
        if (index >= 0)
            *(u32*)((u8*)owner + 0x1CE8u + (u32)index * 4u) = (u32)object;
        func_00182ED8(object);
    }
}
