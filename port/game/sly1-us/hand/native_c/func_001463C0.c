/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only level-stream callback for state notifier objects. */
#include "common.h"

void func_0012AA28(void* object, s32 value);
void func_001289C0(void* object, void* reader);
void func_00124DF8(void* object, s32 count, void* links);
extern u8 D_002619A0[];

void func_001463C0(void* object, void* reader)
{
    func_0012AA28(object, 1);
    func_001289C0(object, reader);
    func_00124DF8(object, 5, D_002619A0);
}
