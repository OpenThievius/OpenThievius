// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only C translation of the NTSC-U scene packet builder. The matching
// decompilation still has INCLUDE_ASM(func_001662A8); keep this implementation
// separate so matching C can take over its retail address later.
using u8 = unsigned char;
using u32 = unsigned int;
using s32 = int;
using u64 = unsigned long long;

extern "C" void func_001EE318(void* packet);
extern "C" void func_0018D608(void);
extern "C" void func_00152980(void* packet, s32 qwords);
extern "C" void func_00152B38(void* packet);
extern "C" void func_001EE3B8(void* packet, u32 words, u32 flags);
extern "C" void func_00152D70(void* packet);
extern "C" void func_00152B80(void* packet, s32 lo, s32 hi);
extern "C" void func_00152C18(void* packet, s32 hi);
extern "C" void func_001EE668(void* packet, s32 tag, s32 count,
                               const u32* data, s32 flags);
extern "C" void func_00152CA0(void* packet);
extern "C" void func_00152A50(void* packet, u32* count, u32* begin, u32* out);
extern "C" void func_0018D6F0(void);
extern "C" void func_001EE7D0(void* packet, s32 qwords, void* buffer,
                                s32 flag);

extern "C" u8 D_002623E0[];

static u32 read_word(const void* object, u32 offset) {
    return *reinterpret_cast<const u32*>(static_cast<const u8*>(object) + offset);
}

extern "C" void func_001662A8(void* self, void* entry, void* sub, void*) {
    auto* owner = static_cast<u8*>(self);
    auto* table = static_cast<u8*>(entry);
    auto* output = static_cast<u8*>(sub);
    const s32 primary_count = static_cast<s32>(read_word(table, 0x58));
    const s32 secondary_count = static_cast<s32>(read_word(table, 0x60));
    auto* primary = reinterpret_cast<u8*>(read_word(table, 0x5C));
    auto* secondary = reinterpret_cast<u8*>(read_word(table, 0x64));

    if (read_word(table, 0x28) != 0 || output == nullptr ||
        read_word(owner, 0x40) != 0) return;
    for (s32 i = 0; i < primary_count; ++i) {
        if (read_word(primary + i * 0x50, 0x24) != 0) return;
    }
    for (s32 i = 0; i < secondary_count; ++i) {
        if (read_word(secondary + i * 0x20, 0x04) != 0) return;
    }

    alignas(16) u8 packet[0x30]{};
    u32 packet_count = 0;
    u32* output_count = reinterpret_cast<u32*>(output + 0x18);
    u32* output_buffer = reinterpret_cast<u32*>(output + 0x20);

    func_001EE318(packet);
    for (u32 pass = 0; pass < 2; ++pass) {
        func_0018D608();
        func_00152980(packet, 0x1000);

        u32 unpack_words = primary_count > 0 ? 40u : 532u;
        for (s32 i = 0; i < primary_count; ++i) {
            auto* record = primary + i * 0x50;
            auto* subrecord = output + i * 0x30;
            if (i != 0) func_00152B38(packet);
            func_001EE3B8(packet, unpack_words, 0);
            func_00152D70(packet);

            const auto* kind_ptr = reinterpret_cast<const u8*>(
                read_word(record, 0x18));
            const u32 kind = kind_ptr != nullptr ? kind_ptr[0] : 0xFFFFFFFFu;
            const bool has_extra = read_word(table, 0x54) != 0;

            if (kind == 0) {
                func_00152B80(packet, static_cast<s32>(read_word(record, 0x38)), 0);
                if (has_extra) {
                    func_00152C18(packet, static_cast<s32>(read_word(record, 0x4C)));
                } else {
                    func_00152B80(packet,
                                  static_cast<s32>(read_word(record, 0x40)),
                                  static_cast<s32>(read_word(record, 0x4C)));
                }
                func_00152B80(packet,
                              static_cast<s32>(read_word(subrecord, 0x00)),
                              static_cast<s32>(read_word(subrecord, 0x04)));
                func_00152B80(packet,
                              static_cast<s32>(read_word(subrecord, 0x0C)),
                              static_cast<s32>(read_word(subrecord, 0x10)));
            } else if (kind == 1) {
                if (has_extra) {
                    func_00152C18(packet,
                                  static_cast<s32>(read_word(record, 0x44)));
                } else {
                    func_00152B80(packet,
                                  static_cast<s32>(read_word(record, 0x38)),
                                  static_cast<s32>(read_word(record, 0x44)));
                }
            } else if (kind >= 4 && kind < 11) {
                if (has_extra) {
                    func_00152C18(packet,
                                  static_cast<s32>(read_word(record, 0x44)));
                } else {
                    func_00152B80(packet,
                                  static_cast<s32>(read_word(record, 0x40)),
                                  static_cast<s32>(read_word(record, 0x44)));
                }
            }
        }

        unpack_words = 572u - unpack_words;
        for (s32 i = 0; i < secondary_count; ++i) {
            auto* record = secondary + i * 0x20;
            func_00152B38(packet);
            func_001EE3B8(packet, unpack_words, 0);

            const u32 rgba = static_cast<u32>(D_002623E0[0]) |
                             (static_cast<u32>(D_002623E0[1]) << 8) |
                             (static_cast<u32>(D_002623E0[2]) << 16) |
                             (static_cast<u32>(D_002623E0[3]) << 24);
            const u32 color_words[4] = {
                rgba | 0x00000000u,
                0x00000001u,
                0x00000000u,
                0x00000000u,
            };
            // The retail word carries the 0x8000<<17 selector in the upper
            // half of the 64-bit attribute; preserve it in the adjacent word.
            u64 attribute[2] = { static_cast<u64>(rgba) | (0x8000ULL << 17), 1 };
            (void)color_words;
            func_001EE668(packet, 12, 1,
                          reinterpret_cast<const u32*>(attribute), 26);
            func_00152D70(packet);
            func_00152B80(packet,
                          static_cast<s32>(read_word(record, 0x18)),
                          static_cast<s32>(read_word(record, 0x1C)));
        }

        func_00152CA0(packet);
        func_00152A50(packet, output_count, nullptr, output_buffer);
        func_001EE7D0(packet, 0xBF0 / 16, output_buffer, 0);
        func_0018D6F0();
        ++packet_count;
        if (packet_count < 2) {
            *reinterpret_cast<u32*>(output + 0x1C) =
                *reinterpret_cast<u32*>(output + 0x18);
            *reinterpret_cast<u32*>(output + 0x24) =
                *reinterpret_cast<u32*>(output + 0x20);
        }
        output_count++;
        output_buffer++;
    }
}
