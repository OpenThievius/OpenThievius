// SPDX-License-Identifier: AGPL-3.0-or-later
// Port-only completion for Sly's startup SIF RPC waits. The sound IOP peer and
// libmc are absent in the native host. Sound bootstrap binds are treated as
// ready, while libmc's bind command is allowed to reach func_001F8290, which
// reports the unavailable service so func_002093F0 can return its error
// instead of spinning forever. The matching RPC implementation remains in
// the staged decomp source.
extern "C" int func_001F80C0(void* context, int service_id, int mode) {
    (void)mode;
    if (context == nullptr ||
        (service_id != 0x123456 && service_id != 0x123457 &&
         service_id != 0x80000400)) {
        return -1;
    }
    auto* words = static_cast<int*>(context);
    words[4] = 0; // f10: no asynchronous result is pending.
    words[9] = 1; // f24: the caller's SIF wait can proceed to its RPC result.
    return 0;
}
