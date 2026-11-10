// SPDX-License-Identifier: AGPL-3.0-or-later
// Host boundary for the EE AddInterruptHandler syscall stub. Native display
// submission completes synchronously, so the callback is recorded by the
// shared host kernel service and is not delivered as a PS2 interrupt.
extern "C" unsigned long long openrac_guest_kernel_syscall(
    unsigned int service, unsigned long long a0, unsigned long long a1,
    unsigned long long a2, unsigned long long a3, unsigned long long a4,
    unsigned long long a5, unsigned long long a6, unsigned long long a7);

extern "C" int func_001F6950(int cause, void (*handler)(void), int argument) {
    return static_cast<int>(openrac_guest_kernel_syscall(
        0x10u, static_cast<unsigned int>(cause),
        static_cast<unsigned int>(reinterpret_cast<unsigned long>(handler)),
        static_cast<unsigned int>(argument), 0, 0, 0, 0, 0));
}
