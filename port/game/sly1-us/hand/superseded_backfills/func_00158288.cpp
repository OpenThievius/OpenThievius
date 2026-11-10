// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only scalar translation of the retail VU0 relationship updater.
// Keep this outside matching sources so a future decompilation replaces it.
using u8 = unsigned char;
using u32 = unsigned int;
using s32 = int;
using f32 = float;

extern "C" void* func_0015A418(void* world, s32 type, s32 id, void* owner);

static u32 read_u32(const void* base, u32 offset) {
    return *reinterpret_cast<const u32*>(static_cast<const u8*>(base) + offset);
}

static f32 read_f32(const void* base, u32 offset) {
    return *reinterpret_cast<const f32*>(static_cast<const u8*>(base) + offset);
}

static void write_f32(void* base, u32 offset, f32 value) {
    *reinterpret_cast<f32*>(static_cast<u8*>(base) + offset) = value;
}

extern "C" void func_00158288(void* record, void* owner) {
    auto* rec = static_cast<u8*>(record);
    auto* self = static_cast<u8*>(owner);

    if (read_u32(rec, 0x120) == 0) {
        const s32 target_id = static_cast<s32>(read_u32(rec, 0x188));
        if (target_id != -1) {
            void* target = func_0015A418(
                reinterpret_cast<void*>(read_u32(self, 0x14)), 0x104,
                target_id, self);
            *reinterpret_cast<void**>(rec + 0x18C) = target;
        }
    }

    if (static_cast<s32>(read_u32(rec, 0x10)) != 3) return;

    const s32 count = static_cast<s32>(read_u32(rec, 0x20));
    auto* entries = reinterpret_cast<u8*>(read_u32(rec, 0x24));
    f32 total = 0.0f;
    for (s32 i = 0; i < count; ++i) {
        auto* entry = entries + i * 0x28;
        const s32 first_id = static_cast<s32>(read_u32(entry, 0));
        const s32 second_id = static_cast<s32>(read_u32(entry, 4));
        void* first = func_0015A418(
            reinterpret_cast<void*>(read_u32(self, 0x14)), 0x104,
            first_id, self);
        void* second = func_0015A418(
            reinterpret_cast<void*>(read_u32(self, 0x14)), 0x104,
            second_id, self);
        *reinterpret_cast<void**>(entry + 0x18) = first;
        *reinterpret_cast<void**>(entry + 0x1C) = second;

        const f32 dx = read_f32(first, 0x140) - read_f32(second, 0x140);
        const f32 dy = read_f32(first, 0x144) - read_f32(second, 0x144);
        const f32 dz = read_f32(first, 0x148) - read_f32(second, 0x148);
        const f32 inverse_distance_squared =
            1.0f / (dx * dx + dy * dy + dz * dz);
        const f32 scale = (read_f32(entry, 8) + read_f32(entry, 12)) * 0.5f;
        const f32 weight = scale * inverse_distance_squared;
        write_f32(entry, 0x20, inverse_distance_squared);
        write_f32(entry, 0x24, weight);
        total += weight;
    }
    write_f32(rec, 0x28, total);
}
