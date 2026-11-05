// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native display-context setup. The matching source still has only
// an address declaration, so keep this replacement separate from the decomp.
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;

namespace {

void build_native_display_packet(u8* context, u32 width, u32 height) {
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
    *(u64*)(context + 0x00) = 8u | (1ull << 15) | (1ull << 60);
    *(u64*)(context + 0x08) = 0xEu;
    for (u32 i = 0; i < 8; ++i) {
        *(u64*)(context + 0x10u + i * 0x10u) = values[i];
        *(u64*)(context + 0x18u + i * 0x10u) = registers[i];
    }
}

} // namespace

extern "C" void func_00201D60(void* context, int x, int width, int height,
                                int interlace, int mode, int extra_buffers) {
    auto* bytes = static_cast<u8*>(context);
    if (bytes == nullptr || width <= 0 || height <= 0) return;
    for (u32 i = 0; i < 0x250; ++i) bytes[i] = 0;

    // Match the hostgen port backfill's NTSC-U display-context layout.
    const u32 fbw = ((u32)width + 63u) >> 6;
    u32 page = (fbw * (u32)height + 31u) >> 5;
    if (page == 0) page = 1;
    const u64 display = width == 640 && height == 224
                            ? 0x001BE9FF0183327Cull
                            : (((u64)(u32)(width - 1) & 0xFFFu) << 32) |
                                  (((u64)(u32)(height - 1) & 0x7FFu) << 44);

    for (u32 y = 0; y < 2; ++y) {
        u8* block = bytes + y * 0x28u;
        const u32 base = 0x1400u + y * page;
        *(u64*)(block + 0x00) = 0x66u;
        *(u64*)(block + 0x08) = 2u;
        *(u64*)(block + 0x10) = base;
        *(u64*)(block + 0x18) = display;
        *(u64*)(block + 0x20) = 0;
    }

    build_native_display_packet(bytes + 0x50, width, height);
    build_native_display_packet(bytes + 0x140, width, height);
    (void)x;
    (void)interlace;
    (void)mode;
    (void)extra_buffers;
}
