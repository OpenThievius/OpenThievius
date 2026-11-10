/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Port-only host replacement for the 64-bit unsigned division helper.
using u64 = unsigned long long;

extern "C" u64 func_001FBE00(u64 value, u64 radix) {
    return radix == 0 ? 0 : value / radix;
}
