/* SPDX-License-Identifier: AGPL-3.0-or-later */
// Port-only native frame packet builder for the display worker.
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;
using s32 = int;

extern "C" void func_001611B8(void* packet);
extern "C" void func_001529C8(void* packet, s32 capacity, void* output);
extern "C" void func_00152B38(void* packet);
extern "C" void func_001613C0(void* packet, s32 a, s32 b, s32 c);
extern "C" void func_00161708(void* packet, s32 tag, u64 value);
extern "C" void func_001614C8(void* packet, s32 x, s32 y);
extern "C" void func_001615F0(void* packet, s32 x, s32 y, s32 z, s32 flags);
extern "C" void func_00161200(void* packet);
extern "C" void func_00152D30(void* packet);
extern "C" void func_00152D70(void* packet);
extern "C" void func_001686E8(s32 channel, void* packet);

extern u64 D_002625F0[];
extern u64 D_00262500[];
extern u64 D_002624F0[];
extern u64 D_002625E0[];
extern s32 D_002626C0;
extern s32 D_002626C8;
extern s32 D_00261F1C;

extern "C" void func_0015F320(void) {
    // The 80-row loop below emits four 16-byte records per row. Keep enough
    // native packet storage for the full frame; the retail stack-frame-sized
    // scratch range was too small once those records were built host-side.
    alignas(16) u8 storage[0x2830]{};
    void* packet = storage;
    u32* output = reinterpret_cast<u32*>(storage + 0x30);

    func_001611B8(packet);
    func_001529C8(packet, 0x280, output);
    func_00152B38(packet);
    func_001613C0(packet, 0, 1, 0x0E);

    const u64 frame = (D_002626C0 & 1) ? D_002625F0[0] : D_00262500[0];
    const u64 destination = (D_002626C0 & 1) ? D_002624F0[0] : D_002625E0[0];

    func_00161708(packet, 0x3F, 0);
    const u64 texture = (frame & 0x0F000000ULL) | (frame & 0x1FFULL) |
                        (0x8000ULL << 17);
    func_00161708(packet, 0x4E, texture);

    const u64 transfer = (((destination >> 16) & 0x3FULL) << 14) |
                         ((destination & 0x1FFULL) << 5) |
                         (((destination >> 24) & 0x3FULL) << 20) |
                         (0xAA00ULL << 18);
    func_00161708(packet, 0x06, transfer);
    func_00161708(packet, 0x14, 0x61);
    func_00161708(packet, 0x08, 0x05);
    func_00161708(packet, 0x47, 0x00030002ULL);
    func_00161708(packet, 0x42, 0x0000003000000064ULL);
    func_00161708(packet, 0x01, 0x3F80000080808080ULL);
    func_00161708(packet, 0x40, 0x00DE00010000027FULL);
    func_001613C0(packet, 0x156, 2, 0x43);

    const s32 row_stride = (D_002626C8 & 1) ? 0 : 8;
    for (s32 row = 0; row < 0x280; row += 8) {
        func_001614C8(packet, row * 16, row_stride);
        func_001615F0(packet, (row + 0x6C0) * 16, 0x7900, 0, 0);
        func_001614C8(packet, (row + 0x20) * 16, row_stride + 0xE00);
        func_001615F0(packet, (row + 0x6E0) * 16, 0x8700, 0, 0);
    }

    func_001613C0(packet, 0, 1, 0x0E);
    func_00161708(packet, 0x4E, (frame & 0x0F000000ULL) | (frame & 0x1FFULL));
    func_00161708(packet, 0x61, 0);
    func_00161200(packet);
    func_00152D30(packet);
    func_00152D70(packet);
    func_001686E8(D_00261F1C, output);
}
