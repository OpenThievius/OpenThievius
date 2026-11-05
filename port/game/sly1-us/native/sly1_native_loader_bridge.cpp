// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only bridges used by the native resource-loader backfill.
#include "sly1/host_abi.h"
#include "native_function_lookup.hpp"

extern "C" void sly1_native_call_method(u32 retail_address, u32 object) {
    const u32 native_address = sly1::native_function_address(retail_address);
    if (native_address == 0) return;
    using Method = void (*)(void*);
    reinterpret_cast<Method>(native_address)(reinterpret_cast<void*>(object));
}

extern "C" void sly1_native_loader_stage(u32 stage, u32 object, u32 reader) {
    (void)stage;
    (void)object;
    (void)reader;
}
