/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Superseded speculative backfill; retained for history, never compiled. */
#include "common.h"

s32 func_00137D10(void* reader);
void* func_0011E580(s32 slot, void* world, void* parameter, s32 key, s32 flag);
s32 func_0011ECA0(s32 key);
void func_00182ED8(void* object);
u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);

void func_0011ECB8(void* world, void* parameter, void* reader) {
    s32 count = func_00137D10(reader);
    sly1_native_loader_stage(21, (u32)count, (u32)reader);
    while (count-- > 0) {
        /* Some serialized callback records leave a zero pad byte before the
         * next 16-bit type. Only consume it when the current little-endian
         * value is out of the small type range and skipping it reveals one. */
        u8* cursor = (u8*)*(u32*)((u8*)reader + 0x18);
        s32 remaining = *(s32*)((u8*)reader + 0x1C);
        if (remaining >= 3) {
            u32 current = (u32)cursor[0] | ((u32)cursor[1] << 8);
            u32 shifted = (u32)cursor[1] | ((u32)cursor[2] << 8);
            if (current > 0xFFu && cursor[0] == 0 &&
                shifted > 0 && shifted <= 0xFFu) {
                *(u32*)((u8*)reader + 0x18) += 1u;
                *(s32*)((u8*)reader + 0x1C) -= 1;
                sly1_native_loader_stage(36, shifted, (u32)reader);
            }
        }
        s32 slot = func_00137D10(reader);
        s32 key = func_00137D10(reader);
        s32 flag = func_00137D10(reader);
        sly1_native_loader_stage(22, (u32)slot, (u32)reader);
        sly1_native_loader_stage(23, (u32)key, (u32)reader);
        sly1_native_loader_stage(24, (u32)flag, (u32)reader);
        void* object = func_0011E580(slot, world, parameter, key, flag);
        sly1_native_loader_stage(25, (u32)object, (u32)reader);

        if (object == 0) continue;
        sly1_native_loader_stage(26, *(u32*)object, (u32)reader);
        sly1_native_loader_stage(27, *(u32*)((u8*)object + 0x14), (u32)reader);
        sly1_native_loader_stage(28, *(u32*)((u8*)object + 0x18), (u32)reader);
        u32 vtable = *(u32*)object;
        u32 read_method = vtable != 0 ? *(u32*)(vtable + 14u * 4u) : 0;
        sly1_native_loader_stage(34, read_method, (u32)reader);
        if (read_method != 0) {
            if (read_method == 0x0013B598u) {
                /* This C03 reader callback forwards the incoming reader in
                 * a1 even though its current decomp signature omits it. */
                sly1_native_call_regs4(0x001BAC30u, (u32)object,
                                       (u32)reader, 0, 0);
                sly1_native_call_regs4(0x00159478u,
                                       (u32)object + 0x5BCu,
                                       (u32)object, 0, 0);
            } else {
                sly1_native_call_regs4(read_method, (u32)object,
                                       (u32)reader, 0, 0);
            }
        }
        sly1_native_loader_stage(35, read_method, (u32)reader);

        s32 index = func_0011ECA0(key);
        if (index >= 0 && index < 31) {
            *(u32*)((u8*)world + 0x1CE8u + (u32)index * 4u) = (u32)object;
            func_00182ED8(object);
        }
    }
}
