// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only native replacement for Sly's extended Count snapshot.
extern "C" long long func_001430F8();

extern "C" void func_001430C8() {
    auto* previous = reinterpret_cast<unsigned long long*>(0x00261878u);
    auto* wraps = reinterpret_cast<unsigned long long*>(0x0027C000u);
    auto* extended = reinterpret_cast<unsigned long long*>(0x00261870u);

    const auto first = static_cast<unsigned int>(func_001430F8());
    unsigned long long wrap_count = *wraps;
    *previous = first;

    const auto second = static_cast<unsigned int>(func_001430F8());
    if (second < first) ++wrap_count;
    *previous = second;
    *wraps = wrap_count;
    *extended = (wrap_count << 32) | second;
}
