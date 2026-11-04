/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only display setup while the corresponding retail functions lack
 * matching C. Keep the mapping separate so matching decomp C replaces it. */
#include "common.h"

s32 sly1_native_gs_privileged_store64(u32 address, u64 value);

static void write_privileged(u32 address, u64 value)
{
    (void)sly1_native_gs_privileged_store64(address, value);
}

void func_00201D60(void* context, s32 x, s32 width, s32 height,
                   s32 interlace, s32 mode, s32 extra_buffers)
{
    u8* bytes = (u8*)context;
    u64 display;
    u32 fbw;
    u32 page;
    u32 y;
    u32 i;

    if (bytes == 0 || width <= 0 || height <= 0) return;
    for (i = 0; i < 0x250; ++i) bytes[i] = 0;

    /* Retail func_00202140's NTSC-U mode: 64-pixel framebuffer stride,
     * 640x224 display rectangle, and a double-buffered frame allocation. */
    fbw = ((u32)width + 63u) >> 6;
    page = (fbw * (u32)height + 31u) >> 5;
    if (page == 0) page = 1;
    if (width == 640 && height == 224) {
        /* NTSC-U display mode values observed in the retail register trace. */
        display = 0x001BE9FF0183327Cull;
    } else {
        display = (((u64)(u32)(width - 1) & 0xFFFu) << 32) |
                  (((u64)(u32)(height - 1) & 0x7FFu) << 44);
    }

    for (y = 0; y < 2; ++y) {
        u8* block = bytes + y * 0x28u;
        u32 base = 0x1400u + y * page;
        *(u64*)(block + 0x00) = 0x66u;
        *(u64*)(block + 0x08) = 2u;
        *(u64*)(block + 0x10) = base;
        *(u64*)(block + 0x18) = display;
        *(u64*)(block + 0x20) = 0;
    }

    /* Retail submits the selected 0x28-byte block from func_00201FF8. Keep
     * the block data source authoritative instead of writing registers here. */
    (void)x;
    (void)interlace;
    (void)mode;
    (void)extra_buffers;
}

void func_00201BB8(void* context)
{
    u8* bytes = (u8*)context;
    if (bytes == 0) return;
    write_privileged(0x12000000u, *(u64*)(bytes + 0x00));
    write_privileged(0x12000070u, *(u64*)(bytes + 0x10));
    write_privileged(0x12000080u, *(u64*)(bytes + 0x18));
}

void func_00201FF8(void* context, s32 page)
{
    u8* bytes = (u8*)context;
    u32 selected = (u32)page & 1u;
    if (bytes == 0) return;
    func_00201BB8(bytes + selected * 0x28u);
}
