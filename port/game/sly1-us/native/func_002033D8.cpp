// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only result for the unavailable PS2 memory-card RPC service. The
// matching body is not present in the current decomp source. Native callers
// check this gate before issuing optional libmc status/configuration RPCs and
// already return their no-service result when it is false; do not claim a
// memory card is connected.
extern "C" int func_002033D8(int kind) {
    (void)kind;
    return 0;
}
