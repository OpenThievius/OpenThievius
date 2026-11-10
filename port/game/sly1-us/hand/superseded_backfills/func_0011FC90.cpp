// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only fallback for the missing position reader. Preserve the serialized
// position tracks through the matching reader; native startup currently uses
// a conservative false result until the PS2 vector-distance query is translated.
#include "common.h"

extern "C" void func_0011F798(void* tracks, void* values, void* reader);

extern "C" void func_0011FC90(void* object, void* reader) {
    auto* bytes = static_cast<u8*>(object);
    func_0011F798(bytes + 0x20, bytes + 0x10, reader);
    *reinterpret_cast<s32*>(bytes + 0x08) = 0;
}
