// SPDX-License-Identifier: AGPL-3.0-or-later
// Host ABI adapters for compiler-generated C++ methods in the Sly sources.
using u32 = unsigned int;

// The freestanding builtins object supplies the exception::what ABI symbol;
// this file keeps only the game-specific value-list adapters below.

namespace sly1 {
struct NativeFunctionAddress {
    u32 retail_address;
    u32 native_address;
};
extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const u32 sly1_native_function_address_count;

static u32 find_native_function(u32 address) {
    u32 first = 0;
    u32 last = sly1_native_function_address_count;
    while (first < last) {
        const u32 middle = first + (last - first) / 2;
        if (sly1_native_function_addresses[middle].retail_address < address)
            first = middle + 1;
        else
            last = middle;
    }
    if (first == sly1_native_function_address_count ||
        sly1_native_function_addresses[first].retail_address != address)
        return 0;
    return sly1_native_function_addresses[first].native_address;
}
} // namespace sly1

// These methods are declared in the decomp's NTSC-U type sources and share
// the same retail value operations used by the adjacent value types. The
// method bodies below preserve that operation through the retail-address
// function map without changing or copying any matching source.
struct Val_114F48 {
    int tag;
    Val_114F48* f4;
    Val_114F48* f8;

    Val_114F48();
    ~Val_114F48();
};

Val_114F48::Val_114F48() {
    const u32 target = sly1::find_native_function(0x0011B008u);
    if (target != 0) reinterpret_cast<void (*)(void*)>(target)(this);
}

Val_114F48::~Val_114F48() {
    const u32 target = sly1::find_native_function(0x0011B060u);
    if (target != 0) reinterpret_cast<void (*)(void*, int)>(target)(this, 2);
}
