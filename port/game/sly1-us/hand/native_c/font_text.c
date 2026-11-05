/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly font stack and text cursor backfills. */
#include "common.h"

typedef struct FontTextFont {
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
    u32 vtable;
    s32 id;
    u32 image_a;
    u32 image_b;
    u8 pad5c[4];
    u64 tex0;
    s32 f68;
    s32 f6c;
    s32 f70;
    s32 f74;
    s32 glyph_count;
    u32 glyphs;
    u32 mask;
} FontTextFont;

typedef struct FontTextBuffer {
    s32 text_address;
    u32 cursor;
    u32 font;
    u32 default_font;
    u8 inline_font[0x88];
    s32 font_changed;
    union { u32 packed; u8 rgba[4]; } color_9c;
    union { u32 packed; u8 rgba[4]; } color_a0;
    union { u32 packed; u8 rgba[4]; } color_a4;
    union { u32 packed; u8 rgba[4]; } color_a8;
} FontTextBuffer;

typedef struct FontGlyph {
    u16 key;
    u16 u;
    u16 v;
    u16 width;
} FontGlyph;

typedef struct FontPacketWriter {
    u32 end;
    u32 begin;
    u32 dma_tag;
    u32 cursor;
    s32 pad;
    s32 use_close_callback;
    u32 vtable;
    u32 last_tag;
    u32 registers_per_loop;
    u32 register_count;
} FontPacketWriter;

typedef struct FontImageA {
    u32 words[8];
} FontImageA;

typedef struct FontImageB {
    u32 words[6];
} FontImageB;

typedef struct FontDisplayState {
    u8 pad[0x18];
    u32 flags;
} FontDisplayState;

typedef struct FontTextLayout {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
    u32 color;
    s32 horizontal;
    s32 vertical;
} FontTextLayout;

extern u32 D_00262268[];
extern s32 D_0026227C;
extern s8 D_00262250[];
extern u8 D_0024A220[];
extern FontDisplayState* D_002622D8;
FontGlyph* func_0015DAB8(void* opaque, s32 key);
void func_001613C0(void* packet, s32 mode, s32 count, s32 format);
void func_00161708(void* packet, s32 address, u64 value);
void func_00161768(void* packet, u32 register_count, u64 value);
void func_001617E8(void* packet, u32 register_id, f32 value);
void func_00161860(void* packet, s32 first, s32 second);
void func_001618C0(void* packet, s32 first, s32 second, u32 third, u32 fourth);
void func_00152D70(void* packet);
void func_00152B38(void* packet);
void func_00152B80(void* packet, s32 low, s32 high);

static u32 font_vtable_method(FontTextFont* font, u32 method_offset,
                              u32 adjust_offset, u32* adjusted) {
    u32 vtable = font->vtable;
    s16 adjustment = *(s16*)(vtable + adjust_offset);
    *adjusted = (u32)((u8*)font + adjustment);
    return *(u32*)(vtable + method_offset);
}
extern f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2,
                                u32 a3, f32 f12, f32 f14);
extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2,
                                  u32 a3);

typedef struct FontLegacyLayout {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
    s32 color;
    s32 horizontal;
    s32 vertical;
} FontLegacyLayout;

/* Port-only implementation of the retail V24A2 text loop. This is separate
 * from decomp source so it can be replaced when the matching unit is ready. */
void func_0015C638(void* opaque, char* string,
                   FontLegacyLayout* layout, s32 flags, u32 packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    char* p;
    char* next;
    s32 pending = 1;
    f32 x = 0.0f;
    f32 y;
    u32 adjusted;
    u32 method;

    if (font == 0 || string == 0 || layout == 0) return;
    method = font_vtable_method(font, 0x24u, 0x20u, &adjusted);
    (void)sly1_native_call_regs4(method, adjusted, (u32)flags, packet, 0);

    if (layout->vertical == 0) {
        y = layout->y;
    } else {
        f32 height = (f32)font->height * font->sy;
        for (p = string; *p != 0; ++p)
            if (*p == '\n') height += (f32)font->height * font->sy;
        y = layout->vertical == 2
            ? layout->y + layout->h - height
            : layout->y + (layout->h - height) * 0.5f;
    }

    for (p = string; *p != 0; p = next) {
        const u8 character = (u8)*p;
        next = p + 1;
        if (character == '\n') {
            pending = 1;
            y += (f32)font->height * font->sy;
            continue;
        }

        if (pending != 0) {
            if (layout->horizontal == 0) {
                x = layout->x;
            } else {
                char* q;
                f32 line_width = 0.0f;
                method = font_vtable_method(font, 0x1Cu, 0x18u, &adjusted);
                for (q = p; *q != 0 && *q != '\n'; ++q)
                    line_width += sly1_native_call_f32(
                        method, adjusted, (u32)(u8)*q, 0, 0, 0.0f, 0.0f);
                x = layout->horizontal == 2
                    ? layout->x + layout->w - line_width
                    : layout->x + (layout->w - line_width) * 0.5f;
            }
            pending = 0;
        }

        method = font_vtable_method(font, 0x34u, 0x30u, &adjusted);
        x += sly1_native_call_f32(method, adjusted, (u32)character,
                                  (u32)layout->color, packet, x, y);
    }

    method = font_vtable_method(font, 0x3Cu, 0x38u, &adjusted);
    (void)sly1_native_call_regs4(method, adjusted, (u32)flags, packet, 0);
}

void func_0015C8B8(void* opaque, f32 sx, f32 sy) {
    FontTextFont* font = (FontTextFont*)opaque;
    f32* point = (f32*)((u8*)font + 0x10u + (u32)font->depth * 8u);
    point[0] = font->sx;
    point[1] = font->sx;
    ++font->depth;
    font->sx = *(f32*)((u8*)font + 0x10u) * sx;
    font->sy = *(f32*)((u8*)font + 0x14u) * sy;
}

void func_0015C900(void* opaque) {
    FontTextFont* font = (FontTextFont*)opaque;
    if (font->depth <= 0) return;
    --font->depth;
    font->sx = *(f32*)((u8*)font + 0x10u + (u32)font->depth * 8u);
    font->sy = *(f32*)((u8*)font + 0x14u + (u32)font->depth * 8u);
}

void func_0015C260(void* source, void* destination) {
    FontTextFont* from = (FontTextFont*)source;
    FontTextFont* to = (FontTextFont*)destination;
    to->first = from->first;
    to->advance = from->advance;
    to->height = from->height;
    to->sx = from->sx;
    to->sy = from->sy;
    to->depth = from->depth;
    for (u32 index = 0; index < (u32)from->depth * 2u; ++index)
        ((f32*)to->scale_stack)[index] = ((f32*)from->scale_stack)[index];
    to->f30 = from->f30;
    to->f38 = from->f38;
    to->f40 = from->f40;
}

void func_0015C9B0(void* source, void* destination) {
    func_0015C260(source, destination);
}

s32 func_0015C1C0(s32 index) {
    if (index == -1) return 0;
    return D_00262268[index] != 0 ? D_00262268[index] : D_0026227C;
}

s32 func_0015C200(s32 key) {
    s32 index;
    for (index = 0; index < 5; ++index)
        if ((s8)key == D_00262250[index]) return D_00262268[index];
    return 0;
}

FontGlyph* func_0015DAB8(void* opaque, s32 key) {
    FontTextFont* font = (FontTextFont*)opaque;
    s32 low = 0;
    s32 high = font->glyph_count - 1;
    while (low <= high) {
        s32 middle = low + (high - low) / 2;
        FontGlyph* glyph = (FontGlyph*)(font->glyphs + (u32)middle * 8u);
        s32 delta = (u8)key - glyph->key;
        if (delta == 0) return glyph;
        if (delta > 0) low = middle + 1;
        else high = middle - 1;
    }
    return 0;
}

f32 func_0015CA60(void* opaque, s32 key) {
    FontTextFont* font = (FontTextFont*)opaque;
    FontGlyph* glyph = func_0015DAB8(opaque, key);
    return glyph == 0 ? (f32)font->advance * font->sx
                      : (f32)(glyph->width + 1u) * font->sx;
}

void func_0015D338(void* opaque, u32 color, f32 x0, f32 y0, f32 x1,
                   f32 y1, f32 u0, f32 v0, f32 u1, f32 v1,
                   void* packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    color = (color & 0xFFFFFFu) | ((((color >> 24) + 1u) & 0x1FEu) << 23);
    func_001617E8(packet, color, 0.0f);
    func_00161860(packet, (s32)(u0 * 16.0f), (s32)(v0 * 16.0f));
    func_001618C0(packet, (s32)((x0 + 1728.0f) * 16.0f),
                  (s32)((y0 * 0.45454547f + 1936.0f) * 16.0f),
                  (u32)font->f40, 0);
    func_00161860(packet, (s32)(u1 * 16.0f), (s32)(v1 * 16.0f));
    func_001618C0(packet, (s32)((x1 + 1728.0f) * 16.0f),
                  (s32)((y1 * 0.45454547f + 1936.0f) * 16.0f),
                  (u32)font->f40, 0);
}

s32 func_0015CAB8(void* opaque, void* packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    s32 changed = (D_002622D8->flags & (u32)font->mask) == 0;
    if (changed) {
        FontImageA* image_a = (FontImageA*)font->image_a;
        FontImageB* image_b = (FontImageB*)font->image_b;
        func_00152D70(packet);
        func_00152B80(packet, font->f68, font->f6c);
        func_00152B80(packet, (s32)image_a->words[5], (s32)image_a->words[4]);
        if (image_b != 0) {
            func_00152B80(packet, font->f70, font->f74);
            func_00152B80(packet, (s32)image_b->words[3], (s32)image_b->words[2]);
        }
        func_00152B38(packet);
        D_002622D8->flags |= (u32)font->mask;
    }
    return changed;
}

void func_0015C2E8(void* unused, void* rectangle, void* packet) {
    if (rectangle == 0) return;
    f32* rect = (f32*)rectangle;
    s32 width = (s32)(rect[0] + rect[2]) - 1;
    s32 bottom = (s32)((rect[1] + rect[3]) * 0.454545468f) - 1;
    s32 top = (s32)(rect[1] * 0.454545468f);
    s32 left = (s32)rect[0];
    func_001613C0(packet, 0, 1, 0xE);
    u64 value = (u64)(u32)left | ((u64)(u32)width << 16) |
                ((u64)(u32)top << 32) | ((u64)(u32)bottom << 48);
    func_00161708(packet, 0x40, value);
}

void func_0015CB88(void* opaque, s32 rectangle, void* packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    func_0015C2E8(font, (void*)(u32)rectangle, packet);
    s32 changed = func_0015CAB8(font, packet);
    func_001613C0(packet, 6, 1, 0xE);
    if (changed) func_00161708(packet, 0x3F, 0);
    func_00161708(packet, 0x47, font->f30 != 0 ? font->f38 : 0x31001u);
    func_00161708(packet, 0x42, 0x44);
    func_00161708(packet, 6, font->tex0);
    func_00161708(packet, 0x14, 0x60);
    func_00161708(packet, 8, 5);
    func_00161708(packet, 0, 0x1D6);
    func_00161768(packet, 5, 0x43431);
}

void func_0015CC80(void* opaque, void* packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    s32 changed = func_0015CAB8(font, packet);
    func_001613C0(packet, 6, 1, 0xE);
    if (changed) func_00161708(packet, 0x3F, 0);
    func_00161708(packet, 6, font->tex0);
    func_00161708(packet, 0, 0x1D6);
    func_00161768(packet, 5, 0x43431);
}

void func_0015C3C0(void* unused, s32 flush, void* packet) {
    if (flush) {
        func_001613C0(packet, 0, 1, 0xE);
        func_00161708(packet, 0x40, 0xDF0000027F0000ULL);
    }
}

void func_0015DBB8(void* opaque, void* color) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    text->color_a4.packed = *(u32*)color;
    text->color_9c.packed = *(u32*)color;
}

void func_0015E318(void* opaque, FontTextLayout* layout, s32 flag,
                   void* packet) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    f32 line_widths[64] = {0};
    s32 line = 0;
    s32 first = 1;
    s32 begun = 0;
    f32 top;
    f32 left = 0.0f;
    s32 value;

    func_0015DB80(text);
    while ((value = func_0015DBE0(text)) != 0) {
        if (value == 10) {
            ++line;
        } else {
            FontTextFont* font = (FontTextFont*)text->font;
            u32 adjusted;
            u32 callback = font_vtable_method(font, 0x1Cu, 0x18u, &adjusted);
            line_widths[line] += sly1_native_call_f32(
                callback, adjusted, (u32)value, 0, 0, 0.0f, 0.0f);
        }
    }

    if (layout->vertical == 0) {
        top = layout->y;
    } else {
        FontTextFont* font = (FontTextFont*)text->default_font;
        f32 total = (f32)(line + 1) * ((f32)font->height * font->sy);
        top = layout->vertical == 2 ? (layout->y + layout->h) - total
                                    : layout->y + (layout->h - total) * 0.5f;
    }
    func_0015DBB8(text, &layout->color);
    func_0015DB80(text);
    line = 0;

    while ((value = func_0015DBE0(text)) != 0) {
        if (value == 10) {
            first = 1;
            ++line;
            top += (f32)((FontTextFont*)text->default_font)->height *
                   ((FontTextFont*)text->default_font)->sy;
            continue;
        }
        FontTextFont* font = (FontTextFont*)text->font;
        u32 adjusted;
        if (first) {
            if (layout->horizontal == 0) left = layout->x;
            else {
                f32 width = line_widths[line];
                if (layout->horizontal == 2)
                    left = (layout->x + layout->w) - width;
                else
                    left = layout->x + (layout->w - width) * 0.5f;
            }
            first = 0;
        }

        u32 begin_method = font_vtable_method(font, 0x24u, 0x20u, &adjusted);
        if (!begun) {
            (void)sly1_native_call_regs4(begin_method, adjusted,
                                         (u32)flag, (u32)packet, 0);
            begun = 1;
        } else if (text->font_changed != 0) {
            u32 flush_method = font_vtable_method(font, 0x2Cu, 0x28u, &adjusted);
            (void)sly1_native_call_regs4(flush_method, adjusted,
                                         (u32)packet, 0, 0);
            text->font_changed = 0;
        }
        u32 draw_method = font_vtable_method(font, 0x34u, 0x30u, &adjusted);
        left += sly1_native_call_f32(draw_method, adjusted, (u32)value,
                                     text->color_9c.packed, (u32)packet,
                                     left, top);
    }

    FontTextFont* font = (FontTextFont*)text->font;
    u32 adjusted;
    u32 end_method = font_vtable_method(font, 0x3Cu, 0x38u, &adjusted);
    (void)sly1_native_call_regs4(end_method, adjusted, (u32)flag,
                                 (u32)packet, 0);
}

f32 func_0015CD18(f32 x, f32 y, void* opaque, s32 key, u32 color,
                  void* packet) {
    FontTextFont* font = (FontTextFont*)opaque;
    FontGlyph* glyph = func_0015DAB8(font, key);
    if (glyph == 0) return (f32)font->advance * font->sx;
    s32 shift = (s32)((font->tex0 >> 30) & 0xFu);
    s32 texture_height = 1 << shift;
    f32 height = (f32)font->height * font->sy;
    f32 width = (f32)glyph->width * font->sx;
    f32 right = x + width;
    f32 bottom = y + height;
    if (0.0f <= right && x <= 640.0f && 0.0f <= bottom && y <= 492.80002f) {
        u32 tag = (color & 0xFFFFFFu) | ((((color >> 24) + 1u) & 0x1FEu) << 23);
        func_001617E8(packet, tag, 0.0f);
        func_00161860(packet, (s32)glyph->u << 4,
                      (texture_height - (s32)glyph->v) << 4);
        func_001618C0(packet, (s32)((x + 1728.0f) * 16.0f),
                      (s32)((y * 0.45454547f + 1936.0f) * 16.0f),
                      (u32)font->f40, 0);
        func_00161860(packet, (s32)(glyph->u + glyph->width) << 4,
                      (texture_height - ((s32)glyph->v + font->height)) << 4);
        func_001618C0(packet, (s32)((right + 1728.0f) * 16.0f),
                      (s32)((bottom * 0.45454547f + 1936.0f) * 16.0f),
                      (u32)font->f40, 0);
    }
    return (f32)(glyph->width + 1u) * font->sx;
}

void func_00137228(FontTextLayout* layout, f32 x, f32 y) {
    layout->x = x;
    layout->y = y;
}

void func_00137238(FontTextLayout* layout, f32 width, f32 height) {
    layout->w = width;
    layout->h = height;
}

void func_00137248(FontTextLayout* layout, void* color) {
    layout->color = *(u32*)color;
}

void func_00137260(FontTextLayout* layout, s32 horizontal) {
    layout->horizontal = horizontal;
}

void func_00137268(FontTextLayout* layout, s32 vertical) {
    layout->vertical = vertical;
}

void func_0015DB80(void* opaque) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    text->cursor = (u32)text->text_address;
    text->font = text->default_font;
    text->color_a0.packed = text->color_a4.packed;
    text->color_9c.packed = text->color_a0.packed;
    text->font_changed = 0;
}

void* func_0015DB28(void* opaque, s32 text_address, s32 font_address) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    *(u32*)(text->inline_font + 0x4Cu) = (u32)D_0024A220;
    text->text_address = text_address;
    text->default_font = (u32)font_address;
    text->color_a8.packed = 0xFF808080u;
    text->color_a4.packed = 0xFF808080u;
    text->color_a0.packed = 0xFF808080u;
    text->color_9c.packed = 0xFF808080u;
    func_0015DB80(opaque);
    return opaque;
}

static s32 read_hex_digit(u8 value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

s32 func_0015DBE0(void* opaque) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    u8* current;
    s32 emit = 0;
    do {
        current = (u8*)text->cursor;
        switch (current[0]) {
        case '~':
            if (current[1] == '~') {
                text->cursor = (u32)(current + 1);
                emit = 1;
            } else if (current[1] == '.') {
                text->color_a0.packed = text->color_a4.packed;
                text->color_9c.packed = text->color_a0.packed;
                text->cursor += 2;
            } else {
                u32 value = 0;
                s32 digits = 0;
                while (digits < 6) {
                    s32 digit = read_hex_digit(current[1 + digits]);
                    if (digit < 0) break;
                    value = (value << 4) | (u32)digit;
                    ++digits;
                }
                if (digits == 6) {
                    text->color_a0.rgba[0] = (u8)((((value >> 16) & 0xFFu) + 1u) >> 1);
                    text->color_a0.rgba[1] = (u8)((((value >> 8) & 0xFFu) + 1u) >> 1);
                    text->color_a0.rgba[2] = (u8)(((value & 0xFFu) + 1u) >> 1);
                    text->color_9c.packed = text->color_a0.packed;
                    text->cursor += 7;
                } else {
                    text->font = text->default_font;
                    text->color_9c.packed = text->color_a4.packed;
                    emit = 1;
                }
            }
            break;
        case '&':
            if (current[1] == '&') {
                text->cursor = (u32)(current + 1);
                emit = 1;
            } else if (current[1] == '.') {
                text->font = text->default_font;
                text->color_9c.packed = text->color_a0.packed;
                text->font_changed = 1;
                text->cursor += 2;
            } else {
                u32 next_font = (u32)func_0015C200(current[1]);
                if (next_font != 0) {
                    FontTextFont* inline_font = (FontTextFont*)text->inline_font;
                    FontTextFont* default_font = (FontTextFont*)text->default_font;
                    func_0015C9B0((void*)next_font, inline_font);
                    text->cursor += 2;
                    f32 ratio = ((f32)default_font->height * default_font->sy) /
                                ((f32)inline_font->height * inline_font->sy);
                    inline_font->sx *= ratio;
                    inline_font->sy *= ratio;
                    text->font = (u32)inline_font;
                    text->color_9c.rgba[0] = text->color_a8.rgba[0];
                    text->color_9c.rgba[1] = text->color_a8.rgba[1];
                    text->color_9c.rgba[2] = text->color_a8.rgba[2];
                    text->color_9c.rgba[3] = text->color_a0.rgba[3];
                    text->font_changed = 1;
                } else {
                    text->font = text->default_font;
                    text->color_9c.packed = text->color_a4.packed;
                    text->font_changed = 1;
                    emit = 1;
                }
            }
            break;
        case '^':
            if (current[1] == '^') {
                text->cursor = (u32)(current + 1);
                emit = 1;
            } else if (current[1] == '.') {
                func_0015C900((void*)text->font);
                text->cursor += 2;
            } else if (current[1] != 0 && current[2] != 0) {
                f32 scale = (f32)(current[1] - '0') + (f32)(current[2] - '0') * 0.1f;
                FontTextFont* font = (FontTextFont*)text->font;
                func_0015C8B8(font, scale * font->sx, scale * font->sy);
                text->cursor += 3;
            } else {
                emit = 1;
            }
            break;
        default:
            emit = 1;
            break;
        }
    } while (!emit);
    current = (u8*)text->cursor;
    text->cursor = (u32)(current + 1);
    return (s8)*current;
}

f32 func_0015DFF8(void* opaque) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    f32 width = 0.0f;
    u32 vtable;
    u32 callback;
    func_0015DB80(opaque);
    while (func_0015DBE0(opaque) != 0) {
        FontTextFont* font = (FontTextFont*)text->font;
        vtable = font->vtable;
        callback = *(u32*)(vtable + 0x1Cu);
        font = (FontTextFont*)((u8*)font + *(s16*)(vtable + 0x18u));
        width += sly1_native_call_f32(callback, (u32)font,
                                      (u32)(u8)*((u8*)text->cursor - 1), 0, 0,
                                      0.0f, 0.0f);
    }
    return width;
}

typedef struct FontTextLineState {
    u32 start;
    u32 cursor;
    u32 font;
} FontTextLineState;

static s32 font_text_count_lines(FontTextBuffer* text, f32 limit) {
    FontTextLineState* lines = (FontTextLineState*)text;
    s32 line_count = 1;
    f32 width = 0.0f;
    const s32 newline = 10;
    const s32 space = 32;
    const s32 tab = 9;
    func_0015DB80(text);
    s32 value;
    while ((value = func_0015DBE0(text)) != 0) {
        if (value == newline) {
            width = 0.0f;
            ++line_count;
        } else {
            FontTextFont* font = (FontTextFont*)text->font;
            u32 vtable = font->vtable;
            u32 callback = *(u32*)(vtable + 0x1Cu);
            FontTextFont* adjusted = (FontTextFont*)((u8*)font + *(s16*)(vtable + 0x18u));
            width += sly1_native_call_f32(callback, (u32)adjusted, (u32)value,
                                          0, 0, 0.0f, 0.0f);
        }
        if (limit != 0.0f && limit < width) {
            u8* p = (u8*)(text->cursor - 1u);
            while ((s32)p >= (s32)lines->start) {
                if (*(s8*)p == newline) break;
                if (*(s8*)p == space || *(s8*)p == tab) {
                    width = 0.0f;
                    text->cursor = (u32)p + 1u;
                    ++line_count;
                    *(s8*)p = (s8)newline;
                    break;
                }
                --p;
            }
            if (width != 0.0f) {
                *(s8*)text->cursor = (s8)newline;
                ++line_count;
                width = 0.0f;
            }
        }
    }
    return line_count;
}

f32 func_0015E1B0(void* opaque, f32 limit) {
    FontTextBuffer* text = (FontTextBuffer*)opaque;
    FontTextFont* font = (FontTextFont*)text->default_font;
    s32 lines = font_text_count_lines(text, limit);
    return (f32)font->height * font->sy * (f32)lines;
}
