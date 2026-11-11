/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only copies of the class-0x53 lifecycle callbacks. The matching C is
 * in the reworked decomp, but hostgen cannot compile its containing unit. */
#include "common.h"

void func_00123ED0(void* object);
void func_00124A48(void* object);
void func_00152470(void* list, void* item);
void sly1_native_call_method(u32 address, u32 object);

void func_001963D8(void* object) {
    if (object == 0) return;

    /* InitPlacedObject sets up the common child and world lists first. */
    func_00123ED0(object);

    const u32 world = *(u32*)((u8*)object + 0x14u);
    if (world != 0)
        func_00152470((void*)(world + 0x1C3Cu), object);
}

void func_00196900(void* object) {
    if (object == 0) return;

    /* StartPlacedObject recursively starts the group's attached members. */
    func_00124A48(object);

    /* Match ObjectGroup::StartObjectGroup's descriptor slot-3 callback. */
    const u32 descriptor = *(u32*)object;
    if (descriptor == 0) return;
    const u32 on_start = *(u32*)(descriptor + 0x1Cu);
    if (on_start != 0)
        sly1_native_call_method(on_start, (u32)object);
}
