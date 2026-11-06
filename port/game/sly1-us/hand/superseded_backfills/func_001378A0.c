/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only LZ window refill for Sly's buffered resource reader. */
#include "common.h"

#define SLY_LZ_WINDOW_SIZE 0x2000u
#define SLY_LZ_DICTIONARY 0x70002000u
#define SLY_LZ_OUTPUT 0x70000000u
#define SLY_SPLASH_IMAGE_OFFSET 0x9E1Au
#define SLY_SPLASH_IMAGE_SIZE 0x5F4F0u

void func_00137820(void* self);
void sly1_native_loader_stage(u32 stage, u32 value, u32 reader);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);
extern s32 D_00275718;
extern s32 D_00275720;
extern s32 D_00275724;

static s32 sly_lz_get_byte(u8** source, s32* source_remaining,
                           u8* reader) {
    if (*source_remaining <= 0) {
        /* The retail reader refills its compressed source here. Decoder
         * state lives at +0x2C, clear of the sector window's +0x38/+0x3C. */
        func_00137820(reader);
        *source = (u8*)(u32)*(u32*)(reader + 0x20);
        *source_remaining = *(s32*)(reader + 0x24);
        if (*source_remaining <= 0) return -1;
    }

    --*source_remaining;
    return *(*source)++;
}

void func_001378A0(void* self) {
    static u8* tracked_reader;
    static u32 decoded_offset;
    u8* reader = (u8*)self;
    u8* dictionary = (u8*)SLY_LZ_DICTIONARY;
    u8* output = (u8*)SLY_LZ_OUTPUT;
    u8* source;
    s32 source_remaining;
    u32 control;
    u32 write_position;
    u32 pending_count;
    u32 pending_read_position;
    u32 state;
    u32 output_count = 0;
    u32 i;

    source = (u8*)(u32)*(u32*)(reader + 0x20);
    source_remaining = *(s32*)(reader + 0x24);
    state = *(u32*)(reader + 0x2C);
    if ((state & 0x80000000u) == 0) {
        tracked_reader = reader;
        decoded_offset = 0;
        control = *(u32*)(reader + 0x28);
        write_position = 0;
        pending_count = 0;
        pending_read_position = 0;
        for (i = 0; i < SLY_LZ_WINDOW_SIZE; ++i) dictionary[i] = 0;
    } else {
        if (tracked_reader != reader) {
            tracked_reader = reader;
            decoded_offset = 0;
        }
        control = *(u32*)(reader + 0x28);
        write_position = state & (SLY_LZ_WINDOW_SIZE - 1u);
        pending_read_position = (state >> 13) & (SLY_LZ_WINDOW_SIZE - 1u);
        pending_count = (state >> 26) & 0xFu;
    }

    while (output_count < SLY_LZ_WINDOW_SIZE) {
        u32 token;
        u32 read_position;
        u32 count;

        if (pending_count != 0) {
            u8 value = dictionary[pending_read_position];
            pending_read_position = (pending_read_position + 1u) &
                                   (SLY_LZ_WINDOW_SIZE - 1u);
            dictionary[write_position] = value;
            output[output_count++] = value;
            write_position = (write_position + 1u) &
                             (SLY_LZ_WINDOW_SIZE - 1u);
            --pending_count;
            continue;
        }

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
            pending_count = count;
            pending_read_position = read_position;
        }

        control >>= 1;
    }

    /* Port-only experiment: the observed Splash image table requests exactly
     * 0x5F4F0 bytes with this revision. Metadata offsets reach the same size,
     * and that exact range is the 0x9E1A-byte tail of the decoded resource.
     * Preserve the retail reader above; probe only whether that tail fills the
     * otherwise EOF-bound cache. Remove when the object layout is understood.
     */
    if (D_00275720 == (s32)SLY_SPLASH_IMAGE_SIZE &&
        (u32)D_00275724 == 0x3D6876BBu && D_00275718 != 0 &&
        output_count != 0) {
        u32 chunk_start = decoded_offset;
        u32 chunk_end = chunk_start + output_count;
        u32 image_start = SLY_SPLASH_IMAGE_OFFSET;
        u32 image_end = image_start + SLY_SPLASH_IMAGE_SIZE;
        u32 copy_start = chunk_start > image_start ? chunk_start : image_start;
        u32 copy_end = chunk_end < image_end ? chunk_end : image_end;

        if (copy_start < copy_end) {
            u8* image = (u8*)(u32)D_00275718;
            u32 source_offset = copy_start - chunk_start;
            u32 image_offset = copy_start - image_start;
            u32 copy_count = copy_end - copy_start;
            for (i = 0; i < copy_count; ++i)
                image[image_offset + i] = output[source_offset + i];
            if (copy_end == image_end)
                sly1_native_loader_stage(88, SLY_SPLASH_IMAGE_SIZE,
                                         (u32)reader);
        }
    }
    decoded_offset += output_count;

    *(u32*)(reader + 0x18) = SLY_LZ_OUTPUT;
    *(s32*)(reader + 0x1C) = (s32)output_count;
    *(u32*)(reader + 0x20) = (u32)source;
    *(s32*)(reader + 0x24) = source_remaining;
    *(u32*)(reader + 0x28) = control;
    *(u32*)(reader + 0x2C) = 0x80000000u | write_position |
                             (pending_read_position << 13) |
                             (pending_count << 26);
    sly1_native_trace_font(22, (u32)reader, output_count,
                           *(u32*)output, control,
                           (u32)source_remaining);
    sly1_native_loader_stage(87, output_count, (u32)reader);
}
