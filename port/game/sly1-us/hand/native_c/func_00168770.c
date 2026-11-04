// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfill for Sly's display environment packet builder.
#include "common.h"

void func_001611B8(void* frame);
void func_001529C8(void* frame, s32 qwords, void* output);
void func_001613C0(void* frame, s32 mode, s32 count, s32 format);
void func_00161708(void* frame, s32 address, u64 value);
void func_00161200(void* frame);

void func_00168770(void* output, const u8 color[4], s32 alternate_test) {
    _Alignas(16) u8 frame[0xA0];
    const u32 packed_color = (u32)color[0] | ((u32)color[1] << 8) |
                             ((u32)color[2] << 16) | ((u32)color[3] << 24);
    u64 first = 0x79000000u | 0x6C00u;
    u64 second = 0x87000000u | 0x6E00u;

    func_001611B8(frame);
    func_001529C8(frame, 0x2C, output);
    func_001613C0(frame, 0, 1, 0xE);
    func_00161708(frame, (s8)color[3] < 0 ? 6 : 0x46, 1);
    func_00161708(frame, 1, packed_color);
    func_00161708(frame, 0x47, alternate_test ? 0x32000u : 0x32001u);

    for (s32 offset = 0; offset <= 0x260; offset += 0x20) {
        func_00161708(frame, 5, first);
        func_00161708(frame, 5, second);
        first += 0x200;
        second += 0x200;
    }

    func_00161708(frame, 0x47, 0x70000u);
    func_00161200(frame);
}
