/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only native resource object record reader. */
#include "sly1/host_abi.h"
#include "native_function_lookup.hpp"

extern "C" {
s32 func_00137D10(void* reader);
void* func_0011E580(s32 slot, void* world, void* parameter, s32 key, s32 flag);
void func_00182ED8(void* object);
void sly1_native_loader_stage(u32 stage, u32 value, u32 reader);
}

static u32 call_retail_regs4(u32 address, u32 a0, u32 a1, u32 a2, u32 a3) {
    const u32 native = sly1::native_function_address(address);
    if (native == 0) return 0;
    using Function = u32 (*)(u32, u32, u32, u32);
    return reinterpret_cast<Function>(native)(a0, a1, a2, a3);
}

extern "C" void func_0011ECB8(void* world, void* parameter, void* reader) {
    s32 count = func_00137D10(reader);
    sly1_native_loader_stage(21, static_cast<u32>(count), (u32)reader);
    while (count-- > 0) {
        const s32 slot = func_00137D10(reader);
        const s32 key = func_00137D10(reader);
        const s32 flag = func_00137D10(reader);
        sly1_native_loader_stage(22, static_cast<u32>(slot), (u32)reader);
        sly1_native_loader_stage(23, static_cast<u32>(key), (u32)reader);
        sly1_native_loader_stage(24, static_cast<u32>(flag), (u32)reader);

        void* object = func_0011E580(slot, world, parameter, key, flag);
        sly1_native_loader_stage(25, (u32)object, (u32)reader);
        if (object == nullptr) continue;

        const u32 vtable = *reinterpret_cast<u32*>(object);
        const u32 read_method = vtable != 0 ? *reinterpret_cast<u32*>(vtable + 14u * 4u) : 0;
        if (read_method == 0x0013B598u) {
            call_retail_regs4(0x001BAC30u, (u32)object, (u32)reader, 0, 0);
            call_retail_regs4(0x00159478u, (u32)object + 0x5BCu,
                              (u32)object, 0, 0);
        } else if (read_method != 0) {
            call_retail_regs4(read_method, (u32)object, (u32)reader, 0, 0);
        }

        const s32 index = key - 12;
        if (index >= 0 && index < 31) {
            *reinterpret_cast<u32*>((u32)world + 0x1CE8u +
                                    static_cast<u32>(index) * 4u) =
                (u32)object;
            func_00182ED8(object);
        }
    }
}

/* These prototypes describe the same retail entry in different source units.
 * Export both host symbols so the null-safe port reader overrides each one. */
extern "C" void func_0011ECB8_C01(void* world, void* parameter, void* reader)
    __asm__("_Z13func_0011ECB8P3C6CPvS1_");
extern "C" void func_0011ECB8_C01(void* world, void* parameter, void* reader) {
    func_0011ECB8(world, parameter, reader);
}

extern "C" void func_0011ECB8_stream(void* world, void* parameter, void* reader)
    __asm__("_Z13func_0011ECB8PvP3C53P8Stream53");
extern "C" void func_0011ECB8_stream(void* world, void* parameter, void* reader) {
    func_0011ECB8(world, parameter, reader);
}

extern "C" void func_0011ECB8_world_base(void* world, void* parameter, void* reader)
    __asm__("_Z13func_0011ECB8P5WorldP10BaseObjectP14BufferedStream");
extern "C" void func_0011ECB8_world_base(void* world, void* parameter, void* reader) {
    func_0011ECB8(world, parameter, reader);
}

extern "C" void func_0011ECB8_group(void* world, void* parameter, void* reader)
    __asm__("_Z13func_0011ECB8PvP11ObjectGroupP10LoadStream");
extern "C" void func_0011ECB8_group(void* world, void* parameter, void* reader) {
    func_0011ECB8(world, parameter, reader);
}

extern "C" void func_0011ECB8_placed(void* world, void* parameter, void* reader)
    __asm__("_Z13func_0011ECB8P5WorldP12PlacedObjectPv");
extern "C" void func_0011ECB8_placed(void* world, void* parameter, void* reader) {
    func_0011ECB8(world, parameter, reader);
}
