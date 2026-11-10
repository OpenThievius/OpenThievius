// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only backfill for the US decomp's current near-match. Measure the
// widest rendered line by dispatching each character through the active font.
#include "text_reader.h"

void func_0015DB80(TextReader* self);
s32 func_0015DBE0(TextReader* self);

f32 func_0015E1F0(void* opaque) {
    TextReader* self = (TextReader*)opaque;
    f32 maximum = 0.0f;
    f32 line_width = 0.0f;

    func_0015DB80(self);
    s32 kind;
    while ((kind = func_0015DBE0(self)) != 0) {
        if (kind == 10) {
            if (line_width > maximum) maximum = line_width;
            line_width = 0.0f;
        } else {
            Font* font = self->font;
            const FontVt* vtable = font->vt;
            Font* adjusted_font = (Font*)((u8*)font + vtable->adj3);
            line_width += vtable->width(adjusted_font, kind);
        }
    }
    if (line_width > maximum) maximum = line_width;
    return maximum;
}
