// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only world-pool insertion. The first renderer cleanup may run before
// Sly has created a world; in that state the matching caller passes an offset
// from a null D_00275710. Ignore the return until the world pool exists. The
// matching insertion body remains staged and is used once the world is live.
extern "C" void* D_00275710;

extern "C" void func_001B5F28(void* pool, void* item) {
    if (item == nullptr || D_00275710 == nullptr)
        return;

    auto** head = static_cast<void**>(pool);
    *static_cast<void**>(item) = *head;
    *head = item;
}
