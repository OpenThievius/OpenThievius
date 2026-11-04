/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Clang AST declarations for the target C library when hostgen runs -nostdinc. */
#ifndef OPENRAC_HOSTGEN_STRING_H
#define OPENRAC_HOSTGEN_STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t size);
void *memmove(void *destination, const void *source, size_t size);
void *memset(void *destination, int value, size_t size);
int memcmp(const void *left, const void *right, size_t size);
size_t strlen(const char *text);

#endif
