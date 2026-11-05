// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only fallback for the missing EE vector-track evaluator. Preserve the
// vanilla optional-buffer initialization while leaving interpolated values at
// their stable defaults until the math path can be translated faithfully.
#include "common.h"

extern "C" void func_0011F6B0(void* tracks, void* owner, s32 context,
                               void* input, void* output0, void* output1,
                               f32 factor, f32 weight) {
    (void)tracks;
    (void)owner;
    (void)context;
    (void)factor;
    (void)weight;
    if (output0 != nullptr) {
        auto* dst = static_cast<u8*>(output0);
        auto* src = static_cast<const u8*>(input);
        for (s32 i = 0; i < 16; ++i) dst[i] = src[i];
    }
    if (output1 != nullptr) {
        auto* dst = static_cast<u8*>(output1);
        for (s32 i = 0; i < 16; ++i) dst[i] = 0;
    }
}
