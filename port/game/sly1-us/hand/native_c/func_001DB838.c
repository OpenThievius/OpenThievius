/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only backfill for retail world teardown before a resource transition. */
#include "common.h"

extern u32 D_00261990;
extern u32 D_00262E10;
extern u32 D_00262C6C;
extern u32 D_00269C90;
extern u32 D_002640A4;
extern f32 D_00261854;
extern u8 D_0026CE48[];
extern u8 D_0026E760[];
extern u8 D_00275BC0[];
extern s32 D_00261D38;

void func_00192880(s32 index);
void func_0016E148(void);
void func_00143AF0(void* camera);
void func_001DFD70(void);
void func_0017FD70(void);
void func_00191EE8(void);
void func_00169CA0(void);
void func_001C07B0(void);
void func_001B40C8(void);
void func_0011C040(void);
void func_001E9688(void* element_stack);
void func_0018D550(void);
void func_0015F210(s32 mode);
void func_0015EFE8(void* context);
void func_00152440(void* list);

typedef struct NativeList_1DB838 {
    u32 head;
    u32 tail;
    s32 offset;
} NativeList_1DB838;

typedef struct NativeFrameCleanup_1DB838 {
    u8 pad[0x20];
    NativeList_1DB838 frame_lists[3];
    u32 tail;
} NativeFrameCleanup_1DB838;

void func_001DB838(s32 has_world) {
    if (has_world == 0) return;

    func_00192880(-1);
    D_00269C90 = 0;
    D_00262E10 = 0;
    D_00262C6C = 0;
    func_0016E148();
    func_00143AF0((void*)D_00261990);
    func_001DFD70();
    func_0017FD70();
    func_00191EE8();
    func_00169CA0();
    func_001C07B0();
    func_001B40C8();
    func_0011C040();
    func_001E9688(D_00275BC0);
    /* Retire render-list nodes before the front heap is rewound. The native
     * loader can enter this teardown without passing through the retail main
     * loop's usual DrainRenderFrames transition gate. */
    func_0015F210(1);
    {
        NativeFrameCleanup_1DB838 pending = {0};
        NativeList_1DB838* lists = (NativeList_1DB838*)0x002622E0u;
        pending.frame_lists[0] = lists[0];
        pending.frame_lists[1] = lists[1];
        pending.frame_lists[2] = lists[2];
        func_0015EFE8(&pending);
        func_00152440(&lists[0]);
        func_00152440(&lists[1]);
        func_00152440(&lists[2]);
    }
    func_0018D550();

    D_00261990 = 0;
    D_002640A4 = 0;
    D_00261854 = 1.0f;
    *(s32*)(D_0026CE48 + 0x274) = 0;
    *(s32*)(D_0026CE48 + 0x278) = 0;
    *(s32*)(D_0026E760 + 0x280) = 0;
    *(s32*)(D_0026E760 + 0x28C) = 0;
    D_00261D38 = 0;
}
