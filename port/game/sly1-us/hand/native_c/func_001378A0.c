/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only LZ window refill for Sly's buffered resource reader. */
#include "common.h"

#define SLY_LZ_STATE_MAGIC 0x4C5A5353u
#define SLY_LZ_WINDOW_SIZE 0x2000u
#define SLY_LZ_DICTIONARY 0x70002000u
#define SLY_LZ_OUTPUT 0x70000000u

void func_001378A0(void* self) {
    u8* reader = (u8*)self;
    u8* dictionary = (u8*)SLY_LZ_DICTIONARY;
    u8* output = (u8*)SLY_LZ_OUTPUT;
    u32 source;
    s32 source_remaining;
    u32 control;
    u32 write_position;
    u32 output_count = 0;
    u32 i;

    if (*(u32*)(reader + 0x38) != SLY_LZ_STATE_MAGIC) {
        /* The first refill reads compressed bytes from the source cursor;
         * +0x18/+0x1C are the output window and are initially empty. */
        source = *(u32*)(reader + 0x20);
        source_remaining = *(s32*)(reader + 0x24);
        control = 0;
        write_position = 0;
        *(u32*)(reader + 0x10) = 0;
        *(u32*)(reader + 0x14) = 0;
        for (i = 0; i < SLY_LZ_WINDOW_SIZE; ++i) dictionary[i] = 0;
        *(u32*)(reader + 0x38) = SLY_LZ_STATE_MAGIC;
    } else {
        source = *(u32*)(reader + 0x20);
        source_remaining = *(s32*)(reader + 0x24);
        control = *(u32*)(reader + 0x28);
        write_position = *(u32*)(reader + 0x3C) & (SLY_LZ_WINDOW_SIZE - 1u);
    }

    if (source_remaining < 0) source_remaining = 0;
    while (output_count < SLY_LZ_WINDOW_SIZE) {
        u8* input = (u8*)source;
        u32 token;
        u32 read_position;
        u32 count;

        count = *(u32*)(reader + 0x14);
        if (count != 0) {
            read_position = *(u32*)(reader + 0x10) & (SLY_LZ_WINDOW_SIZE - 1u);
            while (count != 0 && output_count < SLY_LZ_WINDOW_SIZE) {
                u8 value = dictionary[read_position];
                read_position = (read_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
                dictionary[write_position] = value;
                output[output_count++] = value;
                write_position = (write_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
                --count;
            }
            *(u32*)(reader + 0x10) = read_position;
            *(u32*)(reader + 0x14) = count;
            continue;
        }

        if (source_remaining <= 0) break;

        if ((control & 0x100u) == 0) {
            /* Retail keeps the control byte in the low bits with a 0xFF00
             * sentinel; the sentinel shifts out after eight token bits. */
            control = (u32)input[0] | 0xFF00u;
            ++source;
            --source_remaining;
            if (source_remaining <= 0) break;
            input = (u8*)source;
        }

        if (control & 1u) {
            u8 value = input[0];
            ++source;
            --source_remaining;
            dictionary[write_position] = value;
            output[output_count++] = value;
            write_position = (write_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
        } else {
            if (source_remaining < 2) break;
            token = (u32)input[0] | ((u32)input[1] << 8);
            source += 2;
            source_remaining -= 2;
            read_position = token & (SLY_LZ_WINDOW_SIZE - 1u);
            count = (token >> 13) + 3u;
            while (count != 0 && output_count < SLY_LZ_WINDOW_SIZE) {
                u8 value = dictionary[read_position];
                read_position = (read_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
                dictionary[write_position] = value;
                output[output_count++] = value;
                write_position = (write_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
                --count;
            }
            *(u32*)(reader + 0x10) = read_position;
            *(u32*)(reader + 0x14) = count;
        }

        control >>= 1;
    }

    *(u32*)(reader + 0x18) = SLY_LZ_OUTPUT;
    *(s32*)(reader + 0x1C) = (s32)output_count;
    *(u32*)(reader + 0x20) = source;
    *(s32*)(reader + 0x24) = source_remaining;
    *(u32*)(reader + 0x28) = control;
    *(u32*)(reader + 0x3C) = write_position;
}
