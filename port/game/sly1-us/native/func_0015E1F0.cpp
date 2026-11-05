// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill for the US decomp's current near-match. Measure the
// widest rendered line by dispatching each character through the active font.
#include "s15db28.h"

void func_0015DB80(S15DB28* self);
s32 func_0015DBE0(S15DB28* self);

f32 func_0015E1F0(void* opaque) {
    S15DB28* self = (S15DB28*)opaque;
    f32 maximum = 0.0f;
    f32 line_width = 0.0f;

    func_0015DB80(self);
    s32 kind;
    while ((kind = func_0015DBE0(self)) != 0) {
        if (kind == 10) {
            if (line_width > maximum) maximum = line_width;
            line_width = 0.0f;
        } else {
            V24A2* font = self->font;
            const V24A2_Vt* vtable = font->vt;
            V24A2* adjusted_font = (V24A2*)((u8*)font + vtable->adj3);
            line_width += vtable->width(adjusted_font, kind);
        }
    }
    if (line_width > maximum) maximum = line_width;
    return maximum;
}
