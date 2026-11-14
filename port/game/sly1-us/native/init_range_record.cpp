// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only implementation of the still-assembly InitRangeRecord routine.
#include "globals.h"
#include "range_record.h"

extern "C" void func_001B1AC8(RangeRecord* record)
{
    __builtin_memset(record, 0, sizeof(*record));
    record->f04 = -1.0f;
    record->f08 = -1.0f;
    record->v20 = g_unitZVector;
    record->v20.x = -record->v20.x;
    record->v20.y = -record->v20.y;
    record->v20.z = -record->v20.z;
    record->v20.w = -record->v20.w;
    record->v30 = g_unitYVector;
    record->f40 = 100.0f;
    record->f44 = 400.0f;
    record->fB0 = -1;
}
