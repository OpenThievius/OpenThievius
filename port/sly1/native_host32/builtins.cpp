// SPDX-License-Identifier: AGPL-3.0-or-later
// Freestanding compiler support for Linux/i386 without multilib libraries.
using u64 = unsigned long long;

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

extern "C" float sqrtf(float value) {
    float result;
    __asm__ volatile("flds %1; fsqrt; fstps %0" : "=m"(result) : "m"(value));
    return result;
}
