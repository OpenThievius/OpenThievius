// SPDX-License-Identifier: AGPL-3.0-or-later
// Freestanding compiler support for Linux/i386 without multilib libraries.
using u64 = unsigned long long;

extern "C" void* memcpy(void* destination, const void* source, unsigned int size) {
    auto* dst = static_cast<unsigned char*>(destination);
    const auto* src = static_cast<const unsigned char*>(source);
    for (unsigned int i = 0; i < size; ++i) dst[i] = src[i];
    return destination;
}

extern "C" void* memset(void* destination, int value, unsigned int size) {
    auto* dst = static_cast<unsigned char*>(destination);
    for (unsigned int i = 0; i < size; ++i) dst[i] = static_cast<unsigned char>(value);
    return destination;
}

extern "C" void* memmove(void* destination, const void* source, unsigned int size) {
    auto* dst = static_cast<unsigned char*>(destination);
    const auto* src = static_cast<const unsigned char*>(source);
    if (dst < src) {
        for (unsigned int i = 0; i < size; ++i) dst[i] = src[i];
    } else if (dst > src) {
        for (unsigned int i = size; i != 0; --i) dst[i - 1] = src[i - 1];
    }
    return destination;
}

extern "C" unsigned int strlen(const char* value) {
    unsigned int size = 0;
    while (value[size] != '\0') ++size;
    return size;
}

static u64 divide_unsigned(u64 numerator, u64 denominator, u64* remainder) {
    u64 quotient = 0;
    u64 rest = 0;
    if (denominator == 0) {
        if (remainder) *remainder = 0;
        return 0;
    }
    for (int bit = 63; bit >= 0; --bit) {
        rest = (rest << 1) | ((numerator >> bit) & 1);
        if (rest >= denominator) {
            rest -= denominator;
            quotient |= (u64)1 << bit;
        }
    }
    if (remainder) *remainder = rest;
    return quotient;
}

extern "C" long long __divdi3(long long numerator, long long denominator) {
    const bool negative = (numerator < 0) != (denominator < 0);
    u64 n = numerator < 0 ? 0u - (u64)numerator : (u64)numerator;
    const u64 d = denominator < 0 ? 0u - (u64)denominator : (u64)denominator;
    if (d == 0) return 0;
    u64 quotient = 0, remainder = 0;
    for (int bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) | ((n >> bit) & 1);
        if (remainder >= d) {
            remainder -= d;
            quotient |= (u64)1 << bit;
        }
    }
    return negative ? (long long)(0u - quotient) : (long long)quotient;
}

extern "C" unsigned long long __udivdi3(unsigned long long numerator,
                                         unsigned long long denominator) {
    return divide_unsigned(numerator, denominator, nullptr);
}

extern "C" unsigned long long __umoddi3(unsigned long long numerator,
                                         unsigned long long denominator) {
    u64 remainder;
    divide_unsigned(numerator, denominator, &remainder);
    return remainder;
}

extern "C" long long __moddi3(long long numerator, long long denominator) {
    const u64 n = numerator < 0 ? 0u - (u64)numerator : (u64)numerator;
    const u64 d = denominator < 0 ? 0u - (u64)denominator : (u64)denominator;
    u64 remainder;
    divide_unsigned(n, d, &remainder);
    return numerator < 0 ? (long long)(0u - remainder) : (long long)remainder;
}

extern "C" {
void* __dso_handle = &__dso_handle;
}

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    // This freestanding process terminates directly through its Linux syscall
    // entry and has no C++ static destruction phase.
    return 0;
}

extern "C" float sqrtf(float value) {
    float result;
    __asm__ volatile("flds %1; fsqrt; fstps %0" : "=m"(result) : "m"(value));
    return result;
}

extern "C" float fminf(float a, float b) { return a < b ? a : b; }
extern "C" float fmaxf(float a, float b) { return a > b ? a : b; }

// The retail exception runtime is linked as address-preserving code even though
// native gameplay does not throw C++ exceptions. These two ABI entry points
// satisfy its unused virtual teardown and diagnostic references.
void operator delete(void*, unsigned int) noexcept {}
extern "C" const char* sly1_exception_what(const void*) asm("_ZNK9exception4whatEv");
extern "C" const char* sly1_exception_what(const void*) { return "Sly exception"; }
