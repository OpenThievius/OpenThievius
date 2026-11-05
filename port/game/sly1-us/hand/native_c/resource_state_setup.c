/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly resource state selection and request queue setup. */
#include "common.h"

u64 sly1_native_monotonic_ticks(void);

extern u8* D_002623C0;
extern u8* D_002623C4;
extern u8* D_002623C8;
extern s32 D_002623CC;
extern u8 D_00247AF0[];

void func_001608E0(void);
void func_00160880(void);
void func_00140D90(u8* queue);

typedef struct ResourceTableEntry {
    f32 key;
    s32 first;
    s32 second;
} ResourceTableEntry;

typedef struct ResourceStateTable {
    u32 header[21];
    s32 count;
    ResourceTableEntry* entries;
    s32 tail;
} ResourceStateTable;

static s32 resource_key_id(const u8* key)
{
    u32 key_value;
    s32 index;

    if (key == 0) return -1;
    key_value = *(const u32*)(key + 8) ^ *(const u32*)(key + 24);
    for (index = 0; index < 46; ++index) {
        const u8* entry = D_00247AF0 + (u32)index * 0x2Cu;
        if (key_value == (*(const u32*)(entry + 8) ^
                          *(const u32*)(entry + 24))) {
            return *(const s32*)(entry + 0x20);
        }
    }
    return -1;
}

void func_00140F48(u8* queue)
{
    u32 index;
    *(s32*)(queue + 4) = 0;
    for (index = 0; index < 0x81u; ++index) {
        *(u32*)(queue + 8 + index * 4u) =
            *(u32*)(queue + 0x220 + index * 4u);
    }
    *(s32*)(queue + 0x20C) = 0;
}

void func_00160690(const u8* key, s32 flags)
{
    s32 resource_id = resource_key_id(key);
    s32 hi = resource_id != -1 ? resource_id >> 8 : 0;
    s32 lo = resource_id != -1 ? resource_id & 0xFF : 0;
    u8* base = D_002623C0 + 0x10u + (u32)hi * 0x44Cu;
    s32 gate = *(s32*)(D_002623C0 + 0x19E0u);

    D_002623C4 = base;
    D_002623C8 = base + (u32)lo * 0x78u;
    if (gate < 0 || (flags & 2) != 0) {
        func_001608E0();
    } else if ((flags & 1) != 0) {
        func_00140F48((u8*)0x00261420u);
    } else if ((flags & 4) != 0) {
        func_00160880();
    } else {
        func_00140D90((u8*)0x00261420u);
    }

    D_002623CC = *(s32*)(D_002623C0 + 0x19D8u);
    *(s32*)(D_002623C0 + 0x19D8u) = hi;
    *(s32*)(D_002623C0 + 0x19DCu) = lo;
    *(u32*)(D_002623C4 + 0x448u) |= 1u;
    *(u32*)D_002623C8 |= 1u;
}

s32 func_001EAA70(s32 first, s32 last)
{
    s32 span;
    s32 value;

    if (first == last) return first;
    span = last - first + 1;
    /* The retail helper's RNG routine has no C; use the host clock as the
     * port-only source while preserving the retail range reduction. */
    value = (s32)(u32)sly1_native_monotonic_ticks();
    value %= 0x95675;
    if (span == 0) return first;
    value %= span;
    return first + value;
}

void func_00152410(void* object, f32 input)
{
    const u8* table = (const u8*)(*(u32*)((u8*)object + 0x1Cu));
    f32 lower = *(const f32*)(table + 0x00u);
    f32 upper = *(const f32*)(table + 0x04u);
    if (input < lower) input = lower;
    if (input > upper) input = upper;
    *(f32*)(D_002623C8 + 0x0Cu) = input;
}

void func_00151A88(void* object)
{
    ResourceStateTable* table;
    ResourceTableEntry* entry = 0;
    s32 first;
    s32 last;
    s32 index;
    s32 count;
    f32 amount;
    f32 start;
    f32 end;

    *(s32*)((u8*)object + 0x04u) = 4;
    *(s32*)((u8*)object + 0x08u) = 6;
    *(s32*)((u8*)object + 0x0Cu) = 1;
    *(s32*)((u8*)object + 0x10u) = 3;

    if (*(s32*)(D_002623C0 + 0x19D8u) == 0 ||
        *(s32*)(D_002623C0 + 0x19D8u) == 5 ||
        *(s32*)(D_002623C0 + 0x19DCu) == 1) {
        table = (ResourceStateTable*)0x00261E90u;
    } else if ((*(u32*)D_002623C8 & 2u) == 0) {
        table = (ResourceStateTable*)0x00261D70u;
    } else {
        table = (ResourceStateTable*)0x00261E30u;
    }
    *(u32*)((u8*)object + 0x1Cu) = (u32)table;

    amount = *(f32*)(D_002623C8 + 0x0Cu);
    func_00152410(object, amount);
    amount = *(f32*)(D_002623C8 + 0x0Cu);

    first = *(s32*)((u8*)table + 0x1Cu);
    last = *(s32*)((u8*)table + 0x24u);
    start = (f32)first;
    end = (f32)last;
    if (first == last) {
        first = last;
    } else if (first < last) {
        first += (s32)(amount * (end + 0.99900001f - (f32)first));
    } else {
        first = last + (s32)((1.0f - amount) * (start + 0.99900001f - end));
    }

    s32 second = *(s32*)((u8*)table + 0x20u);
    s32 second_last = *(s32*)((u8*)table + 0x28u);
    start = (f32)second;
    end = (f32)second_last;
    if (second == second_last) {
        second = second_last;
    } else if (second < second_last) {
        second += (s32)(amount * (end + 0.99900001f - (f32)second));
    } else {
        second = second_last + (s32)((1.0f - amount) * (start + 0.99900001f - end));
    }

    *(s32*)((u8*)object + 0x14u) = func_001EAA70(first, second);
    count = table->count;
    for (index = 0; index < count; ++index) {
        entry = &table->entries[index];
        if (entry->key <= *(f32*)(D_002623C8 + 0x10u)) break;
    }
    *(s32*)((u8*)object + 0x18u) = entry != 0
        ? func_001EAA70(entry->first, entry->second) : 0;
    *(s32*)((u8*)object + 0x20u) = 0;
}
