// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only bridges used by the native resource-loader backfill.
#include "sly1/host_abi.h"
#include "native_function_lookup.hpp"
#if !defined(SLY1_HOST32_FREESTANDING)
#include <openrac/guest.h>
#include <cstdio>
#include <cstdlib>
#endif

extern "C" void sly1_native_call_method(u32 retail_address, u32 object) {
    const u32 native_address = sly1::native_function_address(retail_address);
    if (native_address == 0) return;
    using Method = void (*)(void*);
    reinterpret_cast<Method>(native_address)(reinterpret_cast<void*>(object));
}

extern "C" void sly1_native_loader_stage(u32 stage, u32 object, u32 reader) {
#if defined(SLY1_HOST32_FREESTANDING)
    extern void sly1_native_trace_loader_stage(u32, u32, u32);
    sly1_native_trace_loader_stage(stage, object, reader);
#else
    if (stage == 160u) {
        if (std::getenv("OPENRAC_TRACE_LOADER") == nullptr) return;
        std::fprintf(stderr,
                     "[heap] exhausted request=%u current=%#010x limit=%#010x\n",
                     object, reader, GREF(u32, 0x002699A8u));
        return;
    }
    if (std::getenv("OPENRAC_STARTUP_TRACE") == nullptr || reader == 0) return;
    const u32 cursor = GREF(u32, reader + 0x18u);
    const s32 remaining = GREF(s32, reader + 0x1Cu);
    const u32 source = GREF(u32, reader + 0x20u);
    const s32 source_remaining = GREF(s32, reader + 0x24u);
    const u32 next_word = remaining >= 4 && cursor != 0 ? GREF(u32, cursor) : 0;
    std::fprintf(stderr,
                 "[loader] stage=%u object=%08x reader=%08x mode=%d cursor=%08x "
                 "remaining=%d next=%08x source=%08x source_remaining=%d\n",
                 stage, object, reader, GREF(s32, reader + 4u), cursor,
                 remaining, next_word, source, source_remaining);
#endif
}
