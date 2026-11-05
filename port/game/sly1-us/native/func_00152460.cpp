// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only host translation for Sly's list link address calculation. The
// retail helper adds a list's link offset to an EE address. Linux prevents
// this process from mapping the low EE page, so redirect non-null results
// below 0x10000 to an equal offset in the mapped guest-RAM shadow at +0x10000.
// Keep the matching helper staged under its original body for later use.
extern "C" unsigned int func_00152460(void* list, unsigned int value) {
    unsigned int list_address = reinterpret_cast<unsigned int>(list);
    if (list_address != 0 && list_address < 0x10000u)
        list_address += 0x10000u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(list_address);
    const unsigned int address = value + *reinterpret_cast<const unsigned int*>(bytes + 8);
    return address != 0 && address < 0x10000u ? address + 0x10000u : address;
}
