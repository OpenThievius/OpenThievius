// SPDX-License-Identifier: AGPL-3.0-or-later
// The retail implementation installs IOP SIF command handlers and waits for
// an IOP completion packet. This host-native build has no IOP command engine;
// its disc and graphics services are provided directly by the native host.
// Keep this startup replacement separate so matching Sly C can take the
// retail address when it becomes available.
extern "C" int D_00277134;

extern "C" int func_001F7BE8(int mode) {
    (void)mode;
    D_00277134 = 1;
    return 0;
}
