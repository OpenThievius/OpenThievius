/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only display setup while the corresponding retail functions lack
 * matching C. Keep the mapping separate so matching decomp C replaces it. */
#include "common.h"

s32 sly1_native_gs_privileged_store64(u32 address, u64 value);
s32 openrac_guest_ee_mmio_store32(u32 address, u32 value);

static void write_privileged(u32 address, u64 value)
{
    (void)sly1_native_gs_privileged_store64(address, value);
}

static void build_display_packet(u8* context, s32 width, s32 height)
{
    static const u8 registers[8] = {0x4C, 0x4E, 0x18, 0x40, 0x1A, 0x46, 0x45, 0x47};
    u64 values[8];
    u64 tag;
    u32 i;

    if (context == 0 || width <= 0 || height <= 0) return;
    values[0] = 0; /* BITBLTBUF: no image transfer during display setup. */
    values[1] = (u64)(u32)width | ((u64)(u32)height << 32);
    values[2] = ((u64)0x8000u << 32) | 0x8000u;
    values[3] = ((u64)(u32)(height - 1) << 48) |
                ((u64)(u32)(width - 1) << 16);
    values[4] = 1;
    values[5] = 0;
    values[6] = 0;
    values[7] = 1;

    /* Eight packed A+D writes, one register per loop, followed by EOP. */
    tag = 8u | (1ull << 15) | (1ull << 60);
    *(u64*)(context + 0x00) = tag;
    *(u64*)(context + 0x08) = 0xEu;
    for (i = 0; i < 8; ++i) {
        *(u64*)(context + 0x10u + i * 0x10u) = values[i];
        *(u64*)(context + 0x18u + i * 0x10u) = registers[i];
    }
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

    /* The retail helpers are still nonmatching. This port backfill emits the
     * same eight-register setup as a valid packed A+D packet for the native GS. */
    build_display_packet(bytes + 0x50, width, height);
    build_display_packet(bytes + 0x140, width, height);

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

void func_00201C78(void* context)
{
    u32 address = (u32)context;
    u32 qwc;
    u64 header;

    if (context == 0) return;
    header = *(u64*)context;
    qwc = (u32)(header & 0x7FFFu) + 1u;
    if ((address & 0x70000000u) == 0x70000000u) {
        address = (address & 0x0FFFFFFFu) | 0x80000000u;
    } else {
        address &= 0x0FFFFFFFu;
    }

    /* The guest DMA register page is ordinary mapped memory; only CHCR starts
     * the transfer through the host's synchronous GIF DMA implementation. */
    *(volatile u32*)0x1000E000u = 1u;
    *(volatile u32*)0x1000A020u = qwc;
    *(volatile u32*)0x1000A010u = address;
    if (openrac_guest_ee_mmio_store32(0x1000A000u, 0x101u) == 0) return;
}

void func_00201FF8(void* context, s32 page)
{
    u8* bytes = (u8*)context;
    u32 selected = (u32)page & 1u;
    if (bytes == 0) return;
    func_00201BB8(bytes + selected * 0x28u);
    func_00201C78(bytes + (selected ? 0x140u : 0x50u));
}

/* Native display-frame callbacks. The host owns GS timing, so alternate the
 * field locally while preserving Sly's double-buffer state and packet setup. */
extern s32 D_002BE470, D_002BE474, D_00262484, D_002626C0;
extern s32 D_002626C4, D_002626C8, D_002626CC;
void func_00202098(void* address, s32 source_size, s32 destination_size, s32 field);
void func_001F6C20(s32 channel);
void func_00168130(void);
void func_00201988(s32 a, s32 b);
void func_001683B8(void);

void func_001683E8(void)
{
    D_002BE470 = 1;
    func_001683B8();
    s32 pending = D_00262484;
    D_002626C8 = D_002626C4;
    D_002BE470 = 0;
    if (pending > 0) {
        D_002626C4 = 0;
        D_00262484 = pending - 1;
    } else {
        D_002626C4 ^= 1;
    }

    const s32 field_buffer = (D_002626C0 & 1) != 0
        ? 0x002625E0u : 0x002624F0u;
    func_00202098((void*)field_buffer, 0x800, 0x800, D_002626C4);
    func_001F6C20(0);
    const s32 page = D_002626C0 + 1;
    D_002626C0 = page;
    func_00201FF8((void*)0x00262490u, page);

    const s32 state = D_002626CC;
    if (state == 1 || state == 2) func_00168130();
    func_00201988(0, 0);
}

void func_001684E8(void)
{
    if (D_002626CC == 3)
        D_002626CC = 2;
    if (D_002BE474 == 0) return;

    D_002BE474 = 0;
    D_002626C4 ^= 1;
    const s32 current = D_002626C0;
    const s32 field_buffer = (current & 1) != 0
        ? 0x002625E0u : 0x002624F0u;
    func_00202098((void*)field_buffer, 0x800, 0x800, D_002626C4);
    func_001F6C20(0);
    const s32 packet = (current & 1) != 0
        ? 0x002625D0u : 0x002624E0u;
    func_00201C78((void*)packet);
}
