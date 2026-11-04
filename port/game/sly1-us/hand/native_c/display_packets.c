// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C backfills for Sly's GS packet writer, translated from US retail.
#include "common.h"

void func_00152888(void* frame) {
    u32* words = (u32*)frame;
    for (s32 i = 0; i < 6; ++i) words[i] = 0;
    words[6] = 0x0024A0E0u;
}

void func_001611B8(void* frame) {
    u32* words = (u32*)frame;
    func_00152888(frame);
    words[5] = 1;
    words[6] = 0x0024A6F8u;
    words[7] = 0;
    words[8] = 0;
    words[9] = 0;
}

void func_001612E8(void* frame) {
    ++((u32*)frame)[9];
}

void func_00161278(void* frame, s32 end_packet) {
    u32* words = (u32*)frame;
    u32 last = words[7];
    if (last == 0) return;

    const u32 register_count = words[9];
    const u32 tag_units = words[8] == 0 ? 0 : register_count / words[8];
    u32 current = words[3];
    if ((current & 0xFu) != 0) {
        *(u64*)current = 0;
        current += 8;
        words[3] = current;
    }

    u32 tag = *(u32*)last;
    if (end_packet) tag |= 0x8000u;
    *(u32*)last = tag | tag_units;
    words[7] = 0;
}

void func_001613C0(void* frame, s32 mode, s32 count, s32 format) {
    u32* words = (u32*)frame;
    if (words[7] != 0) func_00161278(frame, 0);

    func_001612E8(frame);
    u32 start = words[3];
    u64 tag = ((u64)(u32)count << 60) |
              ((u64)0x8000u << 31) |
              ((u64)(u32)mode << 47) |
              ((u64)(u32)format << 60);
    *(u64*)start = tag;
    *(u64*)(start + 8) = (u64)(u32)format;
    words[3] = start + 16;
    words[8] = (u32)count;
    words[7] = start;
    words[9] = 0;
    words[3] += (u32)count * (u32)format * 16u;
}

void func_00161708(void* frame, s32 address, u64 value) {
    u32* words = (u32*)frame;
    func_001612E8(frame);
    u32 current = words[3];
    *(u64*)current = value;
    *(u64*)(current + 8) = (u64)(u32)address;
    words[3] = current + 16;
}

void func_00161200(void* frame) {
    u32* words = (u32*)frame;
    if (words[7] == 0) func_001613C0(frame, 0, 1, 0xE);
    func_00161278(frame, 1);
}

// Retail packet-list finalizer at 0x00152D70. The host packet submitter
// consumes completed DMA tags synchronously, so console cache work is absent.
void func_00152D70(void* self) {
    u32* fields = (u32*)self;
    u32 current = fields[3];

    if (fields[4] != 0) {
        if ((current & 0xFu) != 0) {
            do {
                *(u32*)current = 0;
                current += 4u;
            } while ((current & 0xFu) != 0);
            fields[3] = current;
        }
    } else if (fields[5] != 0) {
        u32 callback_data = fields[6];
        s16 offset = *(s16*)(callback_data + 8u);
        u32 callback = *(u32*)(callback_data + 12u);
        ((void (*)(void*, s32))callback)((u8*)self + offset, 0);
    }

    if (fields[2] != 0) {
        u32* tag = (u32*)fields[2];
        const u32 units = ((fields[3] - fields[2]) >> 4) - 1u;
        tag[0] |= units;
        fields[2] = 0;
    }
}
