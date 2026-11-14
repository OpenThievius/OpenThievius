// SPDX-License-Identifier: AGPL-3.0-or-later
// Freestanding Linux/i386 entry for the no-emulation host32 build.
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
extern "C" char* getenv(const char*);

#if defined(OPENRAC_SLY1_WITH_SDL)
#include "openrac/native_window.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <signal.h>
#include <thread>
#include <ucontext.h>
extern "C" unsigned long long openrac_native_renderer_dma_submit_count();
extern "C" unsigned long long openrac_native_renderer_dma_accept_count();
extern "C" unsigned long long openrac_native_renderer_dma_reject_count();
extern "C" unsigned long long openrac_native_renderer_dma_bytes();
extern "C" int sly1_native_critical_owner();
extern "C" unsigned int sly1_native_critical_depth();
extern "C" unsigned int sly1_native_critical_spins();
#endif

struct NativeFunctionAddress { u32 retail_address; u32 native_address; };
namespace sly1 {
extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const u32 sly1_native_function_address_count;
}
extern "C" void func_001FAE18();
extern "C" int sly1_native_host32_crt_initialized = 0;
#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_host32_trace_dma_call(
    unsigned int channel, unsigned int source, unsigned int caller,
    unsigned int gif_channel) {
    const char* enabled = std::getenv("OPENRAC_TRACE_DMA_CALL");
    if (enabled == nullptr || enabled[0] == '0') return;
    std::fprintf(stderr,
                 "[native direct DMA] channel=%#010x source=%#010x caller=%#010x gif=%#010x\n",
                 channel, source, caller, gif_channel);
}
extern "C" void sly1_native_host32_trace_dma_channel(
    unsigned int index, unsigned int address, unsigned int caller) {
    const char* enabled = std::getenv("OPENRAC_TRACE_DMA_CALL");
    if (enabled == nullptr || enabled[0] == '0') return;
    std::fprintf(stderr,
                 "[native direct DMA channel] index=%u address=%#010x caller=%#010x\n",
                 index, address, caller);
}
extern "C" void sly1_native_host32_wait_frame(void) {
    // The retail main loop is paced by the PS2 display cycle. SDL owns the
    // native display cadence here, so keep the game thread at that cadence.
    SDL_Delay(16);
}
#endif
void func_00185758(int, int);
extern "C" int sly1_i386_syscall6(int, int, int, int, int, int, int);
#if !defined(OPENRAC_SLY1_WITH_SDL)
extern "C" __attribute__((weak)) void openrac_native_renderer_submit_dma_chain(u32) {}
#endif

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

bool syscall_failed(int result);

#if defined(OPENRAC_SLY1_WITH_SDL)
__attribute__((no_instrument_function)) void native_fault_trace(
    int signal_number, siginfo_t* info, void* context) {
    const auto* state = static_cast<const ucontext_t*>(context);
    write_hex("native host32: fatal signal ", signal_number);
    write_hex("native host32: fault address ", (int)info->si_addr);
    write_hex("native host32: fault eip ", state->uc_mcontext.gregs[REG_EIP]);
    write_hex("native host32: fault esp ", state->uc_mcontext.gregs[REG_ESP]);
    write_hex("native host32: fault ebp ", state->uc_mcontext.gregs[REG_EBP]);
    write_hex("native host32: fault eax ", state->uc_mcontext.gregs[REG_EAX]);
    write_hex("native host32: fault ebx ", state->uc_mcontext.gregs[REG_EBX]);
    write_hex("native host32: fault ecx ", state->uc_mcontext.gregs[REG_ECX]);
    write_hex("native host32: fault edx ", state->uc_mcontext.gregs[REG_EDX]);
    write_hex("native host32: fault esi ", state->uc_mcontext.gregs[REG_ESI]);
    write_hex("native host32: fault edi ", state->uc_mcontext.gregs[REG_EDI]);
    u32* stack = reinterpret_cast<u32*>(state->uc_mcontext.gregs[REG_ESP]);
    for (u32 index = 0; index < 16; ++index)
        write_hex("native host32: fault stack word ", stack[index]);
    u32* frame = reinterpret_cast<u32*>(state->uc_mcontext.gregs[REG_EBP]);
    for (u32 depth = 0; depth < 8 && frame != nullptr; ++depth) {
        const u32 previous = frame[0];
        const u32 caller = frame[1];
        write_hex("native host32: fault caller ", caller);
        if (previous <= (u32)frame || previous - (u32)frame > 0x10000u) break;
        frame = reinterpret_cast<u32*>(previous);
    }
    syscall1(252, 128 + signal_number);
}

void install_fault_trace() {
    struct sigaction action{};
    action.sa_sigaction = native_fault_trace;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
}

int make_game_constants_writable(dl_phdr_info* image, size_t, void*) {
    if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
    for (ElfW(Half) i = 0; i < image->dlpi_phnum; ++i) {
        const ElfW(Phdr)& segment = image->dlpi_phdr[i];
        if (segment.p_type != PT_LOAD || (segment.p_flags & PF_R) == 0 ||
            (segment.p_flags & (PF_W | PF_X)) != 0) continue;
        const u32 begin = (u32)(image->dlpi_addr + segment.p_vaddr) & ~(kPage - 1);
        const u32 limit = (u32)(image->dlpi_addr + segment.p_vaddr + segment.p_memsz);
        const u32 end = (limit + kPage - 1) & ~(kPage - 1);
        const int result = syscall3(125, (int)begin, (int)(end - begin),
                                    kProtRead | kProtWrite);
        if (syscall_failed(result)) {
            write_hex("native host32: writable constants mprotect error ", result);
        } else {
            write_hex("native host32: writable constants begin ", (int)begin);
            write_hex("native host32: writable constants end ", (int)end);
        }
    }
    return 1;
}

void make_game_constants_writable() {
    dl_iterate_phdr(make_game_constants_writable, nullptr);
}
#endif

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

u64 divide_u64_u32(u64 numerator, u32 denominator) {
    u64 quotient = 0;
    u32 remainder = 0;
    if (denominator == 0) return 0;
    for (int bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | (u32)((numerator >> bit) & 1);
        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= (u64)1 << bit;
        }
    }
    return quotient;
}

int g_disc_fd = -1;
u32 g_disc_stride = 0;
u32 g_disc_payload = 0;
u32 g_disc_sector_count = 0;
int g_disc_read_error = 0;
volatile u32 g_disc_read_count = 0;
volatile u32 g_disc_sectors_read = 0;
extern "C" volatile u32 sly1_native_iop_boot_stage = 0;
extern "C" volatile u32 sly1_native_startup_callback_index = 0;
extern "C" volatile u32 D_002613F0;
bool g_startup_trace_enabled = false;
volatile u32 g_native_frame_stage = 0;
volatile u32 g_native_frame_count = 0;
volatile u32 g_native_world = 0;
volatile u32 g_native_ui_state = 0;
volatile u32 g_native_ui_stack_count = 0;
volatile u32 g_native_ui_resume_state = 0;
volatile u32 g_native_world_load_request = 0;
volatile u32 g_native_movie_queued = 0;
volatile u32 g_native_active_ui = 0;
volatile u32 g_native_text_ui = 0;
volatile u32 g_native_pad_status = 0;
volatile u32 g_native_pad_mode = 0;
volatile u32 g_native_pad_flags = 0;
volatile u32 g_native_pause_open = 0;
volatile u32 g_native_pause_page = 0;
volatile u32 g_native_pause_mode = 0;
volatile u32 g_native_fade_mode = 0;
volatile u32 g_native_stack_top = 0;
volatile u32 g_native_stack_top_vt = 0;
volatile u32 g_native_stack_top_mode = 0;
volatile u32 g_native_stack_top_text = 0;
volatile u32 g_native_pad_routing = 0;
volatile u32 g_native_pad_select = 0;
volatile u32 g_native_blank_pad_status = 0;
volatile u32 g_native_card_state = 0;
volatile u32 g_native_card_target = 0;
volatile u32 g_native_card_display = 0;
volatile u32 g_native_card_progress = 0;
volatile u32 g_native_memory_gate = 0;
volatile u32 g_native_element_state = 0;
volatile u32 g_native_card_thread = 0;
volatile u32 g_native_card_job = 0;
volatile u32 g_native_card_result = 0;
volatile u32 g_native_card_semaphore = 0;
volatile u32 g_native_elapsed_bits = 0;
volatile u32 g_native_card_state_time_bits = 0;
volatile u32 g_native_card_display_time_bits = 0;

bool probe_disc_signature(u32 offset) {
    u8 signature[6]{};
    const int bytes = sly1_i386_syscall6(180, g_disc_fd, (int)signature,
                                         sizeof(signature), (int)offset, 0, 0);
    return bytes == (int)sizeof(signature) && signature[0] == 1 &&
           signature[1] == 'C' && signature[2] == 'D' &&
           signature[3] == '0' && signature[4] == '0' && signature[5] == '1';
}

bool open_disc_image(const char* path) {
    constexpr int kOpenLargeFile = 0x8000;
    g_disc_fd = syscall3(5, (int)path, kOpenLargeFile, 0);
    if (g_disc_fd < 0) { write_text("native host32: ISO open failed\n"); return false; }
    u64 end = 0;
    const int seek_result = sly1_i386_syscall6(140, g_disc_fd, 0, 0,
                                               (int)&end, 2, 0);
    if (seek_result != 0 || end == 0) { write_text("native host32: ISO size query failed\n"); return false; }

    constexpr u32 kPvdSector = 16;
    if (probe_disc_signature(kPvdSector * 2048)) {
        g_disc_stride = 2048;
        g_disc_payload = 0;
    } else if (probe_disc_signature(kPvdSector * 2352 + 16)) {
        g_disc_stride = 2352;
        g_disc_payload = 16;
    } else {
        write_text("native host32: ISO PVD probe failed\n");
        return false;
    }
    g_disc_sector_count = (u32)divide_u64_u32(end, g_disc_stride);
    return g_disc_sector_count != 0;
}

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
    const u32 guest_ram_base = 0x00010000u;
    const u32 guest_ram_size = 0x01ff0000u;
    const int guest_ram = sly1_i386_syscall6(
        192, (int)guest_ram_base, (int)guest_ram_size, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(guest_ram) || (u32)guest_ram != guest_ram_base) {
        write_text("native host32: retail EE RAM address range unavailable\n");
        return false;
    }
    const int scratchpad = sly1_i386_syscall6(
        192, 0x70000000, 0x4000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(scratchpad) || (u32)scratchpad != 0x70000000u) {
        write_text("native host32: scratchpad address range unavailable\n");
        return false;
    }
    // Back the GS privileged register aperture with native shadow RAM. Sly's
    // matching display setup writes PMODE/SMODE2/DISPFB/DISPLAY/BGCOLOR here;
    // native GIF packets are consumed by the SDL renderer separately.
    const int gs_registers = sly1_i386_syscall6(
        192, 0x12000000, 0x2000, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(gs_registers) || (u32)gs_registers != 0x12000000u) {
        write_text("native host32: GS register shadow address unavailable\n");
        return false;
    }
    *(volatile u32*)0x12001000u = 0;
    // The native DMA service completes submissions synchronously. Sly still
    // clears this VIF0 CHCR register directly in matching frame code, so map
    // its page as a zeroed status shadow for those host-native accesses.
    const int vif0_registers = sly1_i386_syscall6(
        192, 0x10003000, kPage, kProtRead | kProtWrite,
        kMapPrivate | kMapAnonymous | kMapFixedNoReplace, -1, 0);
    if (syscall_failed(vif0_registers) || (u32)vif0_registers != 0x10003000u) {
        write_text("native host32: VIF0 register shadow address unavailable\n");
        return false;
    }
    *(volatile u32*)0x10003010u = 0;
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
        // PS2 executable images commonly carry the ELF header as a writable
        // PT_LOAD at virtual address zero. It is file metadata, not EE RAM;
        // the native process deliberately leaves the null page unmapped.
        if (address == 0 && file_size == 0x1000 && memory_size == 0x1000) continue;
        if (address < guest_ram_base || address >= guest_ram_base + guest_ram_size ||
            memory_size > guest_ram_base + guest_ram_size - address) {
            write_text("native host32: ELF segment lies outside mapped EE RAM\n");
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
        if (address == 0) continue; // .elf_header is not part of the running game image.
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
    if (argc != 3 && argc != 4) {
        write_text("usage: sly1_host32 <Sly 1 NTSC-U ELF> <ISO or BIN> [--trace]\n");
        return 2;
    }
    g_startup_trace_enabled = argc == 4 && argv[3][0] == '-' && argv[3][1] == '-' &&
                              argv[3][2] == 't';
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (g_startup_trace_enabled) install_fault_trace();
#endif
    if (!load_retail_image(argv[1])) return 1;
    if (!open_disc_image(argv[2])) {
        write_text("native host32: cannot open ISO 9660 disc image\n");
        return 1;
    }
#if defined(OPENRAC_SLY1_WITH_SDL)
    make_game_constants_writable();
#endif
    write_text("native host32: retail data and ISO mapped; starting compiled Sly frame path\n");
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (!openrac_native_window_create()) {
        write_text("native host32: SDL3 renderer startup failed\n");
        return 1;
    }
    std::atomic<bool> game_finished{false};
    std::thread game_thread([&] {
        func_00185758((int)*(volatile u32*)0x0027B380u, (int)0x0027B384u);
        game_finished.store(true, std::memory_order_release);
    });
    const bool trace_graphics = [] {
        const char* value = std::getenv("OPENRAC_STARTUP_TRACE");
        return value != nullptr && value[0] != '0';
    }();
    unsigned trace_frames = 0;
    for (;;) {
        openrac_native_window_present();
        // GS CSR.FIELD is the only status bit read by the Sly display path.
        // Advance it with the native window's presentation cadence.
        __atomic_fetch_xor((u32*)0x12001000u, 1u << 13, __ATOMIC_RELAXED);
        if (openrac_native_window_poll_input()) {
            break;
        }
        if (trace_graphics && (++trace_frames % 60u) == 0) {
            std::fprintf(stderr, "[native graphics] dma=%llu accepted=%llu rejected=%llu bytes=%llu\n",
                         openrac_native_renderer_dma_submit_count(),
                         openrac_native_renderer_dma_accept_count(),
                         openrac_native_renderer_dma_reject_count(),
                         openrac_native_renderer_dma_bytes());
            std::fprintf(stderr,
                         "[native startup] entry_returned=%u callback=%u iop_stage=%u cd_reads=%u sectors=%u game_frames=%u frame_stage=%u world=%#x ui_state=%u ui_stack=%u resume=%d load=%u movie=%u visible_ui=%u text_ui=%u pad_status=%u pad_mode=%u pad_flags=%#x pad_route=%u pad_select=%u blank_pad_status=%u pause_open=%u pause_page=%d pause_mode=%u fade=%u stack_top=%#x top_vt=%#x top_mode=%u top_text=%u card_state=%d card_target=%d card_display=%u card_progress=%d memory_gate=%u element_state=%u card_thread=%u card_job=%#x card_result=%d card_semaphore=%u game_elapsed_bits=%#x card_state_time_bits=%#x card_display_time_bits=%#x\n",
                         game_finished.load(std::memory_order_acquire) ? 1u : 0u,
                         sly1_native_startup_callback_index,
                         sly1_native_iop_boot_stage,
                         g_disc_read_count, g_disc_sectors_read,
                         __atomic_load_n(&g_native_frame_count, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_frame_stage, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_world, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_ui_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_ui_stack_count, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_ui_resume_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_world_load_request, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_movie_queued, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_active_ui, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_text_ui, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_status, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_flags, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_routing, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pad_select, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_blank_pad_status, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pause_open, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_pause_page, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_pause_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_fade_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_vt, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_mode, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_stack_top_text, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_state, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_target, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_display, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_progress, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_memory_gate, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_element_state, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_thread, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_job, __ATOMIC_RELAXED),
                         (int)__atomic_load_n(&g_native_card_result, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_semaphore, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_elapsed_bits, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_state_time_bits, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_card_display_time_bits, __ATOMIC_RELAXED));
            std::fprintf(stderr, "[native critical] owner=%d depth=%u spins=%u\n",
                         sly1_native_critical_owner(), sly1_native_critical_depth(),
                         sly1_native_critical_spins());
        }
        SDL_Delay(16);
    }
    game_thread.detach();
    openrac_native_window_destroy();
    // Native guest worker services may still be running. Exit the process as
    // a unit rather than racing their use of C++ globals during CRT teardown.
    syscall1(1, 0);
    __builtin_unreachable();
#else
    func_00185758((int)*(volatile u32*)0x0027B380u,
                  (int)0x0027B384u);
    return 0;
#endif
}

extern "C" int sly1_host_cd_read(u32 sector, u32 count, u32 destination) {
    ++g_disc_read_count;
    g_disc_sectors_read += count;
    if (g_disc_fd < 0 || sector > g_disc_sector_count ||
        count > g_disc_sector_count - sector) {
        g_disc_read_error = -1;
        return 0;
    }
    for (u32 i = 0; i < count; ++i) {
        const u64 offset = (u64)(sector + i) * g_disc_stride + g_disc_payload;
        const int bytes = sly1_i386_syscall6(180, g_disc_fd,
                                             (int)(destination + i * 2048), 2048,
                                             (int)offset, (int)(offset >> 32), 0);
        if (bytes != 2048) {
            g_disc_read_error = bytes < 0 ? bytes : -1;
            return 0;
        }
    }
    g_disc_read_error = 0;
    return 1;
}

extern "C" __attribute__((noreturn)) void sly1_missing_function(
    u32 address, u32 caller, u32 argument0, u32 argument1, u32 argument2) {
    write_hex("native host32: missing compiled function at ", (int)address);
    write_hex("native host32: called from ", (int)caller);
    write_hex("native host32: first argument ", (int)argument0);
    write_hex("native host32: second argument ", (int)argument1);
    write_hex("native host32: third argument ", (int)argument2);
    if (address == 0x00162890u && argument1 >= 0x10000000u &&
        argument1 < 0xF0000000u) {
        if (argument0 >= 0x00100000u && argument0 < 0x02100000u) {
            const auto* block = reinterpret_cast<volatile const u32*>(argument0);
            write_hex("native host32: GLOBSET list count ", (int)block[0]);
            write_hex("native host32: GLOBSET record count ", (int)block[3]);
            write_hex("native host32: GLOBSET pose count ", (int)block[16]);
            if (block[3] != 0 && block[4] >= 0x00100000u &&
                block[4] < 0x02100000u) {
                for (u32 i = 0; i < block[3] && i < 6; ++i) {
                    const auto* record = reinterpret_cast<volatile const u32*>(
                        block[4] + i * 0x70u);
                    write_hex("native host32: GLOB flags ", (int)record[26]);
                    write_hex("native host32: GLOB nested count ", (int)record[22]);
                    if (record[22] != 0 && record[23] >= 0x00100000u &&
                        record[23] < 0x02100000u) {
                        const auto* nested = reinterpret_cast<volatile const u32*>(record[23]);
                        for (u32 j = 0; j < record[22] && j < 3; ++j) {
                            const auto* entry = nested + j * 20;
                            write_hex("native host32: nested type ", (int)entry[5]);
                            write_hex("native host32: nested reference ", (int)entry[6]);
                            write_hex("native host32: nested reference count ", (int)entry[9]);
                        }
                    }
                }
            }
        }
        const auto* reader = reinterpret_cast<volatile const u32*>(argument1);
        const u32 cursor = reader[6];
        write_hex("native host32: reader flags ", (int)reader[0]);
        write_hex("native host32: reader mode ", (int)reader[1]);
        write_hex("native host32: reader end ", (int)reader[2]);
        write_hex("native host32: reader start ", (int)reader[3]);
        write_hex("native host32: reader cursor ", (int)cursor);
        write_hex("native host32: reader remaining ", (int)reader[7]);
        write_hex("native host32: reader next ", (int)reader[8]);
        write_hex("native host32: reader next count ", (int)reader[9]);
        write_hex("native host32: reader f28 ", (int)reader[10]);
        write_hex("native host32: reader f2c ", (int)reader[11]);
        if (cursor >= 0x10000000u && cursor < 0xF0000000u) {
            const auto* bytes = reinterpret_cast<volatile const u8*>(cursor);
            for (u32 i = 0; i < 16; ++i)
                write_hex("native host32: reader byte ", bytes[i]);
        }
    }
    if (address == 0x001841D0u && argument0 >= 0x00100000u &&
        argument0 < 0x02100000u) {
        const auto* camera = reinterpret_cast<volatile const u32*>(argument0);
        static const u32 offsets[] = {
            0x2D0u, 0x2D4u, 0x2E0u, 0x2E4u, 0x2E8u, 0x2ECu,
            0x2F0u, 0x2F4u, 0x2F8u, 0x2FCu, 0x300u, 0x304u,
            0x320u, 0x324u, 0x328u, 0x32Cu};
        for (u32 i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
            const u32 offset = offsets[i];
            write_hex("native host32: C51+offset ", (int)offset);
            write_hex("native host32: C51 value ", (int)camera[offset / 4]);
        }
    }
    syscall1(1, 127);
    __builtin_unreachable();
}

extern "C" void sly1_port_trace_reader(u32 stage, void* reader_ptr) {
    if (!g_startup_trace_enabled) return;
    const auto* reader = reinterpret_cast<volatile const u32*>(reader_ptr);
    write_hex("native host32: GLOBSET reader stage ", (int)stage);
    write_hex("native host32: GLOBSET reader cursor ", (int)reader[6]);
    write_hex("native host32: GLOBSET reader remaining ", (int)reader[7]);
    if (stage == 0xb4) {
        const auto* bytes = reinterpret_cast<volatile const u8*>(reader[6]);
        for (u32 i = 0; i < 16; ++i)
            write_hex("native host32: scene next byte ", bytes[i]);
    }
}

extern "C" void sly1_native_trace_loader_stage(u32 stage, u32 value,
                                                   u32 reader_ptr) {
    if (!g_startup_trace_enabled || reader_ptr == 0) return;
    // Keep the high volume object field reads out of the log. These points
    // bracket the scene record parsers and identify their decoded values.
    if (stage != 3 && stage != 4 && (stage < 21 || stage > 28) &&
        (stage < 29 || stage > 33)) return;
    const auto* reader = reinterpret_cast<volatile const u32*>(reader_ptr);
    write_hex("native host32: loader stage ", (int)stage);
    write_hex("native host32: loader value ", (int)value);
    write_hex("native host32: loader cursor ", (int)reader[6]);
    write_hex("native host32: loader remaining ", (int)reader[7]);
    if (stage == 3 || (stage >= 29 && stage <= 33) ||
        stage == 21 || stage == 22 || stage == 23 || stage == 24) {
        const auto* bytes = reinterpret_cast<volatile const u8*>(reader[6]);
        write_hex("native host32: loader next halfword ",
                  (int)((u32)bytes[0] | ((u32)bytes[1] << 8)));
    }
}

extern "C" void sly1_port_trace_nested_counts(
    u32 count0, u32 count1, u32 count2, u32 count3, u32 count4) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: nested count 0 ", (int)count0);
    write_hex("native host32: nested count 1 ", (int)count1);
    write_hex("native host32: nested count 2 ", (int)count2);
    write_hex("native host32: nested count 3 ", (int)count3);
    write_hex("native host32: nested count 4 ", (int)count4);
}

extern "C" void sly1_port_trace_value(u32 tag, u32 value) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: trace tag ", (int)tag);
    write_hex("native host32: trace value ", (int)value);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_script_host_call(u32 function, u32 self, u32 count) {
    if (!g_startup_trace_enabled || self < 0x00100000u || self >= 0x12000000u)
        return;
    const auto* object = reinterpret_cast<volatile const u32*>(self);
    const u32 descriptor = object[0];
    if (descriptor < 0x00100000u || descriptor >= 0x12000000u) return;
    const auto* head = reinterpret_cast<volatile const u32*>(descriptor);
    if (head[1] != 0x57u) return;
    write_hex("native host32: notifier script host function ", (int)function);
    write_hex("native host32: notifier script host self ", (int)self);
    write_hex("native host32: notifier script host argument count ", (int)count);
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address == function) {
            write_hex("native host32: notifier script host retail function ",
                      (int)entry.retail_address);
            break;
        }
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_script_builtin(u32 function, u32 count, u32 args, u32 scope) {
    if (!g_startup_trace_enabled) return;
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address != function) continue;
        if (entry.retail_address != 0x0012c7d8u &&
            entry.retail_address != 0x00101a90u) return;
        write_hex("native host32: animation script builtin target ",
                  (int)entry.retail_address);
        write_hex("native host32: animation script builtin count ", (int)count);
        write_hex("native host32: animation script builtin args ", (int)args);
        write_hex("native host32: animation script builtin scope ", (int)scope);
        return;
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_startup(u32 index, u32 phase, u32 callback) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: startup callback index ", (int)index);
    write_hex("native host32: startup callback phase ", (int)phase);
    if (callback != 0)
        write_hex("native host32: startup callback address ", (int)callback);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_resource_state(u32 self, u32 pending, u32 world) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: resource self ", (int)self);
    write_hex("native host32: resource pending ", (int)pending);
    write_hex("native host32: current world ", (int)world);
    if (self >= 0x08048000u && self < 0x09000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(self);
        for (u32 i = 0; i < 13; ++i)
            write_hex("native host32: resource field ", (int)words[i]);
    }
    if (pending >= 0x08048000u && pending < 0x09000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(pending);
        for (u32 i = 0; i < 8; ++i)
            write_hex("native host32: pending blob word ", (int)words[i]);
    }
    if (world >= 0x08048000u && world < 0x09000000u)
        write_hex("native host32: current world state ",
                  (int)*(volatile const u32*)(world + 0x1D7Cu));
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_group_child(u32 type, u32 object, u32 descriptor, u32 read_method) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: group child type ", (int)type);
    write_hex("native host32: group child object ", (int)object);
    write_hex("native host32: group child descriptor ", (int)descriptor);
    write_hex("native host32: group child read method ", (int)read_method);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_path_curve(u32 kind, u32 curve, u32 vtable, u32 read_method) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: path curve kind ", (int)kind);
    write_hex("native host32: path curve object ", (int)curve);
    write_hex("native host32: path curve vtable ", (int)vtable);
    write_hex("native host32: path curve reader ", (int)read_method);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_capped_list(u32 stage, u32 count, u32 list, u32 stream) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: capped-list stage ", (int)stage);
    write_hex("native host32: capped-list count ", (int)count);
    write_hex("native host32: capped-list object ", (int)list);
    write_hex("native host32: capped-list stream ", (int)stream);
    if (stream >= 0x00100000u && stream < 0x12000000u) {
        const auto* words = reinterpret_cast<volatile const u32*>(stream);
        write_hex("native host32: capped-list cursor ", (int)words[6]);
        write_hex("native host32: capped-list remaining ", (int)words[7]);
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_c02_state(u32 tag, u32 self) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: C02 trace point ", (int)tag);
    write_hex("native host32: C02 object ", (int)self);
    if (self < 0x00010000u || self >= 0x12000000u) return;
    const auto* words = reinterpret_cast<volatile const u32*>(self);
    write_hex("native host32: C02 descriptor ", (int)words[0]);
    write_hex("native host32: C02 parent ", (int)words[6]);
    write_hex("native host32: C02 world ", (int)words[5]);
    write_hex("native host32: C02 root ", (int)words[20]);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_c02_list(u32 tag, u32 list, u32 item, u32 result) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: C02 list trace point ", (int)tag);
    write_hex("native host32: C02 list address ", (int)list);
    write_hex("native host32: C02 list result ", (int)result);
    if (list < 0x00010000u || list >= 0x12000000u ||
        item < 0x00010000u || item >= 0x12000000u) return;
    const auto* fields = reinterpret_cast<volatile const u32*>(list);
    const u32 offset = fields[2];
    write_hex("native host32: C02 list head ", (int)fields[0]);
    write_hex("native host32: C02 list tail ", (int)fields[1]);
    write_hex("native host32: C02 list offset ", (int)offset);
    if (offset < 0x100000u) {
        const auto* link = reinterpret_cast<volatile const u32*>(item + offset);
        write_hex("native host32: C02 item next ", (int)link[0]);
        write_hex("native host32: C02 item prev ", (int)link[1]);
    }
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_sorter(u32 stage, u32 world, u32 axis, u32 value) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: sorter stage ", (int)stage);
    write_hex("native host32: sorter world ", (int)world);
    write_hex("native host32: sorter axis ", (int)axis);
    write_hex("native host32: sorter value ", (int)value);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_frame_stage(u32 stage) {
    __atomic_store_n(&g_native_frame_stage, stage, __ATOMIC_RELAXED);
    if (stage == 8)
        __atomic_add_fetch(&g_native_frame_count, 1u, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_frame_state(u32 world, u32 state, u32 count, u32 resume,
                              u32 load_request, u32 movie, u32 visible_ui,
                              u32 text_ui, u32 pad_status, u32 pad_mode,
                              u32 pad_flags, u32 pause_open, u32 pause_page,
                              u32 pause_mode, u32 fade_mode, u32 stack_top,
                              u32 stack_top_vt, u32 stack_top_mode,
                              u32 stack_top_text, u32 pad_routing,
                              u32 pad_select, u32 blank_pad_status) {
    __atomic_store_n(&g_native_world, world, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_state, state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_stack_count, count, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_ui_resume_state, resume, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_world_load_request, load_request, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_movie_queued, movie, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_active_ui, visible_ui, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_text_ui, text_ui, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_status, pad_status, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_mode, pad_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_flags, pad_flags, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_open, pause_open, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_page, pause_page, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pause_mode, pause_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_fade_mode, fade_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top, stack_top, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_vt, stack_top_vt, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_mode, stack_top_mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_stack_top_text, stack_top_text, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_routing, pad_routing, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_pad_select, pad_select, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_blank_pad_status, blank_pad_status, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_gate(u32 card_state, u32 card_target,
                              u32 card_display, u32 card_progress,
                              u32 memory_gate, u32 element_state,
                              u32 card_thread, u32 card_job,
                              u32 card_result, u32 card_semaphore) {
    __atomic_store_n(&g_native_card_state, card_state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_target, card_target, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_display, card_display, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_progress, card_progress, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_memory_gate, memory_gate, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_element_state, element_state, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_thread, card_thread, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_job, card_job, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_result, card_result, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_semaphore, card_semaphore, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_timing(float elapsed, float state_time,
                                float display_time) {
    union FloatBits { float f; u32 u; };
    const FloatBits elapsed_bits{elapsed};
    const FloatBits state_bits{state_time};
    const FloatBits display_bits{display_time};
    __atomic_store_n(&g_native_elapsed_bits, elapsed_bits.u, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_state_time_bits, state_bits.u, __ATOMIC_RELAXED);
    __atomic_store_n(&g_native_card_display_time_bits, display_bits.u, __ATOMIC_RELAXED);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_memory_update(u32 stage, u32 state, u32 target,
                                u32 display, float elapsed,
                                float state_time, u32 pause_open,
                                int pause_page) {
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (!g_startup_trace_enabled) return;
    static u32 last_state = 0xffffffffu;
    static u32 last_target = 0xffffffffu;
    static u32 last_display = 0xffffffffu;
    static u32 last_page = 0x7fffffffu;
    static u32 last_open = 0xffffffffu;
    static bool threshold_reported = false;
    static u32 calls = 0;
    ++calls;
    const bool state_changed = state != last_state || target != last_target ||
                               display != last_display;
    const bool page_changed = pause_page != last_page || pause_open != last_open;
    const bool threshold = elapsed - state_time >= 0.3f;
    if ((stage == 0 && (state_changed || (threshold && !threshold_reported) ||
                        (calls % 120u) == 1)) ||
        (stage == 1 && page_changed)) {
        std::fprintf(stderr,
                     "[native memory update] stage=%u state=%d target=%d display=%u elapsed=%.3f state_time=%.3f threshold=%u pause_open=%u page=%d\n",
                     stage, (int)state, (int)target, display,
                     (double)elapsed, (double)state_time, threshold ? 1u : 0u,
                     pause_open, pause_page);
    }
    if (state_changed) {
        last_state = state;
        last_target = target;
        last_display = display;
    }
    if (page_changed) {
        last_page = (u32)pause_page;
        last_open = pause_open;
    }
    if (stage == 0 && threshold) threshold_reported = true;
#else
    (void)stage;
    (void)state;
    (void)target;
    (void)display;
    (void)elapsed;
    (void)state_time;
    (void)pause_open;
    (void)pause_page;
#endif
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_display_setup(u32 width, u32 height, u32 context,
                                u64 scissor1, u64 scissor2) {
#if defined(OPENRAC_SLY1_WITH_SDL)
    if (std::getenv("OPENRAC_TRACE_DIRECT") != nullptr)
        std::fprintf(stderr,
                     "[native display setup] width=%u height=%u context=%#x scissor1=%#llx scissor2=%#llx\n",
                     width, height, context,
                     static_cast<unsigned long long>(scissor1),
                     static_cast<unsigned long long>(scissor2));
#else
    (void)width; (void)height; (void)context; (void)scissor1; (void)scissor2;
#endif
}

#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_startup_callback(unsigned int index) {
    sly1_native_startup_callback_index = index + 1;
#if defined(OPENRAC_SLY1_WITH_SDL)
    const char* trace = std::getenv("OPENRAC_STARTUP_TRACE");
    if (trace != nullptr && trace[0] != '0')
        std::fprintf(stderr, "[native startup] callback=%u iop_stage=%u\n",
                     index + 1, sly1_native_iop_boot_stage);
#endif
}

extern "C" int sly1_native_host32_iop_boot(void) {
    if (g_disc_sector_count == 0) return 0;
    D_002613F0 = 0x14u;
    return 1;
}

extern "C" int sly1_native_host32_iop_initialize(int mode) {
    (void)mode;
    return g_disc_sector_count != 0;
}

extern "C" int sly1_native_host32_iop_services_start(int mode) {
    (void)mode;
    if (g_disc_sector_count == 0) return 0;
    *(volatile u32*)0x0027AE80u = 1u;
    return 1;
}

int main(int argc, char** argv) {
    sly1_native_host32_crt_initialized = 1;
    return sly1_native_host32_main(argc, argv);
}
#endif

#if !defined(OPENRAC_SLY1_WITH_SDL)
// Host32 diagnostic shims: SDL builds bind these services to the native
// runtime. The freestanding launcher has no input or renderer, but still
// needs to execute startup far enough to inspect resource loading.
extern "C" void sly1_native_host32_wait_frame(void) {}
extern "C" int sly1_native_host32_iop_boot(void) {
    if (g_disc_sector_count == 0) return 0;
    D_002613F0 = 0x14u;
    sly1_native_iop_boot_stage = 1;
    return 1;
}

extern "C" int sly1_native_host32_iop_initialize(int mode) {
    (void)mode;
    sly1_native_iop_boot_stage = 2;
    return g_disc_sector_count != 0;
}

extern "C" int sly1_native_host32_iop_services_start(int mode) {
    (void)mode;
    if (g_disc_sector_count == 0) return 0;
    *(volatile u32*)0x0027AE80u = 1u;
    sly1_native_iop_boot_stage = 3;
    return 1;
}

extern "C" void sly1_native_startup_callback(unsigned int index) {
    sly1_native_startup_callback_index = index + 1;
}

extern "C" unsigned long long openrac_guest_kernel_syscall(
    unsigned int service, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long,
    unsigned long long, unsigned long long, unsigned long long) {
    // Host32 runs startup on one thread; the shared runtime's GS mask calls
    // therefore need no locking service here.
    (void)service;
    return 0;
}

extern "C" void openrac_native_renderer_submit_gif_stream(u32) {}
extern "C" int openrac_native_pad_read(int, unsigned char*, unsigned int) {
    return 0;
}
#endif
