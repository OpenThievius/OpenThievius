// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only object lookup replacement. It walks the retail ID hash bucket and
// applies the decompiled scope filter; group-member expansion is still a
// separate follow-up because the US helper is not decompiled yet.
#include "common.h"

extern "C" void* func_00159F20(void* world, s32 id);
extern "C" void func_00159F48(void* object, s32 kind, s32 all, s32 mode,
                               void* root, s32 max, s32* count,
                               void** found, s32* minimum_level);

extern "C" s32 func_0015A198(void* world, s32 flags, s32 id, void* context,
                              s32 max_results, void** results) {
    if (results != nullptr) *results = nullptr;
    if (id == -1 || (context == nullptr && (flags & 5) == 0)) return 0;

    const s32 kind = flags & 0xFF;
    s32 count = 0;
    s32 minimum_level = 0x7FFFFFFF;
    void** bucket = static_cast<void**>(func_00159F20(world, id));
    if (bucket == nullptr) return 0;

    auto* object = static_cast<u8*>(*bucket);
    while (object != nullptr) {
        if (*reinterpret_cast<s32*>(object + 0x08) == id) {
            const u32 descriptor = *reinterpret_cast<u32*>(object);
            const u32 descriptor_flags = descriptor != 0
                ? *reinterpret_cast<u32*>(descriptor + 0x08) : 0;

            if ((descriptor_flags & 0x100u) == 0) {
                func_00159F48(object, kind, (flags & 0x100) != 0, 0,
                              context, max_results, &count, results,
                              &minimum_level);
            } else {
                // Group descriptors hold their members at +0x2D0 as linked
                // records whose first word is the member object.
                auto* member = *reinterpret_cast<u8**>(object + 0x2D0);
                while (member != nullptr) {
                    func_00159F48(*reinterpret_cast<void**>(member), kind,
                                  (flags & 0x100) != 0, 1, context,
                                  max_results, &count, results,
                                  &minimum_level);
                    member = *reinterpret_cast<u8**>(member + 8);
                }
            }
        }
        object = *reinterpret_cast<u8**>(object + 0x24);
    }

    if ((flags & 0x200) == 0 && max_results < count) count = max_results;
    return count;
}
