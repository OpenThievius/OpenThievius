// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only silent service for Sly's IOP sound RPC boundary. The retail RPC
// protocol depends on the IOP sound module, which is not present in a native
// host process. Report successful synchronous completion so sound setup and
// later optional sound requests cannot stall gameplay; the matching RPC
// implementation remains available in the staged decomp source.
extern "C" int func_0011D968(int id, int size, void* data) {
    (void)id;
    (void)size;
    (void)data;
    return 0;
}
