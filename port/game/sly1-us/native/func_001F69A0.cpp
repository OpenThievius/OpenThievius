// SPDX-License-Identifier: AGPL-3.0-or-later
// Host boundary for the EE EnableInterruptCause syscall stub.
extern "C" unsigned long long openrac_guest_kernel_syscall(
    unsigned int service, unsigned long long a0, unsigned long long a1,
    unsigned long long a2, unsigned long long a3, unsigned long long a4,
    unsigned long long a5, unsigned long long a6, unsigned long long a7);

extern "C" int func_001F69A0(int cause) {
    return static_cast<int>(openrac_guest_kernel_syscall(
        0x14u, static_cast<unsigned int>(cause), 0, 0, 0, 0, 0, 0, 0));
}
