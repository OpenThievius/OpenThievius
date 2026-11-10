// SPDX-License-Identifier: AGPL-3.0-or-later
// Native Linux replacement for Sly's EE EndOwnThread system-call stub.
#include "sly1/host_abi.h"

extern "C" [[noreturn]] void func_001F6A30(void) {
    __asm__ __volatile__("movl $1, %%eax\n\t"
                         "xorl %%ebx, %%ebx\n\t"
                         "int $0x80"
                         :
                         :
                         : "eax", "ebx", "memory");
    __builtin_unreachable();
}
