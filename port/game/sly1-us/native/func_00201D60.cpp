// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's GS double-buffer setup. The retail
// context is exactly two 0x28-byte display records; the packet buffers are
// separate retail globals and must not be reached by indexing past this one.
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;

extern "C" void func_00201D60(void* context, int x, int width, int height,
                                int interlace, int mode, int extra_buffers) {
    auto* bytes = static_cast<u8*>(context);
    if (bytes == nullptr || width <= 0 || height <= 0) return;
    for (u32 i = 0; i < 0x50; ++i) bytes[i] = 0;

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

    (void)x;
    (void)interlace;
    (void)mode;
    (void)extra_buffers;
}
