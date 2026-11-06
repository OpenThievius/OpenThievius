/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Superseded port backfill; matched C now comes from 00134000.cpp. */
#include "common.h"

void func_00137B60(void* reader, s32 alignment) {
    u8* bytes = (u8*)reader;
    s32 head = *(s32*)(bytes + 0x18);
    s32 aligned = (head + alignment - 1) & -alignment;
    *(s32*)(bytes + 0x18) = aligned;
    *(s32*)(bytes + 0x1C) -= aligned - head;
}
