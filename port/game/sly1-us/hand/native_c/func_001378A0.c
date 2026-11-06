/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only LZ window refill for Sly's buffered resource reader. */
#include "common.h"

#define SLY_LZ_STATE_MAGIC 0x4C5A5353u
#define SLY_LZ_WINDOW_SIZE 0x2000u
#define SLY_LZ_DICTIONARY 0x70002000u
#define SLY_LZ_OUTPUT 0x70000000u

void func_00137820(void* self);
void sly1_native_loader_stage(u32 stage, u32 value, u32 reader);

static s32 sly_lz_get_byte(u8** source, s32* source_remaining,
                           u8* reader) {
    if (*source_remaining <= 0) {
        /* Keep the stream's saved segment start and length intact. The
         * retail refill uses them to advance the underlying file cursor. */
        func_00137820(reader);
        *source = (u8*)(u32)*(u32*)(reader + 0x20);
        *source_remaining = *(s32*)(reader + 0x24);
        if (*source_remaining <= 0) return -1;
    }

    --*source_remaining;
    return *(*source)++;
}

void func_001378A0(void* self) {
    u8* reader = (u8*)self;
    u8* dictionary = (u8*)SLY_LZ_DICTIONARY;
    u8* output = (u8*)SLY_LZ_OUTPUT;
    u8* source;
    s32 source_remaining;
    u32 control;
    u32 write_position;
    u32 output_count = 0;
    u32 i;

    source = (u8*)(u32)*(u32*)(reader + 0x20);
    source_remaining = *(s32*)(reader + 0x24);
    if (*(u32*)(reader + 0x38) != SLY_LZ_STATE_MAGIC) {
        control = 0;
        write_position = 0;
        *(u32*)(reader + 0x10) = 0;
        *(u32*)(reader + 0x14) = 0;
        for (i = 0; i < SLY_LZ_WINDOW_SIZE; ++i) dictionary[i] = 0;
        *(u32*)(reader + 0x38) = SLY_LZ_STATE_MAGIC;
    } else {
        control = *(u32*)(reader + 0x28);
        write_position = *(u32*)(reader + 0x3C) & (SLY_LZ_WINDOW_SIZE - 1u);
    }

    while (output_count < SLY_LZ_WINDOW_SIZE) {
        u32 token;
        u32 read_position;
        u32 count;

        if ((control & 0x100u) == 0) {
            s32 flag = sly_lz_get_byte(&source, &source_remaining, reader);
            if (flag < 0) break;
            control = (u32)flag | 0xFF00u;
        }

        if (control & 1u) {
            s32 literal = sly_lz_get_byte(&source, &source_remaining, reader);
            if (literal < 0) break;
            dictionary[write_position] = (u8)literal;
            output[output_count++] = (u8)literal;
            write_position = (write_position + 1u) & (SLY_LZ_WINDOW_SIZE - 1u);
        } else {
            s32 low = sly_lz_get_byte(&source, &source_remaining, reader);
            s32 high = sly_lz_get_byte(&source, &source_remaining, reader);
            if (low < 0 || high < 0) break;

            token = (u32)low | ((u32)high << 8);
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
        }

        control >>= 1;
    }

    *(u32*)(reader + 0x18) = SLY_LZ_OUTPUT;
    *(s32*)(reader + 0x1C) = (s32)output_count;
    *(u32*)(reader + 0x20) = (u32)source;
    *(s32*)(reader + 0x24) = source_remaining;
    *(u32*)(reader + 0x28) = control;
    *(u32*)(reader + 0x3C) = write_position;
    sly1_native_loader_stage(87, output_count, (u32)reader);
}
