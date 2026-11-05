// SPDX-License-Identifier: AGPL-3.0-or-later
// Native replacement for the Sly libpad port-open and query boundary.
// Retail forwards these requests to padman through SIF RPC. The native port
// reads a DualShock-compatible report from its SDL input layer instead.

extern "C" int openrac_native_pad_read(int port, unsigned char* report,
                                         unsigned int size);

namespace {

struct NativePad {
    unsigned int report_buffer;
    unsigned char open;
    unsigned char connected;
    unsigned char report[32];
};

NativePad g_pads[2][4]{};

bool refresh(int port, int slot) {
    if (port < 0 || port >= 2 || slot < 0 || slot >= 4) return false;
    NativePad& pad = g_pads[port][slot];
    if (!pad.open) return false;
    if (!openrac_native_pad_read(port, pad.report, sizeof(pad.report))) {
        pad.report[0] = 0xFF;
        pad.connected = 0;
        return false;
    }
    pad.connected = pad.report[0] == 0 ? 1 : 0;
    if (pad.report_buffer != 0) {
        auto* output = reinterpret_cast<unsigned char*>(pad.report_buffer);
        for (unsigned int i = 0; i < sizeof(pad.report); ++i)
            output[i] = pad.report[i];
    }
    return pad.connected != 0;
}

} // namespace

extern "C" int func_002042C8(int port, int slot, int buffer) {
    if (port < 0 || port >= 2 || slot < 0 || slot >= 4 || buffer == 0 ||
        (static_cast<unsigned int>(buffer) & 0x3Fu) != 0)
        return 0;

    NativePad& pad = g_pads[port][slot];
    if (pad.open) return 0;
    pad.report_buffer = static_cast<unsigned int>(buffer);
    pad.open = 1;
    refresh(port, slot);
    return 1;
}

extern "C" int func_00204590(int port, int slot) {
    if (port < 0 || port >= 2 || slot < 0 || slot >= 4 ||
        !g_pads[port][slot].open)
        return 0x63;
    return refresh(port, slot) ? 6 : 2;
}

extern "C" int func_00204510(int port, int slot, unsigned char* report) {
    if (report == nullptr || port < 0 || port >= 2 || slot < 0 || slot >= 4 ||
        !g_pads[port][slot].open)
        return 0;
    const bool connected = refresh(port, slot);
    const NativePad& pad = g_pads[port][slot];
    for (unsigned int i = 0; i < sizeof(pad.report); ++i)
        report[i] = pad.report[i];
    return connected ? 1 : 0;
}

extern "C" int func_002047F8(int port, int slot, int info, int index) {
    if (port < 0 || port >= 2 || slot < 0 || slot >= 4 || index != 0 ||
        !g_pads[port][slot].open || !refresh(port, slot))
        return 0;

    // libpad's first info query returns the controller type from the report;
    // type 7 is the pressure-capable DualShock mode used by Sly 1.
    if (info == 1) return 7;
    return 0;
}
