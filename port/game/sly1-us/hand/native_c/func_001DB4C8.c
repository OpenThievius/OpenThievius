/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only Sly type-0x6C constructor backfill, kept outside matching C. */
#include "common.h"

struct AllocationSpec {
    u32 offset;
    s32 element_size;
    s32 count;
    u32 aligned;
};

void func_00182C18(void* self);
void func_001C09D8(void* self, s32 index, s32 value);
void* func_0018D500(s32 size);
void* func_0018D5C0(s32 size);
void func_001DC740(void* self);
void func_001B5DD0(void* self, s32 size, s32 count);
void func_001B5E30(void* self, s32 size, s32 count);
void func_00152438(void* self, void* value);
void func_001DBA90(void* self, f32 value);

extern u32 D_00262C64;

void func_001DB4C8(void* self) {
    const struct AllocationSpec allocations[] = {
        {0x1AF0, 0x10, 0x400, 0},
        {0x1AFC, 0xE0, 0x100, 0},
        {0x1B08, 0x10, 0x80, 0},
        {0x1B14, 0x14, 0x400, 0},
        {0x1B2C, 0x140, 0x180, 0},
        {0x1B40, 0x10C0, 0x80, 1},
        {0x1B4C, 0x580, 0x20, 0},
        {0x1B58, 0xEC, 0x100, 0},
        {0x1B20, 0xD0, 0x400, 0},
        {0x1BA0, 0x90, 0x80, 0},
        {0x1BB8, 0x0C, 0x20, 0},
        {0x1B88, 0x40, 0x100, 0},
        {0x1BF4, 0x2E0, 0x20, 0},
        {0x1BD0, 0x70, 0x20, 0},
        {0x1C0C, 0x08, 0x80, 0},
        {0x1C18, 0x14, 0x100, 0},
    };
    const struct { u32 offset; u32 value; } lists[] = {
        {0x0048, 0x001C}, {0x0054, 0x0048}, {0x0078, 0x0048},
        {0x0060, 0x0040}, {0x006C, 0x0414}, {0x003C, 0x02D0},
        {0x1B64, 0x0034}, {0x1B70, 0x0034}, {0x1B7C, 0x0034},
        {0x1B94, 0x0010}, {0x1BAC, 0x0064}, {0x1BC4, 0x0004},
        {0x1BE8, 0x0410}, {0x1C00, 0x00B8}, {0x1BDC, 0x0064},
        {0x1C3C, 0x02DC}, {0x1C48, 0x05DC}, {0x1C54, 0x0374},
        {0x1C60, 0x0600}, {0x1C6C, 0x0090}, {0x1C78, 0x056C},
        {0x1C84, 0x0050}, {0x1C90, 0x0038}, {0x1C9C, 0x0038},
        {0x1CA8, 0x0AF0}, {0x1CB4, 0x0630}, {0x1CC0, 0x0630},
        {0x1C24, 0x000C}, {0x1C30, 0x000C}, {0x1CCC, 0x0C10},
        {0x1CD8, 0x0034},
    };
    u8* bytes = (u8*)self;
    u32 index;

    func_00182C18(self);
    func_001C09D8(self, 0, 0);

    *(u32*)(bytes + 0x1AD8) = (u32)func_0018D500(0x4800);
    *(u32*)(bytes + 0x1ADC) = (u32)func_0018D5C0(0x10000);
    func_001DC740(self);

    for (index = 0; index < sizeof(allocations) / sizeof(allocations[0]); ++index) {
        const struct AllocationSpec* item = &allocations[index];
        void* field = bytes + item->offset;
        if (item->aligned) func_001B5E30(field, item->element_size, item->count);
        else func_001B5DD0(field, item->element_size, item->count);
    }

    *(u32*)(bytes + 0x1ED4) = (u32)func_0018D500(0x1000);
    for (index = 0; index < sizeof(lists) / sizeof(lists[0]); ++index) {
        func_00152438(bytes + lists[index].offset, (void*)lists[index].value);
    }

    func_001DBA90(self, (D_00262C64 & 4u) != 0 ? -900.0f : -1500.0f);
    *(u32*)(bytes + 0x235C) = 0xF03FFFFFu;
    *((u8*)self + 0x1D65) = 0x10;
    *((u8*)self + 0x1D66) = 0x40;
    *(f32*)(bytes + 0x1D68) = 50.0f;
    *(u32*)(bytes + 0x1D70) = 0xFFFFFFFFu;
    *(f32*)(bytes + 0x2308) = 15.0f;
    *(f32*)(bytes + 0x230C) = 180.0f;
    *(f32*)(bytes + 0x2350) = 1.0f;
    *(u32*)(bytes + 0x2354) = 1;
    *((u8*)self + 0x1D64) = 0x10;
    *(u32*)(bytes + 0x1D74) = 0;
    *(f32*)(bytes + 0x234C) = 1.0f;
}
