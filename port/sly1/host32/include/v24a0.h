/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port ABI view of the retail VIF packet writer used by host backfills. */
#ifndef SLY1_PORT_V24A0_H
#define SLY1_PORT_V24A0_H

#include "common.h"

struct V24A0;
struct V24A0_Vt {
    s16 adj0;
    s16 idx0;
    void* fn0;
    s16 adj1;
    s16 idx1;
    void (*close)(V24A0* self, s32 flag);
};

struct V24A0 {
    u8* end;
    u8* begin;
    s32* f08;
    union {
        u8* cur;
        u64* cur64;
        u32 curU;
        s32 curS;
    };
    s32 f10;
    s32 f14;
    const V24A0_Vt* vt;
    s32 f1C;
    u8 pad20[0x10];
};

#endif
