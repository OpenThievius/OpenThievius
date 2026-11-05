/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only initialization for Sly's global game-manager vtables. */
#include "common.h"

u32 sly1_native_call_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3);
void func_001F0220(void* object, s32 mode);
extern u8 D_00275F80[];

extern u32 D_0026C6C8;
extern u32 D_0026C948;
extern u32 D_0026CBC8;
extern u32 D_0026CE48;
extern u32 D_0026D0C8;
extern u32 D_0026D348;
extern u32 D_0026D5C8;
extern u32 D_0026D848;
extern u32 D_0026DAC8;
extern u32 D_0026DD48;
extern u32 D_0026DFC8;
extern u32 D_0026E248;
extern u32 D_0026E4E0;
extern u32 D_0026E760;
extern u32 D_0026E9F8;
extern u32 D_0026EC70;
extern u32 D_0026EED0;
extern u32 D_0026F138;
extern u32 D_0026F5A0;
extern u32 D_0026F808;
extern u32 D_0026FA90;
extern u32 D_0026FD00;
extern u32 D_0026FF68;
extern u32 D_002701F8;
extern u32 D_00270650;
extern u32 D_002708E0;
extern u32 D_00271558;
extern u32 D_00272430;
extern u32 D_002726B0;
extern u32 D_00272930;
extern u32 D_00272B98;
extern u32 D_002721D0;
extern u32 D_00272E08;
extern u32 D_002739F0;
extern u32 D_00273C50;
extern u32 D_00273EB0;
extern u32 D_00274118;

void func_001A9C68(void) {
    D_0026C6C8 = 0x00217E30u;
    D_0026C948 = 0x00217E80u;
    D_0026CBC8 = 0x00217ED0u;
    D_0026CE48 = 0x00217F20u;
    D_0026D0C8 = 0x00217F70u;
    D_0026D348 = 0x00217FC0u;
    D_0026D5C8 = 0x002180B0u;
    D_0026D848 = 0x00218010u;
    D_0026DAC8 = 0x00218060u;
    D_0026DD48 = 0x00218100u;
    D_0026DFC8 = 0x00218150u;
    D_0026E248 = 0x002182E0u;
    D_0026E4E0 = 0x002181A0u;
    D_0026E760 = 0x00218510u;
    D_0026E9F8 = 0x00221380u;
    D_0026EC70 = 0x00218330u;
    D_0026EED0 = 0x00218380u;
    D_0026F138 = 0x002183D0u;
    D_0026F5A0 = 0x00218420u;
    D_0026F808 = 0x002210F0u;
    D_0026FA90 = 0x002212E0u;
    D_0026FD00 = 0x00221330u;
    D_0026FF68 = 0x002186D0u;
    D_002701F8 = 0x0021A440u;
    D_00270650 = 0x00220BF0u;
    D_002708E0 = 0x0021A3F0u;
    D_00271558 = 0x0021A3F0u;
    D_00272430 = 0x00218240u;
    D_002726B0 = 0x00218290u;
    D_00272930 = 0x00218470u;
    D_00272B98 = 0x002184C0u;
    D_002721D0 = 0x00218560u;
    D_00272E08 = 0x00218720u;
    D_002739F0 = 0x002185B0u;
    D_00273C50 = 0x002185B0u;
    D_00273EB0 = 0x00218600u;
    D_00274118 = 0x002181F0u;

    for (s32 index = 0x24; index >= 0; --index) {
        u32 object = *(u32*)(0x002486B0u + (u32)index * 4u);
        if (object == 0) continue;
        u32 vtable = *(u32*)object;
        u32 initialize = vtable != 0 ? *(u32*)vtable : 0;
        if (initialize != 0)
            sly1_native_call_regs4(initialize, object, (u32)index, 0, 0);
    }

    *(s32*)(D_00275F80 + 4) = -1;
    func_001F0220(D_00275F80, 0);
}
