/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only resource callback translated from the NTSC-U retail path. */
#include "common.h"

#define SLY1_MAX_RESOURCE_BYTES 0x10000000u

extern u8 D_00261EF0[];
extern void* D_00261990;
extern s32 D_00275714;
extern void* D_00275710;
extern u32 D_0027B314;
extern u8 D_002BDC20[];
extern u8 D_00275BC0[];
extern u8 D_0026F808[];
extern void* D_002623C0;

void func_00151A88(void* context);
void func_0011BF70(void);
s32 func_00137B90(void* reader);
s32 func_00137D10(void* reader);
s32 func_00137C40(void* reader);
void func_001DCCC0(void* reader);
void* func_0011E580(s32 type, void* owner, void* parameter, s32 mode,
                    s32 listener_id);
void sly1_native_call_method(u32 address, u32 object);
void sly1_native_loader_stage(u32 stage, u32 object, u32 reader);
void sly1_native_trace_font(u32 stage, u32 a0, u32 a1, u32 a2, u32 a3,
                            u32 a4);
void func_001C1190(void* object, void* reader);
void func_0011EC40(void* object, void* reader);
void func_00138550(void* reader, void* output);
void func_001384A0(void* reader, void* output);
void func_001B3040(void* reader);
void func_0011ECB8(void* world, void* parameter, void* reader);
void func_001DBA38(void* reader);
int func_001BE268(int bank);
void func_001BECF8(void);
void func_001BEB10(s32 value);
void func_001B3450(void);
s32 func_00152720(void* list);
void func_001DC9A0(void* object);
void func_00168770(void* context, u8* value, s32 flag);
void func_001DCD28(void* object);
void func_001DBE48(void* object);
void func_00151D28(void* context);
void func_001E90E8(void* context);
void* func_0018D5C0(s32 size);
void func_00144270(void* object);
void func_00138C00(void* object, s32 mode);
void func_001BEC58(s32 value);
void func_001C0B38(void* object);

#ifndef SLY1_RETAIL_ASM_FUNC_001DCEA0
static void call_descriptor_method(void* object, u32 slot) {
    u32 descriptor;
    u32 address;
    if (object == 0) return;
    descriptor = *(u32*)object;
    if (descriptor == 0) return;
    address = *(u32*)(descriptor + slot);
    if (address != 0) sly1_native_call_method(address, (u32)object);
}

void func_001DCEA0(void* self, void* reader) {
    u8* object = (u8*)self;
    s32 index;
    void* child;
    void* node;
    u32 saved_context;

    func_00151A88(D_00261EF0);
    sly1_native_loader_stage(15, (u32)object, (u32)reader);
    func_0011BF70();
    sly1_native_loader_stage(16, (u32)object, (u32)reader);
    D_00275714 = func_00137B90(reader);
    sly1_native_loader_stage(17, (u32)object, (u32)reader);
    *(s32*)(object + 0x1D70) = func_00137D10(reader);
    sly1_native_loader_stage(18, (u32)object, (u32)reader);
    if (D_00275714 != 0 &&
        *(s32*)(reader + 0x1C) >= 4 &&
        *(u32*)(*(u32*)(reader + 0x18)) > SLY1_MAX_RESOURCE_BYTES / 2u + 1u) {
        /* Each skipped retail entry consumes at least a 16-bit length. A
         * count beyond half the port's maximum accepted resource size cannot
         * fit in any supported stream; consume the field and keep the native
         * loader from spinning billions of times on an invalid Splash table. */
        (void)func_00137C40(reader);
        sly1_native_loader_stage(20, (u32)object, (u32)reader);
    } else {
        func_001DCCC0(reader);
    }
    sly1_native_loader_stage(19, (u32)object, (u32)reader);
    sly1_native_loader_stage(1, (u32)object, (u32)reader);

    D_00261990 = func_0011E580(0x6D, self, 0, 4, -1);
    for (index = 0; index < 10; ++index) {
        child = func_0011E580(0x1E, self, 0, 10, -1);
        call_descriptor_method(child, 0x1C);
    }
    for (index = 0; index < 40; ++index) {
        child = func_0011E580(0x4D, self, 0, 11, -1);
        call_descriptor_method(child, 0x1C);
    }
    sly1_native_loader_stage(2, (u32)object, (u32)reader);

    func_001C1190(self, reader);
    func_0011EC40(self, reader);
    sly1_native_loader_stage(3, (u32)object, (u32)reader);
    func_00138550(reader, object + 0x2310);
    sly1_native_loader_stage(143, 0, (u32)reader);
    func_001384A0(reader, object + 0x2314);
    sly1_native_loader_stage(144, 0, (u32)reader);
    func_00138550(reader, object + 0x1D74);
    sly1_native_loader_stage(145, 0, (u32)reader);
    func_001B3040(reader);
    sly1_native_loader_stage(146, 0, (u32)reader);
    sly1_native_trace_font(23, (u32)reader, *(u32*)(reader + 0x18),
                           *(u32*)(reader + 0x1C), *(u32*)(reader + 0x20),
                           *(u32*)(reader + 0x24));
    {
        u32 cursor = *(u32*)(reader + 0x18);
        s32 remaining = *(s32*)(reader + 0x1C);
        u32 word0 = cursor != 0 && remaining >= 4 ? *(u32*)cursor : 0;
        u32 word1 = cursor != 0 && remaining >= 8 ? *(u32*)(cursor + 4) : 0;
        sly1_native_trace_font(61u, (u32)reader, cursor, (u32)remaining,
                               word0, word1);
    }
    /* Retail passes the world instance, a null factory parameter, and reader.
     * The previous port call passed the camera as owner and shifted the other
     * arguments, leaving every parsed object detached from the world lists. */
    func_0011ECB8(self, 0, reader);
    sly1_native_trace_font(24, (u32)reader, *(u32*)(reader + 0x18),
                           *(u32*)(reader + 0x1C), *(u32*)(reader + 0x20),
                           *(u32*)(reader + 0x24));
    func_001DBA38(reader);

    if (*(s32*)(object + 0x1D70) >= 0)
        (void)func_001BE268(*(s32*)(object + 0x1D70));
    if (*(u32*)(object + 0x1D78) == 0) func_001BECF8();
    func_001BEB10(*(s32*)(object + 0x1D74));
    func_001B3450();
    sly1_native_loader_stage(4, (u32)object, (u32)reader);

    if (func_00152720(object + 0x1BE8)) func_001DC9A0(self);
    *(u8*)(object + 0x1D67) = 0x80;
    func_00168770(D_002BDC20, object + 0x1D64, 0);

    *(f32*)(object + 0x1D68) *= 0.0039215689f;
    *(f32*)(object + 0x1D6C) *= 0.0039215689f;
    func_001DCD28(self);
    func_001DBE48(self);

    node = *(void**)(object + 0x48);
    while (node != 0) {
        call_descriptor_method(node, 0x4C);
        node = *(void**)((u8*)node + 0x1C);
    }
    func_001DBE48(self);

    saved_context = D_0027B314;
    D_0027B314 = (u32)(object + 0x48);
    func_00151D28(D_00261EF0);
    func_001E90E8(D_00275BC0);

    if (*(u32*)(D_0026F808 + 0x260) == 0 && D_002623C0 != 0 &&
        *(u32*)((u8*)D_002623C0 + 0x19D8) != 0) {
        (void)func_0018D5C0(0x80000);
    }
    if (D_00261990 != 0) func_00144270(D_00261990);
    /* Temporary port-only bypass: current active collider list contains
     * malformed records and exact retail contact update faults before the
     * first native frame. Keep this diagnostic override separate from decomp. */
    sly1_native_trace_font(179u, (u32)object, 0, 0, 0, 0);
    func_001BEC58(*(s32*)(object + 0x1D74));
    func_001C0B38(self);
    D_0027B314 = saved_context;
    sly1_native_loader_stage(5, (u32)object, (u32)reader);
}
#endif
