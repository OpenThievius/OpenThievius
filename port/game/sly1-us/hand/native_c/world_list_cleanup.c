/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfills for exact retail world-teardown list cleanup. */
#include "common.h"

void func_00152440(void* list);

void func_00143AF0(void* camera) {
    if (camera != 0) *(s32*)((u8*)camera + 0x340u) = 0;
}

void func_00169CA0(void) {
    /* target curve, target point, and part-object registries */
    func_00152440((void*)0x002626E8u);
    func_00152440((void*)0x002626F8u);
    func_00152440((void*)0x00262708u);
}

void func_00191EE8(void) {
    /* The target-vector registry is a List01 at this fixed retail address. */
    func_00152440((void*)0x00269BA8u);
}

void func_0017FD70(void) {
    /* Registered point objects use this global List01. */
    func_00152440((void*)0x00264088u);
}

void func_001DFD70(void) {
    /* Class 0x75 objects link their registry node at object+0x80. */
    func_00152440((void*)0x002758D0u);
}
