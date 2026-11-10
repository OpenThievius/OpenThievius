/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Port-only native implementation of Sly's unsigned 64-bit remainder helper.
using u64 = unsigned long long;

extern "C" u64 func_001FC3D0(u64 value, u64 radix) {
    return radix == 0 ? 0 : value % radix;
}
