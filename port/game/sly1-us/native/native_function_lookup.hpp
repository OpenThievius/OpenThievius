// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace sly1 {

struct NativeFunctionAddress {
    unsigned int retail_address;
    unsigned int native_address;
};

extern const NativeFunctionAddress sly1_native_function_addresses[];
extern const unsigned int sly1_native_function_address_count;

inline unsigned int native_function_address(unsigned int retail_address) {
    unsigned int first = 0;
    unsigned int last = sly1_native_function_address_count;
    while (first < last) {
        const unsigned int middle = first + (last - first) / 2;
        if (sly1_native_function_addresses[middle].retail_address < retail_address) {
            first = middle + 1;
        } else {
            last = middle;
        }
    }
    if (first == sly1_native_function_address_count ||
        sly1_native_function_addresses[first].retail_address != retail_address) {
        // The freestanding host32 loader patches retail function pointers in
        // data sections in place. Accept already-patched values too, so
        // callback backfills work both before and after that conversion.
        for (unsigned int i = 0; i < sly1_native_function_address_count; ++i) {
            if (sly1_native_function_addresses[i].native_address == retail_address) {
                return retail_address;
            }
        }
        return 0;
    }
    return sly1_native_function_addresses[first].native_address;
}

}  // namespace sly1
