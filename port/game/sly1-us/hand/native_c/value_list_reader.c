/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill for the retail value-list object reader.  The decomp has
 * matching source, but retail metadata classifies this callback as assembly,
 * so the native host currently selects a no-op. */
#include "common.h"

void func_00138398(void* stream, s32* count, void** items);
s32 func_00137BE0(void* stream);
s32 func_00137C40(void* stream);
void* func_0018D500(s32 size);
void func_0011EC40(void* object, void* stream);

void func_001EE930(void* self, void* stream)
{
    s32 count;
    s32 i;
    func_00138398(stream, (s32*)((u8*)self + 0x34),
                  (void**)((u8*)self + 0x38));
    count = func_00137BE0(stream);
    *(s32*)((u8*)self + 0x3C) = count;
    *(u32*)((u8*)self + 0x40) = (u32)func_0018D500(count * 4);
    for (i = 0; i < count; ++i)
        *(s32*)(*(u32*)((u8*)self + 0x40) + (u32)i * 4u) =
            func_00137C40(stream);
    func_0011EC40(self, stream);
}
