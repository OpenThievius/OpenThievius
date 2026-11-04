// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native backfill for Sly's NTSC-U allocator range setup.
void func_001E22C0(void* allocator_state);

extern "C" void func_0018D430() {
    constexpr unsigned int kHeapBase = 0x0064C700u;
    constexpr unsigned int kHeapSize = 0x00200000u;

    *reinterpret_cast<unsigned int*>(0x002C2A10u) = kHeapBase;
    *reinterpret_cast<unsigned int*>(0x002C2A14u) = kHeapBase;
    *reinterpret_cast<unsigned int*>(0x002C2A18u) = kHeapBase;
    *reinterpret_cast<unsigned int*>(0x002C2A1Cu) = kHeapBase;
    *reinterpret_cast<unsigned int*>(0x002C2A20u) = kHeapSize;
    *reinterpret_cast<unsigned int*>(0x002C2A24u) = kHeapSize;
    *reinterpret_cast<unsigned int*>(0x002C2A34u) = 0xFFFFFFFFu;
    *reinterpret_cast<unsigned int*>(0x002699A4u) = kHeapBase;
    *reinterpret_cast<unsigned int*>(0x002699A8u) = kHeapSize;

    func_001E22C0(reinterpret_cast<void*>(0x002C2A28u));
}
