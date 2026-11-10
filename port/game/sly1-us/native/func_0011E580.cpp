/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Port-only native object factory, kept separate from matching game code. */
#include "sly1/host_abi.h"
#include "native_function_lookup.hpp"

extern "C" {
extern u8 D_00247448[];
void* func_0018D5C0(s32 size);
void func_00152470(void* list, void* object);
void* func_00159F20(void* owner, s32 mode);
void sly1_native_call_method(u32 retail_address, u32 object);
}

static void clear_world_slots(void* object) {
    u8* bytes = static_cast<u8*>(object);
    for (u32 i = 0; i < 0x200u * 12u; ++i) bytes[0x84u + i] = 0;
}

static void* resolve_listener(s32 listener_id, void* owner) {
    if (listener_id == -1 || owner == nullptr) return nullptr;
    u8* context = static_cast<u8*>(owner);
    const u32 table = *reinterpret_cast<u32*>(context + 0x1EF4u);
    if (table == 0) return nullptr;
    const u32 value = *reinterpret_cast<u32*>(table + (u32)listener_id * 8u + 4u);
    if (value != 0) {
        const u32 native = sly1::native_function_address(0x0011A6C0u);
        if (native != 0) {
            using Register = void (*)(void*, u32);
            reinterpret_cast<Register>(native)(reinterpret_cast<void*>(0x00259F38u), value);
        }
    }
    return reinterpret_cast<void*>(value);
}

extern "C" void* func_0011E580(s32 type, void* owner, void* parameter,
                                 s32 mode, s32 listener_id) {
    if (type < 0 || type >= 0x95 || (type != 0x6C && owner == nullptr)) return nullptr;

    const u32 descriptor_address = *reinterpret_cast<u32*>(
        D_00247448 + static_cast<u32>(type) * 4u);
    if (descriptor_address == 0) return nullptr;
    const u32 allocation_size = *reinterpret_cast<u32*>(descriptor_address + 0x0Cu);
    if (allocation_size == 0) return nullptr;

    void* object = func_0018D5C0(static_cast<s32>(allocation_size));
    if (object == nullptr) return nullptr;
    u8* bytes = static_cast<u8*>(object);
    *reinterpret_cast<u32*>(bytes + 0x00u) = descriptor_address;
    *reinterpret_cast<u32*>(bytes + 0x08u) = static_cast<u32>(mode);

    if (type == 0x6C) {
        owner = object;
        clear_world_slots(object);
    }
    *reinterpret_cast<u32*>(bytes + 0x18u) = (u32)parameter;
    *reinterpret_cast<u32*>(bytes + 0x14u) = (u32)owner;
    *reinterpret_cast<u32*>(bytes + 0x2Cu) =
        (u32)resolve_listener(listener_id, owner);

    void* owner_list = func_00159F20(owner, mode);
    func_00152470(owner_list, object);
    const u32 owner_slot = static_cast<u32>(type) * 4u + 0x1884u;
    *reinterpret_cast<u32*>(bytes + 0x24u) =
        *reinterpret_cast<u32*>(static_cast<u8*>(owner) + owner_slot);
    *reinterpret_cast<u32*>(static_cast<u8*>(owner) + owner_slot) =
        (u32)object;

    const u32 constructor = *reinterpret_cast<u32*>(descriptor_address + 0x10u);
    if (constructor != 0) sly1_native_call_method(constructor, (u32)object);
    return object;
}

/* Host callers refer to several decompilation prototype spellings for this
 * retail entry. Export each spelling so they all reach the bounded factory. */
#define SLY1_FACTORY_ALIAS(symbol) \
    extern "C" void* sly1_factory_alias_##symbol(s32, void*, void*, s32, s32) \
        __asm__(#symbol); \
    extern "C" void* sly1_factory_alias_##symbol( \
        s32 type, void* owner, void* parameter, s32 mode, s32 listener_id) { \
        return func_0011E580(type, owner, parameter, mode, listener_id); \
    }

SLY1_FACTORY_ALIAS(_Z13func_0011E580P12Alloc_1DC9A0P10Obj_1DC9A0Pvii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iP5WorldP10BaseObjectii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iP5WorldPvii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iiP8Colliderii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iPvS_ii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iPviii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iiP3C03ii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iiPvii)
SLY1_FACTORY_ALIAS(_Z13func_0011E580iiiii)

#undef SLY1_FACTORY_ALIAS
