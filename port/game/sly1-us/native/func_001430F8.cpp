// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's CP0 Count read.
namespace {

struct NativeTimespec {
    long seconds;
    long nanoseconds;
};

int monotonic_time(NativeTimespec* time) {
    int result;
    // Linux i386 clock_gettime syscall. The host32 build intentionally avoids
    // libc's 32-bit headers so it can be compiled with the project's freestanding
    // multilib toolchain.
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "0"(265), "b"(1), "c"(time)
        : "memory", "cc");
    return result;
}

}  // namespace

extern "C" long long func_001430F8() {
    NativeTimespec now{};
    if (monotonic_time(&now) != 0) return 0;
    return static_cast<long long>(now.seconds) * 294912000LL +
           static_cast<long long>(now.nanoseconds) * 294912000LL / 1000000000LL;
}
