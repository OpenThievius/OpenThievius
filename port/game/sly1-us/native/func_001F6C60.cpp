// SPDX-License-Identifier: AGPL-3.0-or-later
// Native implementations of the two retail GS interrupt-mask syscall
// wrappers. The shared host runtime owns this mask while SDL presents frames.
extern "C" unsigned long long openrac_guest_kernel_syscall(
    unsigned int service, unsigned long long a0, unsigned long long a1,
    unsigned long long a2, unsigned long long a3, unsigned long long a4,
    unsigned long long a5, unsigned long long a6, unsigned long long a7);

extern "C" int func_001F6C60(...) {
    return static_cast<int>(openrac_guest_kernel_syscall(0x70u, 0, 0, 0, 0,
                                                        0, 0, 0, 0));
}

extern "C" int func_001F6C70(unsigned long long mask) {
    return static_cast<int>(openrac_guest_kernel_syscall(0x71u, mask, 0, 0, 0,
                                                        0, 0, 0, 0));
}
