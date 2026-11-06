/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only native text rendering for the retail Sly menu callback. */
#include "common.h"

#define MENU_TABLE_ADDRESS 0x00269D50u
#define MENU_ITEM_STRINGS_ADDRESS 0x0026A068u
#define MENU_PACKET_ADDRESS 0x00262280u

typedef struct SlyMenuText {
    u32 title;
    f32 title_scale;
    f32 item_scale;
    s32 vertical;
    s32 count;
    u32 items;
} SlyMenuText;

typedef struct SlyTextLayout {
    f32 x, y, w, h;
    u32 color;
    s32 horizontal;
    s32 vertical;
} SlyTextLayout;

typedef struct SlyFontForMenu {
    s32 first;
    s32 advance;
    s32 height;
    s32 depth;
    f32 scale_stack[8];
    s32 f30;
    u8 pad34[4];
    u64 f38;
    s32 f40;
    f32 sx;
    f32 sy;
} SlyFontForMenu;

extern void func_0015C8B8(void* font, f32 x, f32 y);
extern void func_0015C900(void* font);
extern void* func_0015DB28(void* output, s32 text, s32 font);
extern f32 func_0015DFF8(void* text_buffer);
extern f32 func_0015E1B0(void* text_buffer, f32 width);
extern void func_0015E318(void* text_buffer, SlyTextLayout* layout,
                          s32 flag, void* packet);
extern void func_00137228(SlyTextLayout* out, f32 x, f32 y);
extern void func_00137238(SlyTextLayout* out, f32 w, f32 h);
extern void func_00137248(SlyTextLayout* out, void* color);
extern void func_00137260(SlyTextLayout* out, s32 horizontal);
extern void func_00137268(SlyTextLayout* out, s32 vertical);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2,
                                  u32 a3);
extern s32 func_0015C188(s32 index);
extern s32 func_0015C1C0(s32 index);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);


static void menu_layout(SlyTextLayout* out, f32 x, f32 y, f32 w, f32 h,
                        u32 color, s32 horizontal) {
    func_00137228(out, x, y);
    func_00137238(out, w, h);
    func_00137248(out, &color);
    func_00137260(out, horizontal);
    func_00137268(out, 0);
}

static u32 rgba(u8 r, u8 g, u8 b, u8 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

char* func_00193598(s32 index) {
    u32 string = *(u32*)(MENU_ITEM_STRINGS_ADDRESS + (u32)index * 4u);
    return (char*)string;
}

void func_00194F80(void* opaque) {
    u8* self = (u8*)opaque;
    const s32 mode = *(s32*)(self + 0x250u);
    const s32 page = *(s32*)(self + 0x260u);
    if (mode == 0 || page < 0 || page >= 33) return;

    u32 font_address = *(u32*)(self + 4u);
    if (font_address == 0) {
        /* Native startup can register fonts after the static UI constructors
         * have run. Bind the same default font when the menu first draws. */
        font_address = *(u32*)0x0026227Cu;
        *(u32*)(self + 4u) = font_address;
    }
    SlyFontForMenu* font = (SlyFontForMenu*)font_address;
    SlyMenuText* menu = (SlyMenuText*)(MENU_TABLE_ADDRESS + (u32)page * 24u);
    sly1_native_trace_font(1, (u32)opaque, (u32)mode, (u32)page,
                           font_address, (u32)menu->count);
    const f32 blend = *(f32*)(self + 0x23Cu);
    const f32 width = *(f32*)(self + 0x234u) * blend;
    const f32 height = *(f32*)(self + 0x238u) * blend;
    const f32 shown_x = *(f32*)(self + 0x224u);
    const f32 shown_y = *(f32*)(self + 0x228u);
    const f32 center_x = shown_x * blend +
                         (shown_x + *(f32*)(self + 0x234u) * 0.5f) * (1.0f - blend);
    f32 cursor_y = shown_y * blend +
                   (shown_y + *(f32*)(self + 0x238u) * 0.5f) * (1.0f - blend);
    const s32 cursor = *(s32*)(self + 0x264u);
    SlyTextLayout layout;
    u8 alpha = (u8)(s32)(*(f32*)(self + 0x284u) * 255.0f);
    if (alpha == 0 && blend > 0.0f) alpha = (u8)(s32)(blend * 255.0f);
    *(f32*)(self + 0x284u) = (f32)alpha * (1.0f / 255.0f);

    /* Retail draws the menu's owning frame before its title and options. */
    u32 owner = *(u32*)(self + 0x210u);
    if (owner == 0 && font_address != 0 && func_0015C188(2) != 0) {
        owner = 0x0026A210u;
        *(u32*)(self + 0x210u) = owner;
        *(u32*)owner = (u32)func_0015C1C0(2);
    }
    if (owner != 0) {
        u32 hook = *(u32*)owner;
        if (hook != 0) {
            u32 vtable = *(u32*)(hook + 0x4Cu);
            if (vtable != 0) {
                const u32 entry = vtable + 9u * 8u;
                const s16 adjustment = *(s16*)entry;
                const u32 draw = *(u32*)(entry + 4u);
                u32 saved = *(u32*)(owner + 0x18u);
                *(u32*)(owner + 0x18u) = (saved & 0x00FFFFFFu) |
                                          ((u32)alpha << 24);
                menu_layout(&layout, center_x, cursor_y, width, height,
                            0xFF808080u, 0);
                (void)sly1_native_call_regs4(
                    draw, hook + (s32)adjustment, owner, (u32)&layout,
                    MENU_PACKET_ADDRESS);
                *(u32*)(owner + 0x18u) = saved;
            }
        }
    }

    /* The native font table is populated by the level resource loader. */
    if (font_address == 0) return;

    u8 scratch[0xB0];
    if (menu->title != 0) {
        f32 scale = menu->title_scale * blend;
        func_0015C8B8(font, scale, scale);
        func_0015DB28(scratch, (s32)menu->title, (s32)font_address);
        menu_layout(&layout, center_x, cursor_y, width, height,
                    rgba(0x7F, 0x7F, 0x7F, alpha), 1);
        func_0015E318(scratch, &layout, 0, (void*)MENU_PACKET_ADDRESS);
        cursor_y += func_0015E1B0(scratch, 500.0f);
        func_0015C900(font);
    }

    const f32 item_scale = menu->item_scale * blend;
    func_0015C8B8(font, item_scale, item_scale);
    const f32 line_height = (f32)font->height * font->sy;
    const s32* items = (const s32*)menu->items;
    f32 x = center_x;
    if (menu->vertical == 0) {
        f32 total = 0.0f;
        for (s32 i = 0; i < menu->count; ++i) {
            if (i != 0) total += (f32)font->advance * font->sx * 2.0f;
            func_0015DB28(scratch, (s32)func_00193598(items[i]), (s32)font_address);
            total += func_0015DFF8(scratch);
        }
        x = center_x + (width - total) * 0.5f;
    }

    for (s32 i = 0; i < menu->count; ++i) {
        u8 red = 0x5F, green = 0x5F, blue = 0x5F;
        f32 grow = 1.0f;
        if (i == cursor) {
            red = green = 0x6F;
            blue = 0x4F;
            grow = 1.05f;
        }
        const u32 color = rgba(red, green, blue, alpha);
        char* text = func_00193598(items[i]);
        func_0015DB28(scratch, (s32)text, (s32)font_address);
        f32 measured_width = func_0015DFF8(scratch);
        if (menu->vertical != 0) x = center_x + (width - measured_width) * 0.5f;

        const f32 scale = item_scale * grow;
        func_0015C8B8(font, scale, scale);
        func_0015DB28(scratch, (s32)text, (s32)font_address);
        const f32 scaled_width = func_0015DFF8(scratch);
        const f32 scaled_height = (f32)font->height * font->sy;
        menu_layout(&layout, x + (measured_width - scaled_width) * 0.5f,
                    cursor_y + (line_height - scaled_height) * 0.5f,
                    width, scaled_height, color, 0);
        func_0015E318(scratch, &layout, 0, (void*)MENU_PACKET_ADDRESS);
        func_0015C900(font);
        if (menu->vertical != 0) cursor_y += (f32)font->height * font->sy;
        else x += measured_width + (f32)font->advance * font->sx * 2.0f;
    }
    func_0015C900(font);
}
