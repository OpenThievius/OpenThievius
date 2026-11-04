// SPDX-License-Identifier: AGPL-3.0-or-later
// Freestanding Linux/i386 entry for the no-emulation host32 build.
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

struct NativeFunctionAddress { u32 retail_address; u32 native_address; };
namespace sly1 {
extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const u32 sly1_native_function_address_count;
}
extern "C" void func_001FAE18();
void func_00185758(void*, void*);
extern "C" int sly1_i386_syscall6(int, int, int, int, int, int, int);

namespace {

constexpr int kProtRead = 1;
constexpr int kProtWrite = 2;
constexpr int kMapPrivate = 2;
constexpr int kMapFixedNoReplace = 0x100000;
constexpr int kMapAnonymous = 0x20;
constexpr u32 kPage = 4096;

int syscall1(int number, int a) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a) : "memory", "cc");
    return result;
}

int syscall3(int number, int a, int b, int c) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a), "c"(b), "d"(c) : "memory", "cc");
    return result;
}

int syscall2(int number, int a, int b) {
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number), "b"(a), "c"(b) : "memory", "cc");
    return result;
}

void write_text(const char* text) {
    u32 size = 0;
    while (text[size] != 0) ++size;
    syscall3(4, 2, (int)text, (int)size);
}

void write_hex(const char* prefix, int value) {
    char line[64];
    u32 n = 0;
    while (prefix[n] != 0) { line[n] = prefix[n]; ++n; }
    const char hex[] = "0123456789abcdef";
    line[n++] = '0'; line[n++] = 'x';
    for (int i = 7; i >= 0; --i) line[n++] = hex[((u32)value >> (i * 4)) & 15];
    line[n++] = '\n';
    syscall3(4, 2, (int)line, (int)n);
}

u32 resolve_native(u32 address) {
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.retail_address == address || entry.native_address == address)
            return entry.native_address;
    }
    return 0;
}

u16 read16(const u8* p) { return (u16)p[0] | ((u16)p[1] << 8); }
u32 read32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
void write32(u8* p, u32 v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
u32 align_down(u32 value) { return value & ~(kPage - 1); }
u32 align_up(u32 value) { return (value + kPage - 1) & ~(kPage - 1); }
bool syscall_failed(int result) { return (u32)result >= 0xfffff001u; }

bool load_retail_image(const char* path) {
    const int fd = syscall3(5, (int)path, 0, 0);
    if (fd < 0) { write_text("native host32: cannot open ELF\n"); return false; }
    const int length = syscall3(19, fd, 0, 2);
    if (length < 52) { write_text("native host32: invalid ELF size\n"); syscall1(6, fd); return false; }
    const int image = sly1_i386_syscall6(192, 0, length, kProtRead, kMapPrivate, fd, 0);
    syscall1(6, fd);
    if (syscall_failed(image)) { write_hex("native host32: mmap2 failed ", image); return false; }
    const auto* bytes = (const u8*)image;
    if (bytes[0] != 0x7f || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F' ||
        bytes[4] != 1 || bytes[5] != 1 || read16(bytes + 18) != 8) {
        write_text("native host32: expected 32-bit little-endian MIPS ELF\n");
        return false;
    }
    const u32 phoff = read32(bytes + 28);
    const u16 phentsize = read16(bytes + 42), phnum = read16(bytes + 44);
    if (phentsize < 32 || (u64)phoff + (u64)phentsize * phnum > (u32)length) {
        write_text("native host32: invalid ELF program headers\n");
        return false;
    }
    bool mapped_any = false;
    for (u16 i = 0; i < phnum; ++i) {
        const u8* p = bytes + phoff + (u32)i * phentsize;
        if (read32(p) != 1) continue;
        const u32 offset = read32(p + 4), address = read32(p + 8);
        const u32 file_size = read32(p + 16), memory_size = read32(p + 20);
        if (file_size > memory_size || (u64)offset + file_size > (u32)length ||
            (address & (kPage-1)) != (offset & (kPage-1))) {
            write_text("native host32: invalid ELF load segment\n");
            return false;
        }
        if (memory_size == 0) continue;
        const u32 base = align_down(address);
        const u32 delta = address - base;
        const u32 size = align_up(delta + memory_size);
        const int mapped = sly1_i386_syscall6(192, (int)base, (int)size, kProtRead|kProtWrite,
                                              kMapPrivate|kMapAnonymous|kMapFixedNoReplace, -1, 0);
        if (syscall_failed(mapped) || (u32)mapped != base) {
            write_text("native host32: retail address range unavailable\n");
            return false;
        }
        for (u32 n = 0; n < file_size; ++n) ((u8*)address)[n] = bytes[offset+n];
        for (u32 n = file_size; n < memory_size; ++n) ((u8*)address)[n] = 0;
        mapped_any = true;
    }
    if (!mapped_any) { write_text("native host32: ELF has no load segments\n"); return false; }

    const u32 shoff = read32(bytes + 32);
    const u16 shentsize = read16(bytes + 46), shnum = read16(bytes + 48);
    if (shnum == 0 || shentsize < 40 || (u64)shoff + (u64)shentsize * shnum > (u32)length) {
        write_text("native host32: ELF has no usable section table\n");
        return false;
    }
    for (u16 i = 0; i < shnum; ++i) {
        const u8* s = bytes + shoff + (u32)i * shentsize;
        const u32 type = read32(s + 4), flags = read32(s + 8);
        const u32 address = read32(s + 12), offset = read32(s + 16), size = read32(s + 20);
        if (!(flags & 2) || (flags & 4) || type != 1 || size == 0) continue;
        if ((u64)offset + size > (u32)length) {
            write_text("native host32: data section outside ELF\n");
            return false;
        }
        u32* words = (u32*)address;
        for (u32 j = 0; j < size / 4; ++j) {
            u32 first = 0, last = sly1::sly1_native_function_address_count;
            while (first < last) {
                const u32 middle = first + (last - first) / 2;
                if (sly1::sly1_native_function_addresses[middle].retail_address < words[j]) first = middle + 1;
                else last = middle;
            }
            if (first < sly1::sly1_native_function_address_count &&
                sly1::sly1_native_function_addresses[first].retail_address == words[j] &&
                sly1::sly1_native_function_addresses[first].native_address != 0) {
                words[j] = sly1::sly1_native_function_addresses[first].native_address;
            }
        }
    }
    syscall2(91, image, (int)length);
    return true;
}

} // namespace

extern "C" int sly1_native_host32_main(int argc, char** argv) {
    if (argc != 2) {
        write_text("usage: sly1_host32 <Sly 1 NTSC-U ELF>\n");
        return 2;
    }
    if (!load_retail_image(argv[1])) return 1;
    write_text("native host32: retail data mapped; starting compiled Sly frame path\n");
    func_00185758((void*)*(volatile u32*)0x0027B380u, (void*)0x0027B384u);
    return 0;
}

extern "C" __attribute__((noreturn)) void sly1_missing_function(u32 address) {
    char line[] = "native host32: missing compiled function at 0x00000000\n";
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) line[46 + i] = hex[(address >> ((7-i)*4)) & 15];
    write_text(line);
    syscall1(1, 127);
    __builtin_unreachable();
}
