// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill for Sly's NTSC-U global constructor callback.
#include "native_function_lookup.hpp"

extern "C" void func_00211DE0() {
    const unsigned int native = sly1::native_function_address(0x001FFBE0u);
    if (native == 0) return;
    using ConstructorTarget = void (*)(int, void*);
    reinterpret_cast<ConstructorTarget>(native)(
        0x0027B198, reinterpret_cast<void*>(0x0062C540));
}
