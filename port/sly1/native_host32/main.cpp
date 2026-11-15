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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <ucontext.h>
alignas(16) thread_local u8 g_fault_trace_alt_stack[64 * 1024];
static std::mutex g_native_sly_mesh_mutex;
static std::unordered_map<u32, std::vector<float>> g_native_sly_meshes;
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
extern "C" volatile u32 sly1_native_postload_entry = 0;
extern "C" volatile u32 sly1_native_postload_bind = 0;
extern "C" volatile u32 sly1_native_bind_self = 0;
extern "C" volatile u32 sly1_native_bind_context = 0;
extern "C" volatile u32 sly1_native_bind_world = 0;
extern "C" volatile u32 sly1_native_bind_key = 0;
extern "C" volatile u32 sly1_native_find_world = 0;
extern "C" volatile u32 sly1_native_find_id = 0;
extern "C" volatile u32 sly1_native_find_context = 0;
extern "C" volatile u32 sly1_native_find_esp_before = 0;
extern "C" volatile u32 sly1_native_find_esp_after = 0;
extern "C" volatile u32 sly1_native_matrix_source = 0;
extern "C" volatile u32 sly1_native_matrix_output = 0;
extern "C" volatile u32 sly1_native_bind_ebp_entry = 0;
extern "C" volatile u32 sly1_native_bind_ebp_after_find = 0;
extern "C" volatile u32 sly1_native_motion_stage = 0;
extern "C" volatile u32 sly1_native_motion_self = 0;
extern "C" volatile u32 sly1_native_motion_hook = 0;
extern "C" volatile u32 sly1_native_motion_velocity = 0;
extern "C" volatile u32 sly1_native_motion_accel = 0;
extern "C" volatile u32 sly1_native_motion_tris = 0;
extern "C" volatile u32 sly1_native_motion_entry_hook = 0;
extern "C" volatile u32 sly1_native_motion_entry_velocity = 0;
extern "C" volatile u32 sly1_native_motion_entry_accel = 0;
extern "C" volatile u32 sly1_native_motion_entry_tris = 0;
extern "C" volatile u32 sly1_native_motion_entry_other = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_id = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_id = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_vertices = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_vertices = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_desc = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_desc = 0;
extern "C" volatile u32 sly1_native_motion_entry_all = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_faces = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_face_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_vert_ptr = 0;
extern "C" volatile u32 sly1_native_motion_entry_self_copy_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_faces = 0;
extern "C" volatile u32 sly1_native_motion_entry_other_face_count = 0;
extern "C" volatile u32 sly1_native_motion_entry_return = 0;
extern "C" volatile u32 sly1_native_motion_copy_calls = 0;
extern "C" volatile u32 sly1_native_motion_copy_self = 0;
extern "C" volatile u32 sly1_native_motion_copy_vertices = 0;
extern "C" volatile u32 sly1_native_motion_helper_src = 0;
extern "C" volatile u32 sly1_native_motion_helper_extra = 0;
extern "C" volatile u32 sly1_native_motion_helper_src_id = 0;
extern "C" volatile u32 sly1_native_motion_helper_src_vertices = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_return = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_arg1 = 0;
extern "C" volatile u32 sly1_native_motion_copy_entry_arg2 = 0;
extern "C" volatile u32 sly1_native_arrival_world = 0;
extern "C" volatile u32 sly1_native_arrival_id = 0;
extern "C" volatile u32 sly1_native_arrival_parent = 0;
extern "C" volatile u32 sly1_native_arrival_result = 0;
extern "C" volatile u32 sly1_native_arrival_place_self = 0;
extern "C" volatile u32 sly1_native_arrival_rotation_self = 0;
extern "C" volatile u32 sly1_native_contact_object = 0;
extern "C" volatile u32 sly1_native_contact_descriptor = 0;
extern "C" volatile u32 sly1_native_contact_callback = 0;
extern "C" volatile u32 sly1_native_contact_end = 0;
extern "C" volatile u32 sly1_native_camera_target = 0;
extern "C" volatile u32 sly1_native_camera_target_descriptor = 0;
extern "C" volatile u32 sly1_native_camera_target_callback = 0;
extern "C" volatile u32 sly1_native_oscillator_debug[24] = {};
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

void write_stack_value(u32 offset, u32 value) {
    char label[] = "native host32: fault stack +0000 ";
    const char hex[] = "0123456789abcdef";
    label[28] = hex[(offset >> 12) & 15];
    label[29] = hex[(offset >> 8) & 15];
    label[30] = hex[(offset >> 4) & 15];
    label[31] = hex[offset & 15];
    write_hex(label, (int)value);
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
    const u32 fault_eip = (u32)state->uc_mcontext.gregs[REG_EIP];
    const u32 fault_ebp = (u32)state->uc_mcontext.gregs[REG_EBP];
    u32 handler_sp;
    __asm__ __volatile__("movl %%esp, %0" : "=r"(handler_sp));
    write_hex("native host32: fault signal stack flags ", state->uc_stack.ss_flags);
    write_hex("native host32: fault handler stack pointer ", (int)handler_sp);
    if (fault_eip >= 0x080d0000u && fault_eip < 0x080e0000u &&
        fault_ebp >= 0x10000000u && fault_ebp < 0xf0000000u) {
        const volatile u32* frame = (const volatile u32*)fault_ebp;
        write_hex("native host32: oscillator local self ", (int)frame[-0x124 / 4]);
        write_hex("native host32: oscillator local part ", (int)frame[-0x148 / 4]);
        write_hex("native host32: oscillator local obj ", (int)frame[-0xb8 / 4]);
        write_hex("native host32: oscillator local mesh ", (int)frame[-0xb4 / 4]);
        write_hex("native host32: oscillator local n ", (int)frame[-0x110 / 4]);
        write_hex("native host32: oscillator local weights ", (int)frame[-0x10c / 4]);
        write_hex("native host32: oscillator local verts ", (int)frame[-0x108 / 4]);
        write_hex("native host32: oscillator local data verts ", (int)frame[-0x134 / 4]);
        write_hex("native host32: oscillator local source ", (int)frame[-0x130 / 4]);
    }
    for (u32 i = 0; i < 24; ++i)
        write_hex("native host32: oscillator probe ",
                  (int)sly1_native_oscillator_debug[i]);
        write_hex("native host32: arrival world ", (int)sly1_native_arrival_world);
        write_hex("native host32: arrival id ", (int)sly1_native_arrival_id);
        write_hex("native host32: arrival parent ", (int)sly1_native_arrival_parent);
        write_hex("native host32: arrival result ", (int)sly1_native_arrival_result);
        write_hex("native host32: arrival place self ", (int)sly1_native_arrival_place_self);
        write_hex("native host32: arrival rotation self ", (int)sly1_native_arrival_rotation_self);
        write_hex("native host32: contact object ", (int)sly1_native_contact_object);
        write_hex("native host32: contact descriptor ", (int)sly1_native_contact_descriptor);
        write_hex("native host32: contact callback ", (int)sly1_native_contact_callback);
        write_hex("native host32: contact end ", (int)sly1_native_contact_end);
        write_hex("native host32: camera target ", (int)sly1_native_camera_target);
        write_hex("native host32: camera target descriptor ", (int)sly1_native_camera_target_descriptor);
        write_hex("native host32: camera target callback ", (int)sly1_native_camera_target_callback);
    write_hex("native host32: last PostLoad entry ", (int)sly1_native_postload_entry);
    write_hex("native host32: last PostLoad bind ", (int)sly1_native_postload_bind);
    write_hex("native host32: Bind self ", (int)sly1_native_bind_self);
    write_hex("native host32: Bind context ", (int)sly1_native_bind_context);
    write_hex("native host32: Bind world ", (int)sly1_native_bind_world);
    write_hex("native host32: Bind key ", (int)sly1_native_bind_key);
    write_hex("native host32: Find world ", (int)sly1_native_find_world);
    write_hex("native host32: Find id ", (int)sly1_native_find_id);
    write_hex("native host32: Find context ", (int)sly1_native_find_context);
    write_hex("native host32: Find ESP before ", (int)sly1_native_find_esp_before);
    write_hex("native host32: Find ESP after ", (int)sly1_native_find_esp_after);
    write_hex("native host32: matrix source ", (int)sly1_native_matrix_source);
    write_hex("native host32: matrix output ", (int)sly1_native_matrix_output);
    write_hex("native host32: Bind EBP entry ", (int)sly1_native_bind_ebp_entry);
    write_hex("native host32: Bind EBP after Find ", (int)sly1_native_bind_ebp_after_find);
    write_hex("native host32: motion init stage ", (int)sly1_native_motion_stage);
    write_hex("native host32: motion init self ", (int)sly1_native_motion_self);
    write_hex("native host32: motion init hook ", (int)sly1_native_motion_hook);
    write_hex("native host32: motion init velocity ", (int)sly1_native_motion_velocity);
    write_hex("native host32: motion init accel ", (int)sly1_native_motion_accel);
    write_hex("native host32: motion init tris ", (int)sly1_native_motion_tris);
    write_hex("native host32: motion entry hook ", (int)sly1_native_motion_entry_hook);
    write_hex("native host32: motion entry velocity ", (int)sly1_native_motion_entry_velocity);
    write_hex("native host32: motion entry accel ", (int)sly1_native_motion_entry_accel);
    write_hex("native host32: motion entry tris ", (int)sly1_native_motion_entry_tris);
    write_hex("native host32: motion entry other ", (int)sly1_native_motion_entry_other);
    write_hex("native host32: motion entry self id ", (int)sly1_native_motion_entry_self_id);
    write_hex("native host32: motion entry other id ", (int)sly1_native_motion_entry_other_id);
    write_hex("native host32: motion entry self vertices ", (int)sly1_native_motion_entry_self_vertices);
    write_hex("native host32: motion entry other vertices ", (int)sly1_native_motion_entry_other_vertices);
    write_hex("native host32: motion entry self desc ", (int)sly1_native_motion_entry_self_desc);
    write_hex("native host32: motion entry other desc ", (int)sly1_native_motion_entry_other_desc);
    write_hex("native host32: motion entry all ", (int)sly1_native_motion_entry_all);
    write_hex("native host32: motion entry self faces ", (int)sly1_native_motion_entry_self_faces);
    write_hex("native host32: motion entry self face count ", (int)sly1_native_motion_entry_self_face_count);
    write_hex("native host32: motion entry self verts ptr ", (int)sly1_native_motion_entry_self_vert_ptr);
    write_hex("native host32: motion entry self meshCopy count ", (int)sly1_native_motion_entry_self_copy_count);
    write_hex("native host32: motion entry other faces ", (int)sly1_native_motion_entry_other_faces);
    write_hex("native host32: motion entry other face count ", (int)sly1_native_motion_entry_other_face_count);
    write_hex("native host32: motion entry return ", (int)sly1_native_motion_entry_return);
    write_hex("native host32: motion copy calls ", (int)sly1_native_motion_copy_calls);
    write_hex("native host32: motion copy self ", (int)sly1_native_motion_copy_self);
    write_hex("native host32: motion copy vertices ", (int)sly1_native_motion_copy_vertices);
    write_hex("native host32: mesh helper source ", (int)sly1_native_motion_helper_src);
    write_hex("native host32: mesh helper extra ", (int)sly1_native_motion_helper_extra);
    write_hex("native host32: mesh helper source id ", (int)sly1_native_motion_helper_src_id);
    write_hex("native host32: mesh helper source vertices ", (int)sly1_native_motion_helper_src_vertices);
    write_hex("native host32: mesh copy entry return ", (int)sly1_native_motion_copy_entry_return);
    write_hex("native host32: mesh copy entry arg1 ", (int)sly1_native_motion_copy_entry_arg1);
    write_hex("native host32: mesh copy entry arg2 ", (int)sly1_native_motion_copy_entry_arg2);
    const u32 fault_esp = (u32)state->uc_mcontext.gregs[REG_ESP];
    if (fault_esp >= 0xC0000000u && fault_esp < 0xF0000000u) {
        const u32* stack = reinterpret_cast<const u32*>(fault_esp);
        write_hex("native host32: fault stack +00 ", stack[0]);
        write_hex("native host32: fault stack +04 ", stack[1]);
        write_hex("native host32: fault stack +08 ", stack[2]);
        write_hex("native host32: fault stack +0c ", stack[3]);
        write_hex("native host32: fault stack +10 ", stack[4]);
        write_hex("native host32: fault stack +14 ", stack[5]);
        write_hex("native host32: fault stack +1c ", stack[7]);
        write_hex("native host32: fault stack +20 ", stack[8]);
        write_hex("native host32: fault stack +24 ", stack[9]);
        write_hex("native host32: fault stack +28 ", stack[10]);
        write_hex("native host32: fault stack +2c ", stack[11]);
        write_hex("native host32: fault stack +30 ", stack[12]);
        write_hex("native host32: fault stack +34 ", stack[13]);
        write_hex("native host32: fault stack +3c ", stack[0x3c / 4]);
        write_hex("native host32: fault stack +40 ", stack[0x40 / 4]);
        write_hex("native host32: fault stack +44 ", stack[0x44 / 4]);
        write_hex("native host32: fault stack +48 ", stack[0x48 / 4]);
        write_hex("native host32: fault stack +4c ", stack[0x4c / 4]);
        write_hex("native host32: fault stack +50 ", stack[0x50 / 4]);
        write_hex("native host32: fault stack +54 ", stack[0x54 / 4]);
        write_hex("native host32: fault stack +7c ", stack[0x7c / 4]);
        write_hex("native host32: fault stack +80 ", stack[0x80 / 4]);
        write_hex("native host32: fault stack +84 ", stack[0x84 / 4]);
        write_hex("native host32: fault stack +88 ", stack[0x88 / 4]);
        for (u32 offset = 0x70; offset <= 0xc0; offset += 4) {
            write_stack_value(offset, stack[offset / 4]);
        }
        for (u32 offset = 0x1d0; offset <= 0x230; offset += 4) {
            write_stack_value(offset, stack[offset / 4]);
        }
        const u32 suspected_collider = stack[0x0c / 4];
        const u32 suspected_other = (u32)state->uc_mcontext.gregs[REG_EBX];
        if (suspected_collider >= 0x01000000u && suspected_collider < 0x08000000u) {
            const u32* collider = reinterpret_cast<const u32*>(suspected_collider);
            write_hex("native host32: suspected collider ", (int)suspected_collider);
            write_hex("native host32: collider verts count ", (int)collider[0x384 / 4]);
            write_hex("native host32: collider verts ", (int)collider[0x388 / 4]);
            write_hex("native host32: collider faces count ", (int)collider[0x38c / 4]);
            write_hex("native host32: collider faces ", (int)collider[0x390 / 4]);
        }
        if (suspected_other >= 0x01000000u && suspected_other < 0x08000000u) {
            const u32* other = reinterpret_cast<const u32*>(suspected_other);
            write_hex("native host32: suspected other ", (int)suspected_other);
            write_hex("native host32: other verts count ", (int)other[0x384 / 4]);
            write_hex("native host32: other verts ", (int)other[0x388 / 4]);
            write_hex("native host32: other faces count ", (int)other[0x38c / 4]);
            write_hex("native host32: other faces ", (int)other[0x390 / 4]);
        }
        const u32 suspected_edi = (u32)state->uc_mcontext.gregs[REG_EDI];
        if (suspected_edi >= 0x01000000u && suspected_edi < 0x08000000u) {
            const u32* object = reinterpret_cast<const u32*>(suspected_edi);
            const u32 hook = object[0x3c8 / 4];
            write_hex("native host32: edi object ", (int)suspected_edi);
            write_hex("native host32: object hook ", (int)hook);
            if (hook >= 0x01000000u && hook < 0x08000000u) {
                const u32* motion = reinterpret_cast<const u32*>(hook);
                write_hex("native host32: motion velocity ", (int)motion[0]);
                write_hex("native host32: motion accel ", (int)motion[1]);
                write_hex("native host32: motion tris ", (int)motion[2]);
            }
            write_hex("native host32: mesh copy verts count ", (int)object[0x3a8 / 4]);
            write_hex("native host32: mesh copy verts ", (int)object[0x3ac / 4]);
            write_hex("native host32: mesh copy faces count ", (int)object[0x3b0 / 4]);
            write_hex("native host32: mesh copy faces ", (int)object[0x3b4 / 4]);
            write_hex("native host32: mesh copy edges ", (int)object[0x3bc / 4]);
        }
        const u32 context_pointer = stack[0x4c / 4];
        if (context_pointer >= fault_esp && context_pointer < fault_esp + 0x4000u) {
            const u32* object = reinterpret_cast<const u32*>(context_pointer);
            write_hex("native host32: stack object +00 ", object[0x00 / 4]);
            write_hex("native host32: stack object +04 ", object[0x04 / 4]);
            write_hex("native host32: stack object +08 ", object[0x08 / 4]);
            write_hex("native host32: stack object +14 ", object[0x14 / 4]);
            write_hex("native host32: stack object +18 ", object[0x18 / 4]);
            write_hex("native host32: stack object +34 ", object[0x34 / 4]);
        }
        const u32 linked_object = stack[0x1e8 / 4];
        if (linked_object >= 0x01000000u && linked_object < 0x10000000u) {
            const u32* object = reinterpret_cast<const u32*>(linked_object);
            const u32 descriptor = object[0];
            write_hex("native host32: linked object ", (int)linked_object);
            write_hex("native host32: linked descriptor ", (int)descriptor);
            write_hex("native host32: linked object id ", (int)object[2]);
            write_hex("native host32: linked object world ", (int)object[0x14 / 4]);
            if (descriptor >= 0x08040000u && descriptor < 0x09000000u) {
                write_hex("native host32: linked m0f ",
                          (int)*(const u32*)(descriptor + 0x4c));
            }
        }
        for (u32 offset = 0x8c; offset <= 0x800; offset += 4) {
            const u32 value = stack[offset / 4];
            if (value >= 0x08040000u && value < 0x09000000u) {
                write_stack_value(offset, value);
            }
        }
    }
    syscall1(252, 128 + signal_number);
}

void install_fault_trace() {
    stack_t alternate{};
    alternate.ss_sp = g_fault_trace_alt_stack;
    alternate.ss_size = sizeof(g_fault_trace_alt_stack);
    const int altstack_result = sigaltstack(&alternate, nullptr);
    write_hex("native host32: install alternate stack result ", altstack_result);
    write_hex("native host32: install alternate stack base ",
              (int)alternate.ss_sp);
    struct sigaction action{};
    action.sa_sigaction = native_fault_trace;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGFPE, &action, nullptr);
    sigaction(SIGABRT, &action, nullptr);
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
volatile sig_atomic_t g_native_pc_sample_pc = 0;
volatile sig_atomic_t g_native_pc_sample_sp = 0;
volatile sig_atomic_t g_native_pc_sample_bp = 0;
volatile sig_atomic_t g_native_pc_sample_sequence = 0;
volatile sig_atomic_t g_native_pc_sample_stack[8] = {};
volatile u32 g_native_semaphore_trace_active = 0;
volatile u32 g_semaphore_trace_event_count = 0;
volatile u32 g_native_trace_first_world = 0;
volatile u32 g_native_trace_target_world = 0;

extern "C" void sly1_native_trace_camera(u32 stage, u32 camera_address,
                                           u32 world_address) {
    static volatile u32 printed_mask = 0;
    if (!g_startup_trace_enabled ||
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_TRANSFORM") == nullptr ||
        camera_address == 0)
        return;
    if (stage < 32 && (__atomic_fetch_or(&printed_mask, 1u << stage,
                                         __ATOMIC_RELAXED) & (1u << stage)))
        return;
    const volatile float* position = reinterpret_cast<const volatile float*>(camera_address + 0x40u);
    const volatile float* rotation = reinterpret_cast<const volatile float*>(camera_address + 0x80u);
    const volatile float* view_projection = reinterpret_cast<const volatile float*>(camera_address + 0x100u);
    std::fprintf(stderr,
                 "[native camera stage] stage=%u world=%#010x camera=%#010x pos=%g,%g,%g rot=%g,%g,%g;%g,%g,%g;%g,%g,%g\n",
                 stage, world_address, camera_address,
                 position[0], position[1], position[2],
                 rotation[0], rotation[1], rotation[2],
                 rotation[4], rotation[5], rotation[6],
                 rotation[8], rotation[9], rotation[10]);
    std::fprintf(stderr,
                 "[native camera matrix] stage=%u rows=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g\n",
                 stage, view_projection[0], view_projection[1], view_projection[2], view_projection[3],
                 view_projection[4], view_projection[5], view_projection[6], view_projection[7],
                 view_projection[8], view_projection[9], view_projection[10], view_projection[11],
                 view_projection[12], view_projection[13], view_projection[14], view_projection[15]);
}

#if defined(OPENRAC_SLY1_WITH_SDL)
extern "C" void sly1_native_trace_note_world(u32 world) {
    if (!g_startup_trace_enabled || world == 0) return;
    u32 first = __atomic_load_n(&g_native_trace_first_world, __ATOMIC_RELAXED);
    if (first == 0) {
        __atomic_compare_exchange_n(&g_native_trace_first_world, &first, world, false,
                                   __ATOMIC_RELAXED, __ATOMIC_RELAXED);
        first = __atomic_load_n(&g_native_trace_first_world, __ATOMIC_RELAXED);
    }
    if (world != first)
        __atomic_store_n(&g_native_trace_target_world, world, __ATOMIC_RELAXED);
}

extern "C" void sly1_native_trace_model_part(u32 request_address, u32 world,
                                               u32 camera_address) {
    if (!g_startup_trace_enabled || request_address == 0 || world == 0 ||
        world != __atomic_load_n(&g_native_trace_target_world, __ATOMIC_RELAXED)) return;
    static u32 part_count = 0;
    const u32 trace_index = __atomic_fetch_add(&part_count, 1u, __ATOMIC_RELAXED);
    if (trace_index >= 32u) return;

    const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
    const u32 part_address = request[0x78 / 4];
    const u32 skin_address = request[0x74 / 4];
    const u32 owner_address = request[0x70 / 4];
    if (part_address == 0) return;
    const volatile u32* part = reinterpret_cast<const volatile u32*>(part_address);
    const u32 mesh_count = part[0x58 / 4];
    const u32 mesh_address = part[0x5C / 4];
    const u32 strip_count = part[0x60 / 4];
    u32 owner_class = 0xFFFFFFFFu;
    u32 owner_flags = 0;
    if (owner_address != 0) {
        const u32 class_address = *reinterpret_cast<const volatile u32*>(owner_address);
        if (class_address != 0) {
            const volatile u32* class_descriptor =
                reinterpret_cast<const volatile u32*>(class_address);
            owner_class = class_descriptor[1];
            owner_flags = class_descriptor[2];
        }
    }
    static u32 traced_lit_world_part = 0;
    bool finite_model = true;
    const volatile float* model = reinterpret_cast<const volatile float*>(request_address + 0x10u);
    for (u32 i = 0; i < 16u; ++i)
        if (!std::isfinite(model[i])) finite_model = false;
    if (owner_class == 2u && finite_model &&
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_TRANSFORM") != nullptr &&
        camera_address != 0 &&
        __atomic_load_n(&traced_lit_world_part, __ATOMIC_RELAXED) == 0) {
        __atomic_store_n(&traced_lit_world_part, part_address, __ATOMIC_RELAXED);
        const volatile float* camera_position = reinterpret_cast<const volatile float*>(camera_address + 0x40u);
        const volatile float* camera_rotation = reinterpret_cast<const volatile float*>(camera_address + 0x80u);
        const volatile float* camera_matrix = reinterpret_cast<const volatile float*>(camera_address + 0x100u);
        const volatile float* camera_scale = reinterpret_cast<const volatile float*>(camera_address + 0x1F0u);
        std::fprintf(stderr, "[native model transform] index=%u request=%#010x camera=%#010x\n",
                     trace_index, request_address, camera_address);
        for (u32 row = 0; row < 4u; ++row) {
            std::fprintf(stderr,
                         "[native model transform] model%u=%g,%g,%g,%g viewProjection%u=%g,%g,%g,%g\n",
                         row, model[row * 4u], model[row * 4u + 1u],
                         model[row * 4u + 2u], model[row * 4u + 3u],
                         row, camera_matrix[row * 4u], camera_matrix[row * 4u + 1u],
                         camera_matrix[row * 4u + 2u], camera_matrix[row * 4u + 3u]);
        }
        std::fprintf(stderr,
                     "[native model transform] position=%g,%g,%g rotation=%g,%g,%g,%g;%g,%g,%g,%g;%g,%g,%g,%g scale=%g,%g\n",
                     camera_position[0], camera_position[1], camera_position[2],
                     camera_rotation[0], camera_rotation[1], camera_rotation[2], camera_rotation[3],
                     camera_rotation[4], camera_rotation[5], camera_rotation[6], camera_rotation[7],
                     camera_rotation[8], camera_rotation[9], camera_rotation[10], camera_rotation[11],
                     camera_scale[0], camera_scale[1]);
    }
    std::fprintf(stderr,
                 "[native model draw] index=%u world=%#010x request=%#010x owner=%#010x class=%u class_flags=%#x part=%#010x skin=%#010x meshes=%u strips=%u flags=%#x lit=%u skinned=%u matrix=%#x,%#x,%#x,%#x\n",
                 trace_index, world, request_address, owner_address,
                 owner_class, owner_flags, part_address, skin_address,
                 mesh_count, strip_count, part[0x2C / 4], part[0x18 / 4],
                 part[0x28 / 4], request[0x10 / 4], request[0x20 / 4],
                 request[0x30 / 4], request[0x40 / 4]);
    if (skin_address != 0) {
        const volatile u32* skin = reinterpret_cast<const volatile u32*>(skin_address);
        std::fprintf(stderr,
                     "[native model skin] index=%u flags=%#x count=%u words=%#010x bones=%#010x rgba=%08x\n",
                     trace_index, skin[0x38 / 4], skin[0x40 / 4],
                     skin[0x44 / 4], skin[0x04 / 4], skin[0x3C / 4]);
    }
    const u32 bounded_mesh_count = mesh_count < 4u ? mesh_count : 4u;
    for (u32 i = 0; i < bounded_mesh_count && mesh_address != 0; ++i) {
        const volatile u32* mesh = reinterpret_cast<const volatile u32*>(
            mesh_address + i * 0x50u);
        const u32 kind_address = mesh[0x18 / 4];
        const u32 kind = kind_address != 0
                             ? *reinterpret_cast<const volatile u8*>(kind_address)
                             : 0xFFFFFFFFu;
        std::fprintf(stderr,
                     "[native model mesh] index=%u mesh=%u kind=%u source=%#010x bones=%u qwc=%u,%u,%u data=%#010x,%#010x,%#010x\n",
                     trace_index, i, kind, kind_address, mesh[0x24 / 4], mesh[0x38 / 4],
                     mesh[0x3C / 4], mesh[0x40 / 4], mesh[0x44 / 4],
                     mesh[0x48 / 4], mesh[0x4C / 4]);
        if (kind_address != 0 && std::getenv("OPENRAC_TRACE_NATIVE_MODEL_STRIPS") != nullptr) {
            const volatile u32* strip_mesh = reinterpret_cast<const volatile u32*>(kind_address);
            const u32 strip_count = strip_mesh[0x18 / 4];
            const u32 strip_address = strip_mesh[0x1C / 4];
            const u32 frame_count = strip_mesh[0x20 / 4];
            std::fprintf(stderr,
                         "[native model strip source] index=%u mesh=%u mode=%u color=%08x strips=%u sources=%#010x frames=%u\n",
                         trace_index, i,
                         *reinterpret_cast<const volatile u8*>(kind_address + 1u),
                         strip_mesh[1], strip_count, strip_address, frame_count);
            const u32 bounded_strip_count = strip_count < 8u ? strip_count : 8u;
            for (u32 strip_index = 0; strip_index < bounded_strip_count && strip_address != 0;
                 ++strip_index) {
                const volatile u32* source = reinterpret_cast<const volatile u32*>(
                    strip_address + strip_index * 8u);
                std::fprintf(stderr,
                             "[native model frame source] index=%u mesh=%u strip=%u stride=%u base=%#010x\n",
                             trace_index, i, strip_index, source[0], source[1]);
            }
        }
        if (owner_class == 2u && finite_model && kind == 0u &&
            std::getenv("OPENRAC_DUMP_NATIVE_WORLD_PART") != nullptr) {
            static u32 dumped_world_part = 0;
            if (__atomic_exchange_n(&dumped_world_part, 1u, __ATOMIC_RELAXED) == 0) {
                const u32 packet_address[3] = {mesh[0x44 / 4], mesh[0x48 / 4], mesh[0x4C / 4]};
                const u32 packet_qwc[3] = {mesh[0x38 / 4], mesh[0x3C / 4], mesh[0x40 / 4]};
                for (u32 packet = 0; packet < 3u; ++packet) {
                    if (packet_address[packet] == 0 || packet_qwc[packet] == 0 ||
                        packet_qwc[packet] > 4096u) continue;
                    char path[96];
                    std::snprintf(path, sizeof(path), "/tmp/sly-world-part-%u.bin", packet);
                    if (FILE* file = std::fopen(path, "wb")) {
                        const std::size_t bytes = static_cast<std::size_t>(packet_qwc[packet]) * 16u;
                        const std::size_t written = std::fwrite(
                            reinterpret_cast<const void*>(packet_address[packet]), 1, bytes, file);
                        std::fclose(file);
                        std::fprintf(stderr,
                                     "[native world part dump] part=%#010x mesh=%u packet=%u address=%#010x qwc=%u written=%zu path=%s\n",
                                     part_address, i, packet, packet_address[packet],
                                     packet_qwc[packet], written, path);
                    }
                }
            }
        }
        if (kind == 6u &&
            std::getenv("OPENRAC_DUMP_NATIVE_MODEL_PACKET") != nullptr) {
            static u32 dumped = 0;
            if (__atomic_exchange_n(&dumped, 1u, __ATOMIC_RELAXED) == 0) {
                const u32 packet_address = mesh[0x44 / 4];
                const u32 packet_qwc = mesh[0x38 / 4];
                if (packet_address != 0 && packet_qwc > 0 && packet_qwc <= 4096u) {
                    const char* path = "/tmp/sly-kind6-model-packet.bin";
                    if (FILE* file = std::fopen(path, "wb")) {
                        const std::size_t bytes = static_cast<std::size_t>(packet_qwc) * 16u;
                        const std::size_t written = std::fwrite(
                            reinterpret_cast<const void*>(packet_address), 1, bytes, file);
                        std::fclose(file);
                        std::fprintf(stderr,
                                     "[native model packet dump] kind=%u qwc=%u bytes=%zu written=%zu path=%s\n",
                                     kind, packet_qwc, bytes, written, path);
                    }
                }
            }
        }
    }
}

extern "C" void sly1_native_capture_model_mesh(u32 mesh_address, u32 vecs_address,
                                                   u32 tints_address, u32 vertex_count,
                                                   u32 refs_address) {
    if (mesh_address == 0 || vecs_address == 0 ||
        refs_address == 0 || vertex_count < 3 || vertex_count > 4096)
        return;
    const volatile u8* mesh = reinterpret_cast<const volatile u8*>(mesh_address);
    if (mesh[0] != 6u) return;
    const volatile u8* refs = reinterpret_cast<const volatile u8*>(refs_address);
    const volatile float* positions = reinterpret_cast<const volatile float*>(vecs_address);
    const volatile u8* tints = reinterpret_cast<const volatile u8*>(tints_address);
    std::vector<float> vertices;
    vertices.reserve(static_cast<std::size_t>(vertex_count) * 7u);
    for (u32 i = 0; i < vertex_count; ++i) {
        const u8 position_index = refs[i * 4u];
        const u8 tint_index = refs[i * 4u + 3u] & 0x7Fu;
        const u8 flags = refs[i * 4u + 3u];
        vertices.push_back(static_cast<float>(positions[position_index * 3u]));
        vertices.push_back(static_cast<float>(positions[position_index * 3u + 1u]));
        vertices.push_back(static_cast<float>(positions[position_index * 3u + 2u]));
        for (u32 channel = 0; channel < 4u; ++channel) {
            const u8 base = mesh[4u + channel];
            const float color = tint_index == 0x7Fu || tints == nullptr
                ? static_cast<float>((base + 1u) >> 1u) / 255.0f
                : static_cast<float>((base * tints[tint_index * 4u + channel] + 0x100u) / 0x1FEu) / 255.0f;
            vertices.push_back(color);
        }
        // The VU consumes XYZ and supplies its homogeneous coordinate separately.
    }
    std::scoped_lock lock(g_native_sly_mesh_mutex);
    g_native_sly_meshes[mesh_address] = std::move(vertices);
    if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr) {
        static u32 notices = 0;
        if (notices++ < 16u)
            std::fprintf(stderr, "[native sly mesh capture] mesh=%#010x vertices=%u registered=%zu\n",
                         mesh_address, vertex_count, g_native_sly_meshes.size());
    }
}

extern "C" void sly1_native_submit_model_part(u32 request_address, u32 world,
                                                 u32 camera_address) {
    if (request_address == 0 || world == 0 || camera_address == 0) return;
    const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
    const u32 part_address = request[0x78 / 4];
    if (part_address == 0) return;
    const volatile u32* part = reinterpret_cast<const volatile u32*>(part_address);
    u32 mesh_count = part[0x58 / 4];
    if (mesh_count > 128u) mesh_count = 128u;
    const u32 mesh_address = part[0x5C / 4];
    if (mesh_count == 0 || mesh_address == 0) return;
    const float* model = reinterpret_cast<const float*>(request_address + 0x10u);
    const float* view_projection = reinterpret_cast<const float*>(camera_address + 0x100u);
    static u32 notices = 0;
    for (u32 i = 0; i < mesh_count; ++i) {
        const u32 part_mesh = mesh_address + i * 0x50u;
        const u32 source_mesh = *reinterpret_cast<const volatile u32*>(part_mesh + 0x18u);
        if (source_mesh == 0 || *reinterpret_cast<const volatile u8*>(source_mesh) != 6u)
            continue;
        std::vector<float> vertices;
        {
            std::scoped_lock lock(g_native_sly_mesh_mutex);
            const auto found = g_native_sly_meshes.find(source_mesh);
            if (found == g_native_sly_meshes.end()) {
                if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr && notices++ < 32u)
                    std::fprintf(stderr, "[native sly mesh miss] source=%#010x kind=%u registered=%zu request=%#010x\n",
                                 source_mesh, *reinterpret_cast<const volatile u8*>(source_mesh),
                                 g_native_sly_meshes.size(), request_address);
                continue;
            }
            vertices = found->second;
        }
        const u64 instance = (static_cast<u64>(request_address) << 8u) | i;
        if (std::getenv("OPENRAC_TRACE_SLY_WORLD") != nullptr && notices++ < 32u)
            std::fprintf(stderr, "[native sly mesh submit] source=%#010x request=%#010x mesh=%u vertices=%zu\n",
                         source_mesh, request_address, i, vertices.size() / 7u);
        openrac_native_sly_world_mesh_submit(world, instance, vertices.data(),
                                              static_cast<u32>(vertices.size() / 7u),
                                              model, view_projection);
    }
}

extern "C" void sly1_native_trace_model_build(u32 mesh_address, u32 vecs_address,
                                                 u32 uvs_address, u32 vertex_count,
                                                 u32 refs_address, u32 header_address) {
    if (!g_startup_trace_enabled || mesh_address == 0 ||
        std::getenv("OPENRAC_TRACE_NATIVE_MODEL_BUILDS") == nullptr)
        return;
    const volatile u8* mesh = reinterpret_cast<const volatile u8*>(mesh_address);
    const u32 kind = mesh[0];
    if (kind != 6u || vertex_count == 0 || vertex_count > 4096u) return;

    const u32 strip_count = *reinterpret_cast<const volatile u32*>(mesh_address + 0x18u);
    const u32 sources_address = *reinterpret_cast<const volatile u32*>(mesh_address + 0x1Cu);
    const u32 header_count = header_address != 0
        ? *reinterpret_cast<const volatile u32*>(header_address + 0x2Cu) : 0;
    static u32 sequence = 0;
    const u32 sample = __atomic_fetch_add(&sequence, 1u, __ATOMIC_RELAXED);
    if (sample >= 12u) return;

    const volatile u8* refs = reinterpret_cast<const volatile u8*>(refs_address);
    u32 max_position = 0;
    u32 max_uv = 0;
    bool has_uv = false;
    for (u32 i = 0; i < vertex_count; ++i) {
        if (refs[i * 4u] > max_position) max_position = refs[i * 4u];
        if (refs[i * 4u + 2u] != 0xFFu) {
            has_uv = true;
            if (refs[i * 4u + 2u] > max_uv) max_uv = refs[i * 4u + 2u];
        }
    }
    std::fprintf(stderr,
                 "[native model build] sample=%u kind=6 vertices=%u strips=%u strip_header_count=%u mesh=%#010x vecs=%#010x uvs=%#010x refs=%#010x max_position=%u max_uv=%u\n",
                 sample, vertex_count, strip_count, header_count, mesh_address,
                 vecs_address, uvs_address, refs_address, max_position, has_uv ? max_uv : 0u);

    char path[128];
    auto dump = [&](const char* suffix, const void* data, u32 bytes) {
        std::snprintf(path, sizeof(path), "/tmp/sly-kind6-build-%u-%s.bin", sample, suffix);
        FILE* file = std::fopen(path, "wb");
        if (file == nullptr) return;
        const std::size_t written = std::fwrite(data, 1, bytes, file);
        std::fclose(file);
        std::fprintf(stderr, "[native model build dump] sample=%u part=%s bytes=%u written=%zu path=%s\n",
                     sample, suffix, bytes, written, path);
    };
    dump("refs", reinterpret_cast<const void*>(refs_address), vertex_count * 4u);
    dump("positions", reinterpret_cast<const void*>(vecs_address), (max_position + 1u) * 12u);
    if (has_uv && uvs_address != 0)
        dump("uvs", reinterpret_cast<const void*>(uvs_address), (max_uv + 1u) * 8u);

    if (sources_address == 0 || strip_count > 8u) return;
    const volatile u32* sources = reinterpret_cast<const volatile u32*>(sources_address);
    for (u32 i = 0; i < strip_count; ++i) {
        const u32 stride = sources[i * 2u];
        const u32 base = sources[i * 2u + 1u];
        const u64 byte_count = static_cast<u64>(stride) * 16u;
        bool mapped = false;
        if (stride != 0 && stride <= 1024u && byte_count <= 16384u &&
            base != 0 && byte_count <= 0x100000000ull - base) {
            const long page_size = ::sysconf(_SC_PAGESIZE);
            if (page_size > 0) {
                const u32 mask = static_cast<u32>(page_size - 1);
                const u32 page_begin = base & ~mask;
                const u64 page_end = (static_cast<u64>(base) + byte_count + mask) & ~static_cast<u64>(mask);
                const u64 page_count = (page_end - page_begin) / static_cast<u32>(page_size);
                unsigned char resident[8]{};
                mapped = page_count > 0 && page_count <= 8 &&
                    ::mincore(reinterpret_cast<void*>(page_begin),
                              static_cast<std::size_t>(page_count) * page_size,
                              resident) == 0;
            }
        }
        std::fprintf(stderr,
                     "[native model strip] sample=%u strip=%u stride_qwc=%u base=%#010x mapped=%u\n",
                     sample, i, stride, base, mapped ? 1u : 0u);
        if (mapped) {
            dump(("strip-" + std::to_string(i)).c_str(),
                 reinterpret_cast<const void*>(base), static_cast<u32>(byte_count));
        }
    }
}

extern "C" void sly1_native_trace_world_draw_lists(u32 world_address,
                                                      u32 list60_head,
                                                      u32 list78_head,
                                                      u32 list60_offset,
                                                      u32 list78_offset,
                                                      u32 current_world) {
    if (!g_startup_trace_enabled || world_address == 0 ||
        world_address != current_world ||
        world_address != __atomic_load_n(&g_native_trace_target_world, __ATOMIC_RELAXED))
        return;

    static u32 sample_count = 0;
    const u32 index = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (index < 8u || (index & 63u) == 0) {
        std::fprintf(stderr,
                     "[native world draw lists] sample=%u world=%#010x list60=%#010x offset=%u list78=%#010x offset=%u\n",
                     index, world_address, list60_head, list60_offset,
                     list78_head, list78_offset);
    }
    if (index >= 2u) return;
    const u32 heads[2] = {list60_head, list78_head};
    const u32 offsets[2] = {list60_offset, list78_offset};
    const char* names[2] = {"list60", "list78"};
    for (u32 list_index = 0; list_index < 2u; ++list_index) {
        u32 node_address = heads[list_index];
        for (u32 node_index = 0; node_index < 6u && node_address != 0; ++node_index) {
            const volatile u32* node = reinterpret_cast<const volatile u32*>(node_address);
            const u32 callback = *reinterpret_cast<const volatile u32*>(node[0] + 40u * 4u);
            const u32 next = *reinterpret_cast<const volatile u32*>(node_address + offsets[list_index]);
            std::fprintf(stderr,
                         "[native world draw node] list=%s node=%u address=%#010x object=%#010x draw40=%#010x next=%#010x\n",
                         names[list_index], node_index, node_address, node[0], callback, next);
            node_address = next;
        }
    }
}

extern "C" void sly1_native_trace_deferred_requests(u32 world_address,
                                                       u32 queued_count,
                                                       u32 sorted_count,
                                                       u32 sorted_list,
                                                       u32 model_draw_callback) {
    if (!g_startup_trace_enabled || world_address == 0 || sorted_list == 0 ||
        world_address != __atomic_load_n(&g_native_trace_target_world, __ATOMIC_RELAXED))
        return;

    static u32 sample_count = 0;
    const u32 sample = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (!(sample < 4u || (sample & 63u) == 0)) return;
    std::fprintf(stderr,
                 "[native deferred requests] sample=%u world=%#010x queued=%u sorted=%u",
                 sample, world_address, queued_count, sorted_count);
    const volatile u32* entries = reinterpret_cast<const volatile u32*>(sorted_list);
    const u32 limit = sorted_count < 12u ? sorted_count : 12u;
    for (u32 i = 0; i < limit; ++i) {
        const u32 request_address = entries[i];
        if (request_address == 0) continue;
        const volatile u32* request = reinterpret_cast<const volatile u32*>(request_address);
        std::fprintf(stderr, " [%u]=%#010x,arg=%#010x,group=%u",
                     i, request[0], request[0x60 / 4], request[0x08 / 4]);
        if (request[0] == model_draw_callback) {
            const u32 object_address = request[0x60 / 4];
            if (object_address >= 0x01000000u && object_address < 0x02000000u) {
                const volatile u32* object = reinterpret_cast<const volatile u32*>(object_address);
                std::fprintf(stderr,
                             ",model_parts=%u,parts=%#010x,range=%#010x,packet_sizes=%u/%u",
                             object[0x238 / 4], object[0x23C / 4], object[0x2D0 / 4],
                             object[0x2D8 / 4], object[0x2E0 / 4]);
            }
        }
    }
    std::fputc('\n', stderr);
}

extern "C" void sly1_native_trace_draw_object(u32 world_address,
                                                u32 object_address,
                                                u32 part_count,
                                                u32 parts_address) {
    if (!g_startup_trace_enabled || world_address == 0 || object_address == 0 ||
        world_address != __atomic_load_n(&g_native_trace_target_world, __ATOMIC_RELAXED))
        return;
    static u32 sample_count = 0;
    const u32 sample = __atomic_fetch_add(&sample_count, 1u, __ATOMIC_RELAXED);
    if (sample >= 16u) return;
    std::fprintf(stderr,
                 "[native draw object] sample=%u world=%#010x object=%#010x parts=%u array=%#010x",
                 sample, world_address, object_address, part_count, parts_address);
    if (parts_address != 0 && part_count > 0) {
        const volatile u32* part = reinterpret_cast<const volatile u32*>(parts_address);
        std::fprintf(stderr, " first_pos=%#x,%#x,%#x,%#x radius=%#x kind=%u matrix=%#010x",
                     part[0], part[1], part[2], part[3], part[4], part[5], part[0x68 / 4]);
    }
    std::fputc('\n', stderr);
}
#else
extern "C" void sly1_native_trace_model_part(u32, u32, u32) {}
extern "C" void sly1_native_trace_model_build(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_capture_model_mesh(u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_submit_model_part(u32, u32, u32) {}
extern "C" void sly1_native_trace_note_world(u32) {}
extern "C" void sly1_native_trace_world_draw_lists(u32, u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_deferred_requests(u32, u32, u32, u32, u32) {}
extern "C" void sly1_native_trace_draw_object(u32, u32, u32, u32) {}
#endif

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_semaphore_event(const char* event, int identifier, int count,
                                  unsigned int caller) {
    if (!g_startup_trace_enabled ||
        !__atomic_load_n(&g_native_semaphore_trace_active, __ATOMIC_RELAXED))
        return;
    const u32 index = __atomic_fetch_add(&g_semaphore_trace_event_count, 1u,
                                         __ATOMIC_RELAXED);
    if (index >= 64) return;
    std::fprintf(stderr,
                 "[native semaphore] %s id=%d count=%d caller=%08x\n",
                 event, identifier, count, caller);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_trace_null_ramp(void* ramp, void* out, float x, float y, float z,
                             u32 caller) {
    if (!g_startup_trace_enabled) return;
    union FloatBits { float value; u32 bits; };
    write_hex("native host32: null ramp caller ", (int)caller);
    write_hex("native host32: null ramp profile ", (int)ramp);
    write_hex("native host32: null ramp output ", (int)out);
    write_hex("native host32: null ramp x bits ", (int)FloatBits{x}.bits);
    write_hex("native host32: null ramp y bits ", (int)FloatBits{y}.bits);
    write_hex("native host32: null ramp z bits ", (int)FloatBits{z}.bits);
}

void native_pc_sample_handler(int, siginfo_t*, void* context) {
    const auto* state = static_cast<const ucontext_t*>(context);
#if defined(__i386__)
    g_native_pc_sample_pc = state->uc_mcontext.gregs[REG_EIP];
    g_native_pc_sample_sp = state->uc_mcontext.gregs[REG_ESP];
    g_native_pc_sample_bp = state->uc_mcontext.gregs[REG_EBP];
#elif defined(__x86_64__)
    g_native_pc_sample_pc = state->uc_mcontext.gregs[REG_RIP];
    g_native_pc_sample_sp = state->uc_mcontext.gregs[REG_RSP];
    g_native_pc_sample_bp = state->uc_mcontext.gregs[REG_RBP];
#endif
    const u32 sp = (u32)g_native_pc_sample_sp;
    if (sp >= 0xC0000000u && sp < 0xF0000000u) {
        const volatile u32* stack = reinterpret_cast<const volatile u32*>(sp);
        for (u32 i = 0; i < 8; ++i)
            g_native_pc_sample_stack[i] = (sig_atomic_t)stack[i];
    }
    ++g_native_pc_sample_sequence;
}

void install_pc_sampler() {
    struct sigaction action{};
    action.sa_sigaction = native_pc_sample_handler;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR2, &action, nullptr);
}

void trace_game_thread_pc(pthread_t thread, unsigned sample) {
    const int signal_result = pthread_kill(thread, SIGUSR2);
    SDL_Delay(1);
    const u32 pc = (u32)g_native_pc_sample_pc;
    const u32 sp = (u32)g_native_pc_sample_sp;
    const u32 bp = (u32)g_native_pc_sample_bp;
    u32 retail_function = 0;
    u32 native_function = 0;
    for (u32 i = 0; i < sly1::sly1_native_function_address_count; ++i) {
        const auto& entry = sly1::sly1_native_function_addresses[i];
        if (entry.native_address != 0 && entry.native_address <= pc &&
            entry.native_address > native_function) {
            native_function = entry.native_address;
            retail_function = entry.retail_address;
        }
    }
    const u32 offset = pc >= native_function ? pc - native_function : 0;
    std::fprintf(stderr,
                 "[native game thread] sample=%u signal=%d sequence=%d pc=%#010x sp=%#010x bp=%#010x stack=%#010x,%#010x,%#010x,%#010x,%#010x,%#010x,%#010x,%#010x nearest_retail=%#010x native=%#010x offset=%#x\n",
                 sample, signal_result, (int)g_native_pc_sample_sequence,
                 pc, sp, bp,
                 (u32)g_native_pc_sample_stack[0],
                 (u32)g_native_pc_sample_stack[1],
                 (u32)g_native_pc_sample_stack[2],
                 (u32)g_native_pc_sample_stack[3],
                 (u32)g_native_pc_sample_stack[4],
                 (u32)g_native_pc_sample_stack[5],
                 (u32)g_native_pc_sample_stack[6],
                 (u32)g_native_pc_sample_stack[7],
                 retail_function, native_function, offset);
}

volatile u32 g_native_frame_stage = 0;
volatile u32 g_native_frame_count = 0;
volatile u32 g_native_game_thread_tid = 0;
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

extern "C" void sly1_native_prepare_thread_fault_trace() {
    if (g_startup_trace_enabled) {
        install_fault_trace();
        write_hex("native host32: worker fault trace ready ", 1);
    }
}

extern "C" u32 sly1_native_resolve_function(u32 address) {
    return resolve_native(address);
}

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
    const bool trace_graphics = [] {
        const char* value = std::getenv("OPENRAC_STARTUP_TRACE");
        return value != nullptr && value[0] != '0';
    }();
    if (trace_graphics) install_pc_sampler();
    std::atomic<bool> game_finished{false};
    std::thread game_thread([&] {
        g_native_game_thread_tid = (u32)syscall3(224, 0, 0, 0);
        if (g_startup_trace_enabled) install_fault_trace();
        func_00185758((int)*(volatile u32*)0x0027B380u, (int)0x0027B384u);
        game_finished.store(true, std::memory_order_release);
    });
    using NativeInterruptHandler = int (*)(int);
    const auto handle_vblank = reinterpret_cast<NativeInterruptHandler>(
        resolve_native(0x001682C0u));
    if (handle_vblank == nullptr) {
        write_text("native host32: retail VBlank handler is not linked\n");
        game_thread.detach();
        return 1;
    }
    unsigned trace_frames = 0;
    unsigned semaphore_trace_after_frame = 0;
    if (trace_graphics) {
        const char* trace_semaphores_from_start =
            std::getenv("OPENRAC_TRACE_SEMAPHORES_FROM_START");
        const char* delay = std::getenv("OPENRAC_TEST_PRESS_SPACE_AFTER_FRAMES");
        if (trace_semaphores_from_start != nullptr &&
            trace_semaphores_from_start[0] != '0') {
            g_native_semaphore_trace_active = 1;
        } else if (delay != nullptr && delay[0] != '\0' && delay[0] != '0') {
            char* end = nullptr;
            const unsigned long after = std::strtoul(delay, &end, 10);
            if (end != delay && *end == '\0')
                semaphore_trace_after_frame = static_cast<unsigned>(after);
        }
        if (semaphore_trace_after_frame == 0)
            g_native_semaphore_trace_active = 1;
    }
    for (;;) {
        openrac_native_window_present();
        if (trace_graphics) ++trace_frames;
        // GS CSR.FIELD is the only status bit read by the Sly display path.
        // Advance it with the native window's presentation cadence.
        __atomic_fetch_xor((u32*)0x12001000u, 1u << 13, __ATOMIC_RELAXED);
        // The desktop window supplies the vertical-blank event that would
        // wake the retail display semaphore on the console.
        handle_vblank(2);
        const int quit = openrac_native_window_poll_input();
        if (trace_graphics &&
            trace_frames >= semaphore_trace_after_frame)
            g_native_semaphore_trace_active = 1;
        if (quit) {
            break;
        }
        if (trace_graphics && (trace_frames % 60u) == 0) {
            trace_game_thread_pc(game_thread.native_handle(), trace_frames / 60u);
            std::fprintf(stderr, "[native graphics] dma=%llu accepted=%llu rejected=%llu bytes=%llu\n",
                         openrac_native_renderer_dma_submit_count(),
                         openrac_native_renderer_dma_accept_count(),
                         openrac_native_renderer_dma_reject_count(),
                         openrac_native_renderer_dma_bytes());
            std::fprintf(stderr,
                         "[native startup] entry_returned=%u callback=%u iop_stage=%u cd_reads=%u sectors=%u game_frames=%u game_tid=%u frame_stage=%u world=%#x ui_state=%u ui_stack=%u resume=%d load=%u movie=%u visible_ui=%u text_ui=%u pad_status=%u pad_mode=%u pad_flags=%#x pad_route=%u pad_select=%u blank_pad_status=%u pause_open=%u pause_page=%d pause_mode=%u fade=%u stack_top=%#x top_vt=%#x top_mode=%u top_text=%u card_state=%d card_target=%d card_display=%u card_progress=%d memory_gate=%u element_state=%u card_thread=%u card_job=%#x card_result=%d card_semaphore=%u game_elapsed_bits=%#x card_state_time_bits=%#x card_display_time_bits=%#x\n",
                         game_finished.load(std::memory_order_acquire) ? 1u : 0u,
                         sly1_native_startup_callback_index,
                         sly1_native_iop_boot_stage,
                         g_disc_read_count, g_disc_sectors_read,
                         __atomic_load_n(&g_native_frame_count, __ATOMIC_RELAXED),
                         __atomic_load_n(&g_native_game_thread_tid, __ATOMIC_RELAXED),
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
    if (!g_startup_trace_enabled && (tag & 0xFFFF0000u) != 0x1F280000u)
        return;
    write_hex("native host32: trace tag ", (int)tag);
    write_hex("native host32: trace value ", (int)value);
}

extern "C" __attribute__((no_instrument_function)) void
sly1_native_oscillator_probe(u32 tag, u32 value) {
    std::fprintf(stderr, "[native oscillator] tag=%#010x value=%#010x\n",
                 tag, value);
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
sly1_native_trace_thread_service(u32 service, u32 result, u32 a0, u32 a1) {
    if (!g_startup_trace_enabled) return;
    write_hex("native host32: thread service ", (int)service);
    write_hex("native host32: thread result ", (int)result);
    write_hex("native host32: thread arg0 ", (int)a0);
    write_hex("native host32: thread arg1 ", (int)a1);
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
