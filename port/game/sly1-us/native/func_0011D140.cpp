// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only silent sound bank load. The host build has no Sly IOP sound module.
// Keep this separate from matching source so it can be removed when the native
// port gains sound support or this path is no longer needed.
extern "C" int func_0011D140(int bank, int flags) {
    (void)bank;
    (void)flags;
    return 0;
}
