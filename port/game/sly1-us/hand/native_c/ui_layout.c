/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only layout initialization for Sly's registered overlay objects. */
#include "common.h"

#define UI_OBJECTS 0x002486B0u
#define UI_PLACEMENTS 0x0026C3E0u
#define UI_CLOCK 0x00261868u

extern u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
extern f32 sly1_native_call_f32(u32 address, u32 a0, u32 a1, u32 a2,
                                u32 a3, f32 f12, f32 f14);

static u32 ui_object(s32 index) {
    if (index < 0 || index >= 37) return 0;
    return *(u32*)(UI_OBJECTS + (u32)index * 4u);
}

static s32 ui_objects_overlap(u32 a, u32 b) {
    u32 vt = *(u32*)a;
    u32 method = vt != 0 ? *(u32*)(vt + 0x48u) : 0;
    return method != 0 && sly1_native_call_regs4(method, a, b, 0, 0) != 0;
}

s32 func_001AAAC8(void* first, void* second) {
    const s32 a = *(s32*)((u8*)first + 0x250u);
    const s32 b = *(s32*)((u8*)second + 0x250u);
    const s32 a_active = (u32)(a - 1) < 2u;
    const s32 b_active = (u32)(b - 1) < 2u;
    if (a_active && b_active) return 1;
    if (a == 3 && b == 3) return 1;
    if (b == 3 && a == 2) return 1;
    return 0;
}

f32 func_001EAF28(f32 value) {
    if (value <= 0.0f) return 0.0f;
    return (f32)(s32)value;
}

void func_001AAEC0(void* changed);

void func_001AAB60(void* opaque) {
    u8* self = (u8*)opaque;
    u32 place_address = *(u32*)(self + 0x218u);
    u8* place = (u8*)place_address;
    f32 x0 = 0.0f, x1 = 640.0f, y0 = 0.0f, y1 = 492.80002f;
    if (place_address == 0) return;

    s32 anchor = *(s32*)(place + 0x0Cu);
    for (s32 depth = 0; anchor >= 0 && anchor < 37 && depth < 37; ++depth) {
        u32 other = ui_object(anchor);
        if (other != 0 && ui_objects_overlap(other, (u32)self)) {
            const s32 relation = *(s32*)(place + 0x10u);
            const f32 width = *(f32*)(other + 0x234u);
            const f32 height = *(f32*)(other + 0x238u);
            if (relation == 0 && width != 0.0f)
                x0 = *(f32*)(other + 0x21Cu) + width;
            else if (relation == 1 && width != 0.0f)
                x1 = *(f32*)(other + 0x21Cu);
            else if (relation == 2 && height != 0.0f)
                y0 = *(f32*)(other + 0x220u) + height;
            else if (relation == 3 && height != 0.0f)
                y1 = *(f32*)(other + 0x220u);
            break;
        }
        place = (u8*)(UI_PLACEMENTS + (u32)anchor * 20u);
        anchor = *(s32*)(place + 0x0Cu);
    }

    const f32 width = *(f32*)(self + 0x234u);
    const f32 height = *(f32*)(self + 0x238u);
    const f32 dx = *(f32*)(*(u32*)(self + 0x218u));
    const f32 dy = *(f32*)(*(u32*)(self + 0x218u) + 4u);
    const f32 shown_x = dx == 0.0f ? (x1 - x0 - width) * 0.5f
                                  : (dx > 0.0f ? x0 + dx : x1 + dx - width);
    const f32 raw_y = dy == 0.0f ? (y1 - y0 - height) * 0.5f
                                : (dy > 0.0f ? y0 + dy : y1 + dy - height);
    const f32 shown_y = func_001EAF28(raw_y * 0.9090909f) * 1.1f;
    const s32 from = *(s32*)(*(u32*)(self + 0x218u) + 8u);

    *(f32*)(self + 0x224u) = shown_x;
    *(f32*)(self + 0x228u) = shown_y;
    *(f32*)(self + 0x22Cu) = shown_x;
    *(f32*)(self + 0x230u) = shown_y;
    if (from == 0) *(f32*)(self + 0x22Cu) = -width;
    else if (from == 1) *(f32*)(self + 0x22Cu) = 640.0f;
    else if (from == 2) *(f32*)(self + 0x230u) = -height;
    else if (from == 3) *(f32*)(self + 0x230u) = 492.80002f;

    const s32 mode = *(s32*)(self + 0x250u);
    if (mode == 2) {
        *(f32*)(self + 0x21Cu) = shown_x;
        *(f32*)(self + 0x220u) = shown_y;
    } else if (mode == 0) {
        *(f32*)(self + 0x21Cu) = *(f32*)(self + 0x22Cu);
        *(f32*)(self + 0x220u) = *(f32*)(self + 0x230u);
    }
    func_001AAEC0(self);
}

void func_001AAB20(void* object, f32 width, f32 height) {
    u8* self = (u8*)object;
    if (width >= 0.0f) *(f32*)(self + 0x234u) = width;
    if (height >= 0.0f) *(f32*)(self + 0x238u) = height;
    func_001AAB60(object);
}

void func_001AAEC0(void* changed) {
    for (s32 i = 0; i < 37; ++i) {
        u8* place = (u8*)(UI_PLACEMENTS + (u32)i * 20u);
        const s32 anchor = *(s32*)(place + 0x0Cu);
        if (anchor >= 0 && anchor < 37 && ui_object(anchor) == (u32)changed) {
            const u32 object = ui_object(i);
            if (object != 0) func_001AAB60((void*)object);
        }
    }
}

f32 func_001AA898(void* opaque) {
    return *(f32*)((u8*)opaque + 0x240u);
}

f32 func_001AA8A0(void* opaque) {
    return *(f32*)((u8*)opaque + 0x244u);
}

f32 func_001AA8A8(void* opaque) {
    return *(f32*)((u8*)opaque + 0x248u);
}

static f32 call_ui_float_method(u32 object, u32 slot) {
    const u32 vtable = *(u32*)object;
    const u32 method = vtable != 0 ? *(u32*)(vtable + slot) : 0;
    return method != 0
        ? sly1_native_call_f32(method, object, 0, 0, 0, 0.0f, 0.0f)
        : 0.0f;
}

void func_001AA298(void* opaque) {
    u8* self = (u8*)opaque;
    const u32 object = (u32)opaque;
    const u32 vtable = *(u32*)object;
    s32 mode = *(s32*)(self + 0x250u);
    s32 next_mode = mode;
    const u32 clock = *(u32*)(self + 0x258u);
    if (clock == 0 || vtable == 0) return;
    const f32 time = *(f32*)clock;
    const f32 since = *(f32*)(self + 0x254u);
    const f32 offset = *(f32*)(self + 0x24Cu);
    f32 duration;

    if (mode == 2) {
        duration = call_ui_float_method(object, 0x30u);
        if (duration != 0.0f && duration <= time - since) next_mode = 3;
    } else if (mode == 1) {
        duration = call_ui_float_method(object, 0x2Cu);
        if (duration != 0.0f) {
            f32 blend = (time - since + offset) / duration;
            if (blend < 0.0f) blend = 0.0f;
            else if (blend > 1.0f) blend = 1.0f;
            *(f32*)(self + 0x23Cu) = blend;
        }
        if (*(f32*)(self + 0x23Cu) >= 1.0f) next_mode = 2;
    } else if (mode == 3) {
        duration = call_ui_float_method(object, 0x34u);
        if (duration != 0.0f) {
            f32 blend = (time - since + offset) / duration;
            if (blend < 0.0f) blend = 0.0f;
            else if (blend > 1.0f) blend = 1.0f;
            *(f32*)(self + 0x23Cu) = 1.0f - blend;
        }
        if (*(f32*)(self + 0x23Cu) <= 0.0f) next_mode = 0;
    }

    if (next_mode == mode) {
        const f32 blend = *(f32*)(self + 0x23Cu);
        const f32 easing = blend * (2.0f - blend);
        const f32 hidden_x = *(f32*)(self + 0x22Cu);
        const f32 hidden_y = *(f32*)(self + 0x230u);
        *(f32*)(self + 0x21Cu) = hidden_x + easing * (*(f32*)(self + 0x224u) - hidden_x);
        *(f32*)(self + 0x220u) = hidden_y + easing * (*(f32*)(self + 0x228u) - hidden_y);
        func_001AAEC0(self);
    }

    const u32 set_mode = *(u32*)(vtable + 0x40u);
    if (set_mode != 0)
        (void)sly1_native_call_regs4(set_mode, object, (u32)next_mode, 0, 0);
}

void func_001AA1F0(void* opaque, s32 index) {
    u8* self = (u8*)opaque;
    *(u32*)(self + 0x218u) = UI_PLACEMENTS + (u32)index * 20u;
    *(f32*)(self + 0x248u) = 0.25f;
    *(f32*)(self + 0x240u) = 0.25f;
    *(u32*)(self + 0x258u) = UI_CLOCK;
    *(s32*)(self + 0x214u) = index;
    func_001AAB20(self, *(f32*)(self + 0x234u), *(f32*)(self + 0x238u));
    *(f32*)(self + 0x21Cu) = *(f32*)(self + 0x22Cu);
    *(f32*)(self + 0x220u) = *(f32*)(self + 0x230u);
}
