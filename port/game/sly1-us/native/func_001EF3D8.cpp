// SPDX-License-Identifier: AGPL-3.0-or-later
// The matched notification body dereferences the attachment link before its
// null check on a kind-2 detach for an object that is not in the set. Native
// world teardown can deliver that stale/empty detach; preserve the base
// notification and all normal matching behavior while guarding only this case.
struct AttachedSet;
extern void decomp_HandleAttachedSetNotification(AttachedSet* self, int kind,
                                                  void* message);
extern void func_00127378(void* self, int kind, void* message);

extern "C" void func_001EF3D8(AttachedSet* self, int kind, void* message) {
    if (self == nullptr) return;

    auto* links = reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(self) + 0x550);
    if (kind == 2 && message != self && *links == nullptr) {
        func_00127378(self, kind, message);
        return;
    }

    decomp_HandleAttachedSetNotification(self, kind, message);
}
