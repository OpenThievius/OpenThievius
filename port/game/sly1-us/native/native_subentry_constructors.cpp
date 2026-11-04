// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only callbacks for five NTSC-U constructor-table interior entries.
#include "native_function_lookup.hpp"

namespace {

void invoke_constructor_target(unsigned int address) {
    const unsigned int native = sly1::native_function_address(address);
    if (native == 0) return;
    using ConstructorTarget = int (*)(int, int);
    reinterpret_cast<ConstructorTarget>(native)(1, -1);
}

}  // namespace

extern "C" void func_0016C450() { invoke_constructor_target(0x0016C360u); }
extern "C" void func_001963B8() { invoke_constructor_target(0x00196330u); }
extern "C" void func_001AEC90() { invoke_constructor_target(0x001AEA70u); }
extern "C" void func_001E08D8() { invoke_constructor_target(0x001E0848u); }
extern "C" void func_001F2738() { invoke_constructor_target(0x001F2688u); }
