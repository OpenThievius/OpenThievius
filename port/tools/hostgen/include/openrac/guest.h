/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AST-only declarations for hostgen's 32-bit MIPS frontend. The compiled host
 * source includes runtime/include/openrac/guest.h instead. */
#ifndef OPENRAC_HOSTGEN_GUEST_H
#define OPENRAC_HOSTGEN_GUEST_H

typedef unsigned int gaddr;
#define uint64_t unsigned long long
#define openrac_guest_call_regs(...) (0ull)

#endif
