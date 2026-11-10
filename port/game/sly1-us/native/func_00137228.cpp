// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host implementation of the text-layout position setter.
// The matching source defines an inline method but the direct host build does
// not emit it from text_panel.cpp; keep this boundary here until it does.
#include "common.h"

struct NativeTextLayout137228 {
    f32 x;
    f32 y;
};

extern "C" void func_00137228(NativeTextLayout137228* self, f32 x, f32 y) {
    self->x = x;
    self->y = y;
}

extern "C" void func_00137238(NativeTextLayout137228* self, f32 w, f32 h) {
    auto* values = reinterpret_cast<f32*>(self);
    values[2] = w;
    values[3] = h;
}

extern "C" void func_00137248(NativeTextLayout137228* self, const s32* color) {
    reinterpret_cast<u32*>(self)[4] = static_cast<u32>(*color);
}

extern "C" void func_00137260(NativeTextLayout137228* self, s32 align) {
    reinterpret_cast<s32*>(self)[5] = align;
}

extern "C" void func_00137268(NativeTextLayout137228* self, void* align) {
    reinterpret_cast<void**>(self)[6] = align;
}
