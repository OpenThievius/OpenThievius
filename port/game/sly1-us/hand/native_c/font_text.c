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

extern u32 D_00262268[];
extern s32 D_0026227C;
extern s8 D_00262250[];
extern u8 D_0024A220[];
extern f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2,
                                u32 a3, f32 f12, f32 f14);

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
