// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only packet-writer backfill. The decompilation models this call through
// a C++ virtual overlay whose vptr overlaps V24A0's end pointer. On the native
// host that dispatch reads packet bytes as a function address. Use V24A0's
// explicit retail vtable, close the open tag, then append the two-word GIF tag.
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

struct V24A0;
struct V24A0_Vt {
    short adj0;
    short idx0;
    void* fn0;
    short adj1;
    short idx1;
    void (*close)(V24A0*, int);
};

struct V24A0 {
    u8* end;
    u8* begin;
    u32* open_tag;
    union {
        u8* cur;
        u64* cur64;
        u32 cur_u32;
    };
    u32 align_packets;
    u32 active;
    const V24A0_Vt* vt;
    union {
        u32* open_header;
        u8* block;
    };
    u32 words_per_loop;
    u32 words_written;
};

extern "C" void func_00161958(void* context, int count) {
    auto* writer = static_cast<V24A0*>(context);
    const V24A0_Vt* vt = writer->vt;
    if (vt != nullptr && vt->close != nullptr) {
        auto* adjusted = reinterpret_cast<V24A0*>(
            reinterpret_cast<u8*>(writer) + vt->adj1);
        vt->close(adjusted, 0);
    }

    u64* packet = writer->cur64;
    writer->cur64 = packet + 2;
    packet[0] = static_cast<u32>(count) | (static_cast<u64>(2) << 58);
    packet[1] = 0;
}
