/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly animation and image-table reader, separate from matched C. */
#include "common.h"

typedef struct ImageA {
    u32 w0;
    u32 f4;
    u32 f8;
    u32 f0c;
    s32 f10;
    u32 f14;
    u64 f18;
} ImageA;

typedef struct ImageB {
    u32 f0;
    u16 f4;
    u16 f6;
    u32 f8;
    u32 f0c;
    u64 f10;
} ImageB;

typedef struct AnimRecord {
    u8 kind;
    u8 flags;
    s16 key;
    u8 pad04[0x0f];
    u8 element_count;
    u32 elements;
    s32 entry_count;
    u32 entries;
    s32 frame_count;
    u32 owner;
} AnimRecord;

typedef struct AnimElement {
    u8 pad00[4];
    u8 count_a;
    u8 count_b;
    u8 pad06[6];
    u32 images_a;
    u32 images_b;
} AnimElement;

typedef char check_anim_record_size[sizeof(AnimRecord) == 0x28 ? 1 : -1];
typedef char check_anim_element_size[sizeof(AnimElement) == 0x14 ? 1 : -1];

typedef struct AnimOwner {
    u32 vtable;
    f32 stamp;
    s32 type;
    s32 id;
    s32 cursor_flags;
    u32 cursor_record;
    u8 cursor_tail[0x14];
} AnimOwner;

typedef struct FontSource {
    u16 image_a;
    u16 image_b;
    u8 width;
    u8 advance;
    u8 height;
    u8 id;
    f32 scale;
    u16 glyph_count;
    u16 reserved;
} FontSource;

typedef struct BumpAllocator {
    s32 next;
    s32 start;
    s32 limit;
} BumpAllocator;

extern s32 D_002744F8;
extern AnimRecord* D_002744FC;
extern s32 D_00274500;
extern ImageA* D_00274504;
extern s32 D_00274508;
extern ImageB* D_0027450C;
extern s32 D_00274510;
extern s32 D_00274514;
extern s32 D_00274520[6];
extern s32 D_00274538;
extern s32 D_0027453C;
extern u32* D_00274540;
extern s32 D_00262260;
extern u8* D_00262264;
extern s32 D_00262268[5];
extern s32 D_0026227C;
extern u32 D_00275718;
extern u8 D_002626D8[];

s32 func_00137BE0(void* reader);
void func_00137A60(void* reader, s32 size, void* destination);
void* func_0018D5C0(s32 bytes);
void func_00168D68(void* allocator, s32 start, s32 limit);
s32 func_001690C0(s32 mask, BumpAllocator* allocator, void* writer);
void func_001B3510(s32 mask);
void func_0015E5F0(void);
void func_001A9F78(void);
s32 func_00169780(s32 mask);
s32 func_001693A8(s32 mask);

static void read_image_b_table(void* reader) {
    D_00274508 = func_00137BE0(reader);
    D_0027450C = (ImageB*)func_0018D5C0(D_00274508 * (s32)sizeof(ImageB));
    D_00274514 = 0;
    for (s32 i = 0; i < D_00274508; ++i) {
        ImageB* entry = &D_0027450C[i];
        func_00137A60(reader, 12, entry);
        entry->f8 += D_00275718;
        entry->f0c = (entry->f4 * 4u + 15u) >> 4;
        if (entry->f0 & 0x10000000u) ++D_00274514;
    }
    s32 other = D_00274508 - D_00274514;
    if (D_00274514 < other) D_00274514 = other;
}

static void read_image_a_table(void* reader) {
    D_00274500 = func_00137BE0(reader);
    D_00274504 = (ImageA*)func_0018D5C0(D_00274500 * (s32)sizeof(ImageA));
    D_00274510 = 0;
    for (s32 i = 0; i < D_00274500; ++i) {
        ImageA* entry = &D_00274504[i];
        func_00137A60(reader, 0x14, entry);
        entry->f14 = (entry->f0c + 15u) >> 4;
        entry->f10 += (s32)D_00275718;
        if (entry->f4 & 0x10000000u) ++D_00274510;
    }
    s32 other = D_00274500 - D_00274510;
    if (D_00274510 < other) D_00274510 = other;
}

static s32 animation_owner_size(s32 type) {
    switch (type) {
    case 1: case 2: return 0x4c;
    case 3: case 4: return 0x38;
    case 5: return 0x78;
    case 6: return 0x44;
    case 7: return 0x3c;
    case 8: return 0x50;
    default: return 0;
    }
}

static u32 animation_owner_vtable(s32 type) {
    switch (type) {
    case 1: return 0x0021e358u;
    case 2: return 0x0021e378u;
    case 3: return 0x0021e398u;
    case 4: return 0x0021e3b8u;
    case 5: return 0x0021e438u;
    case 6: return 0x0021e3d8u;
    case 7: return 0x0021e3f8u;
    case 8: return 0x0021e418u;
    default: return 0;
    }
}

static void* read_animation_owner(void* reader) {
    s32 type = func_00137BE0(reader);
    s32 bytes = animation_owner_size(type);
    if (bytes == 0) return 0;

    AnimOwner* owner = (AnimOwner*)func_0018D5C0(bytes);
    if (owner == 0) return 0;
    u8 source[0x1c];
    func_00137A60(reader, sizeof(source), source);
    owner->vtable = animation_owner_vtable(type);
    owner->stamp = 0.0f;
    owner->type = type;
    owner->id = *(s16*)source;
    owner->cursor_flags = 1 | (*(u16*)(source + 2) != 0 ? 4 : 0);
    owner->cursor_record = 0;
    return owner;
}

static void read_animation_element(void* reader, AnimElement* element) {
    func_00137A60(reader, 6, element);
    if (element->count_a != 0) {
        element->images_a = (u32)func_0018D5C0(element->count_a * 4);
        for (s32 i = 0; i < element->count_a; ++i) {
            u32 index = func_00137BE0(reader);
            ((u32*)element->images_a)[i] = (u32)D_00274504 + index * sizeof(ImageA);
        }
    }
    if (element->count_b != 0) {
        element->images_b = (u32)func_0018D5C0(element->count_b * 4);
        for (s32 i = 0; i < element->count_b; ++i) {
            u32 index = func_00137BE0(reader);
            ((u32*)element->images_b)[i] = index == 0xffffu ? 0 : (u32)D_0027450C + index * sizeof(ImageB);
        }
    }
}

static void read_font_table(void* reader) {
    D_00262260 = func_00137BE0(reader);
    D_00262264 = (u8*)func_0018D5C0(D_00262260 * 0x88);
    for (s32 i = 0; i < D_00262260; ++i) {
        FontSource source;
        u8* font = D_00262264 + i * 0x88;
        func_00137A60(reader, sizeof(source), &source);
        s32 id = source.id;
        if (id >= 0 && id < 5) D_00262268[id] = (s32)font;
        u32 image_a = (u32)D_00274504 + source.image_a * sizeof(ImageA);
        u32 image_b = source.image_b == 0xffffu ? 0 : (u32)D_0027450C + source.image_b * sizeof(ImageB);
        *(s32*)(font + 0x00) = source.width;
        *(s32*)(font + 0x04) = source.advance;
        *(s32*)(font + 0x08) = source.height;
        *(u32*)(font + 0x4c) = 0x0024a220u;
        *(s32*)(font + 0x50) = id;
        *(u32*)(font + 0x54) = image_a;
        *(u32*)(font + 0x58) = image_b;
        *(f32*)(font + 0x44) = source.scale;
        *(f32*)(font + 0x48) = source.scale;
        *(s32*)(font + 0x78) = source.glyph_count;
        u32 glyphs = (u32)func_0018D5C0(source.glyph_count * 8);
        *(u32*)(font + 0x7c) = glyphs;
        *(u32*)(font + 0x80) = 1u << i;
        func_00137A60(reader, source.glyph_count * 8, (void*)glyphs);
    }
    D_0026227C = D_00262268[0];
}

void func_001B3040(void* reader) {
    read_image_b_table(reader);
    read_image_a_table(reader);

    D_002744F8 = func_00137BE0(reader);
    D_002744FC = (AnimRecord*)func_0018D5C0(D_002744F8 * (s32)sizeof(AnimRecord));
    D_0027453C = func_00137BE0(reader);
    D_00274540 = (AnimOwner**)func_0018D5C0(D_0027453C * 4);
    s32 owner_index = 0;

    for (s32 i = 0; i < D_002744F8; ++i) {
        AnimRecord* record = &D_002744FC[i];
        func_00137A60(reader, 0x14, record);
        record->owner = (u32)read_animation_owner(reader);
        if (record->owner != 0) {
            ((AnimOwner*)record->owner)->cursor_record = (u32)record;
            if (owner_index < D_0027453C) D_00274540[owner_index++] = record->owner;
        }

        record->elements = (u32)func_0018D5C0(record->element_count * (s32)sizeof(AnimElement));
        for (s32 j = 0; j < record->element_count; ++j) {
            AnimElement* element = (AnimElement*)(record->elements + j * sizeof(AnimElement));
            read_animation_element(reader, element);
            *(u32*)((u8*)element + 8) = (u32)record;
        }

        record->frame_count = record->element_count == 0 ? 0 : ((AnimElement*)record->elements)->count_a;
        switch (record->kind) {
        case 0: record->entry_count = 3; break;
        case 1: case 2: case 3: case 5: case 6: case 7: case 8: case 9: case 10:
            record->entry_count = 1; break;
        case 4: record->entry_count = 4; break;
        default: record->entry_count = 0; break;
        }
        record->entries = (u32)func_0018D5C0(record->entry_count * 8);
        u8* entries = (u8*)record->entries;
        switch (record->kind) {
        case 1: case 6: case 7: case 8: case 10:
            *(u32*)entries = 8;
            *(u32*)(entries + 4) = (u32)func_0018D5C0(record->frame_count << 7);
            break;
        case 2: case 3:
            *(u32*)entries = 6;
            *(u32*)(entries + 4) = (u32)func_0018D5C0(record->frame_count * 6 << 4);
            break;
        case 4: {
            s32 first = (record->flags & 2) ? 8 : 5;
            for (s32 k = 0; k < 4; ++k) {
                s32 item_size = k == 0 ? first : (k == 1 ? 5 : (k == 2 ? 7 : 4));
                *(u32*)(entries + k * 8) = item_size;
                *(u32*)(entries + k * 8 + 4) = (u32)func_0018D5C0(record->frame_count * item_size << 4);
            }
            break;
        }
        case 0:
            for (s32 k = 0; k < 3; ++k) {
                *(u32*)(entries + k * 8) = 8;
                *(u32*)(entries + k * 8 + 4) = (u32)func_0018D5C0(record->frame_count << 7);
            }
            break;
        case 5:
            *(u32*)entries = 7;
            *(u32*)(entries + 4) = (u32)func_0018D5C0(record->frame_count * 7 << 4);
            break;
        case 9:
            *(u32*)entries = 4;
            *(u32*)(entries + 4) = (u32)func_0018D5C0(0x40);
            break;
        }
    }

    read_font_table(reader);
    BumpAllocator allocator;
    func_00168D68(&allocator, 0x1e00, 0x4000);
    (void)func_001690C0(0x10000000, &allocator, 0);
    func_001B3510(0x10000000);
    func_00168D68(D_002626D8, allocator.next, 0x4000);
    func_0015E5F0();
    func_001A9F78();
    for (s32 i = 0; i < 6; ++i)
        D_00274520[i] = func_00169780((1 << i) | 0x40000000);
    D_00274538 = func_001693A8(0x20000000);
}
