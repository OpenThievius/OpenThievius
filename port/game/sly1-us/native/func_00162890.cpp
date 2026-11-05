// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only short path for the retail VU0 component-data loader. The initial
// empty-list layout is reconstructed from the US routine; nonempty records
// still require the handwritten 0x00165AE8/VU path and stop explicitly.
#include "common.h"

extern "C" s32 func_00137B90(void* reader);
extern "C" s32 func_00137BE0(void* reader);
extern "C" s32 func_00137D10(void* reader);
extern "C" void func_00137A60(void* reader, s32 size, void* destination);
extern "C" void* func_0018D5C0(s32 size);
extern "C" void* func_0018D568(s32 size, void* previous);
extern "C" __attribute__((noreturn)) void sly1_missing_function(
    u32 address, u32 caller, u32 argument0, u32 argument1, u32 argument2);

extern "C" void func_00162890(void* block, void* reader, void* object) {
    auto* bytes = static_cast<u8*>(block);
    *reinterpret_cast<s32*>(bytes + 0x50) = 0;

    // The first two bytes are retained only in the retail stack frame; the
    // third is a count of 0x90-byte component records.
    const s32 first = func_00137B90(reader);
    const s32 second = func_00137B90(reader);
    const s32 component_count = func_00137B90(reader);
    (void)first;
    (void)second;
    if (component_count < 0 || component_count > 4096)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));

    *reinterpret_cast<s32*>(bytes + 0x00) = component_count;
    *reinterpret_cast<void**>(bytes + 0x04) = func_0018D5C0(component_count * 0x90);
    auto* component_ids = static_cast<s16*>(func_0018D5C0(component_count * 4));
    *reinterpret_cast<void**>(bytes + 0x08) = component_ids;
    for (s32 i = 0; i < component_count; ++i)
        component_ids[i] = static_cast<s16>(func_00137D10(reader));

    const s32 word_count = func_00137B90(reader);
    if (word_count < 0 || word_count > 65535)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
    *reinterpret_cast<s32*>(bytes + 0x40) = word_count;
    void* words = func_0018D5C0(word_count * 4);
    *reinterpret_cast<void**>(bytes + 0x44) = words;
    func_00137A60(reader, word_count * 4, words);
    *reinterpret_cast<void**>(bytes + 0x48) = func_0018D568(word_count * 4, words);

    const s32 record_count = func_00137BE0(reader);
    if (record_count < 0 || record_count > 65535)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
    *reinterpret_cast<s32*>(bytes + 0x0C) = record_count;
    *reinterpret_cast<void**>(bytes + 0x10) = func_0018D5C0(record_count * 0x70);
    *reinterpret_cast<void**>(bytes + 0x14) = func_0018D5C0(record_count * 0x28);
    if (record_count != 0)
        sly1_missing_function(0x00162890u,
            reinterpret_cast<u32>(__builtin_return_address(0)),
            reinterpret_cast<u32>(block), reinterpret_cast<u32>(reader),
            reinterpret_cast<u32>(object));
}
