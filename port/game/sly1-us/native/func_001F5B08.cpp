// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host replacement for the still-assembly libc memmove routine.
#include "sly1/host_abi.h"

extern "C" void* func_001F5B08(void* destination, const void* source, u32 size) {
    volatile u8* out = static_cast<volatile u8*>(destination);
    const volatile u8* in = static_cast<const volatile u8*>(source);
    const u32 dst = static_cast<u32>(reinterpret_cast<unsigned long>(destination));
    const u32 src = static_cast<u32>(reinterpret_cast<unsigned long>(source));

    if (dst <= src || dst - src >= size) {
        for (u32 i = 0; i < size; ++i) out[i] = in[i];
    } else {
        for (u32 i = size; i != 0; --i) out[i - 1] = in[i - 1];
    }
    return destination;
}
