/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only menu option population for the retail startup page. */
#include "common.h"

#define MENU_TABLE 0x00269D50u
#define MENU_OPTIONS 0x00269CD8u

typedef struct SlyMenuPage {
    u32 title;
    f32 title_scale;
    f32 item_scale;
    s32 vertical;
    s32 count;
    u32 items;
} SlyMenuPage;

void func_001936A0(void* opaque, s32 page) {
    u8* self = (u8*)opaque;
    const s32 active_page = *(s32*)(self + 0x260u);
    if (active_page != 0 || page != 0) return;

    SlyMenuPage* menu = (SlyMenuPage*)MENU_TABLE;
    const s32* candidates = (const s32*)MENU_OPTIONS;
    s32* items = (s32*)menu->items;
    if (menu->items == 0) return;

    /* These are the eight retail startup candidates from D_00269CD8. The
     * state-dependent option filtering is still being backfilled; retaining
     * the canonical entries gives the native menu renderer real labels. */
    for (s32 i = 0; i < 8; ++i) items[i] = candidates[i];
    menu->count = 8;
}
