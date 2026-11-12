// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's GS double-buffer setup. The retail
// context is exactly two 0x28-byte display records; the packet buffers are
// separate retail globals and must not be reached by indexing past this one.
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;

extern "C" {
extern u8 D_002624E0[0x10];
extern u8 D_002624F0[0x10];
extern u8 D_00262500[0xD0];
extern u8 D_002625D0[0x10];
extern u8 D_002625E0[0x10];
extern u8 D_002625F0[0xD0];
void sly1_native_trace_display_setup(u32 width, u32 height, u32 context,
                                     u64 scissor1, u64 scissor2);
}

namespace {

void clear(u8* bytes, u32 size) {
    for (u32 i = 0; i < size; ++i) bytes[i] = 0;
}

void build_native_display_packet(u8* packet, u32 width, u32 height) {
    static const u8 registers[8] = {0x4C, 0x4E, 0x18, 0x40, 0x1A, 0x46, 0x45, 0x47};
    const u64 values[8] = {
        0,
        (u64)width | ((u64)height << 32),
        ((u64)0x8000u << 32) | 0x8000u,
        ((u64)(height - 1) << 48) | ((u64)(width - 1) << 16),
        1,
        0,
        0,
        1,
    };

    // Eight packed A+D register writes followed by EOP.
    *(u64*)(packet + 0x00) = 8u | (1ull << 15) | (1ull << 60);
    *(u64*)(packet + 0x08) = 0xEu;
    for (u32 i = 0; i < 8; ++i) {
        *(u64*)(packet + 0x10u + i * 0x10u) = values[i];
        *(u64*)(packet + 0x18u + i * 0x10u) = registers[i];
    }
}

void scatter_display_packet(u8* part0, u8* part1, u8* part2, const u8* packet) {
    for (u32 i = 0; i < 0x10; ++i) {
        part0[i] = packet[i];
        part1[i] = packet[0x10 + i];
    }
    for (u32 i = 0; i < 0x70; ++i) part2[i] = packet[0x20 + i];
}

} // namespace

extern "C" void func_00201D60(void* context, int x, int width, int height,
                                int interlace, int mode, int extra_buffers) {
    auto* bytes = static_cast<u8*>(context);
    if (bytes == nullptr || width <= 0 || height <= 0) return;
    clear(bytes, 0x50);
    clear(D_002624E0, 0x10);
    clear(D_002624F0, 0x10);
    clear(D_00262500, 0xD0);
    clear(D_002625D0, 0x10);
    clear(D_002625E0, 0x10);
    clear(D_002625F0, 0xD0);

    const u32 fbw = ((u32)width + 63u) >> 6;
    u32 page = (fbw * (u32)height + 31u) >> 5;
    if (page == 0) page = 1;
    const u64 display = width == 640 && height == 224
                            ? 0x001BE9FF0183327Cull
                            : (((u64)(u32)(width - 1) & 0xFFFu) << 32) |
                                  (((u64)(u32)(height - 1) & 0x7FFu) << 44);

    for (u32 y = 0; y < 2; ++y) {
        u8* record = bytes + y * 0x28u;
        const u32 base = 0x1400u + y * page;
        *(u64*)(record + 0x00) = 0x66u;
        *(u64*)(record + 0x08) = 2u;
        *(u64*)(record + 0x10) = base;
        *(u64*)(record + 0x18) = display;
        *(u64*)(record + 0x20) = 0;
    }

    u8 packet[0x90]{};
    build_native_display_packet(packet, (u32)width, (u32)height);
    scatter_display_packet(D_002624E0, D_002624F0, D_00262500, packet);
    build_native_display_packet(packet, (u32)width, (u32)height);
    scatter_display_packet(D_002625D0, D_002625E0, D_002625F0, packet);
    sly1_native_trace_display_setup((u32)width, (u32)height,
                                    reinterpret_cast<u32>(bytes),
                                    *(u64*)(D_00262500 + 0x20),
                                    *(u64*)(D_002625F0 + 0x20));

    (void)x;
    (void)interlace;
    (void)mode;
    (void)extra_buffers;
}
