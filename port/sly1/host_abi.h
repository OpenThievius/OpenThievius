/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef SLY1_HOST_ABI_H
#define SLY1_HOST_ABI_H

/* Host-only aliases for direct i386 builds of the PS2 C++ source. */
#define COMMON_H
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;

/* GCC i386 lacks scalar TImode; Sly uses mode(TI) values for 16-byte copies. */
#define mode(x) vector_size(16)

#endif
