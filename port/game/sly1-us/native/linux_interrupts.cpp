// SPDX-License-Identifier: AGPL-3.0-or-later
// Native registry for the retail EE interrupt-handler syscalls. A host event
// source can dispatch these callbacks without executing PS2 exception code.
using InterruptHandler = void (*)(void);

namespace {
struct NativeInterrupt {
    InterruptHandler handler;
    int mode;
    int enabled;
};

NativeInterrupt g_interrupts[32]{};
}

extern "C" int func_001F6950(int interrupt, InterruptHandler handler, int mode) {
    if (interrupt < 0 || interrupt >= 32 || handler == nullptr) return -1;
    g_interrupts[interrupt].handler = handler;
    g_interrupts[interrupt].mode = mode;
    g_interrupts[interrupt].enabled = 0;
    return 0;
}

extern "C" int func_001F6DA8(int interrupt) {
    if (interrupt < 0 || interrupt >= 32 || g_interrupts[interrupt].handler == nullptr)
        return -1;
    g_interrupts[interrupt].enabled = 1;
    return 0;
}

extern "C" void sly1_native_dispatch_interrupt(int interrupt) {
    if (interrupt < 0 || interrupt >= 32) return;
    const NativeInterrupt& entry = g_interrupts[interrupt];
    if (entry.enabled && entry.handler != nullptr) entry.handler();
}
