// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only removal of a C03 body's three pairs of contact-index entries.
// The layout mirrors func_00138828: each 0x1800-byte bank stores 512
// 12-byte {flag, owner, link} records, with one flag-0/flag-1 pair per body.
#include "sly1/host_abi.h"

struct ContactIndexEntry {
    s32 flag;
    void* owner;
    void* link;
};

struct WorldContactIndex {
    u8 pad0[0x38];
    s32 body_count;
    u8 pad3C[0x1AD8 - 0x3C];
    ContactIndexEntry* banks;
};

extern "C" void func_001388A0(WorldContactIndex* world, void* body) {
    if (world == nullptr || body == nullptr || world->banks == nullptr) return;

    const s32 active_count = world->body_count * 2;
    if (active_count <= 0 || active_count > 512) return;

    for (s32 bank_index = 0; bank_index < 3; ++bank_index) {
        ContactIndexEntry* bank = reinterpret_cast<ContactIndexEntry*>(
            reinterpret_cast<u8*>(world->banks) + bank_index * 0x1800);
        ContactIndexEntry* last_flag_zero = nullptr;
        ContactIndexEntry* flag_one = nullptr;

        for (s32 index = 0; index < active_count; ++index) {
            ContactIndexEntry* entry = &bank[index];
            if (entry->owner != body) continue;
            if (entry->flag != 0) {
                flag_one = entry;
                break;
            }
            last_flag_zero = entry;
        }
        if (last_flag_zero == nullptr || flag_one == nullptr) continue;

        // Shift entries between this body's flag-0 and flag-1 records left.
        const s32 gap = static_cast<s32>(flag_one - last_flag_zero);
        if (gap <= 0) continue;
        const u32 intervening_bytes =
            static_cast<u32>(gap - 1) * sizeof(ContactIndexEntry);
        if (intervening_bytes != 0)
            __builtin_memmove(last_flag_zero, last_flag_zero + 1, intervening_bytes);

        const s32 index = static_cast<s32>(flag_one - bank);
        const s32 remaining = active_count - index - 1;
        if (index > 0 && remaining > 0 && index < active_count) {
            __builtin_memmove(flag_one - 1, flag_one + 1,
                              static_cast<u32>(remaining) * sizeof(ContactIndexEntry));
        }
    }
}
