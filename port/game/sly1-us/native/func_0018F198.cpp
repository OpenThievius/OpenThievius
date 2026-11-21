// SPDX-License-Identifier: AGPL-3.0-or-later
// Native port backfill: bridge the retail movie call to an optional host player.
#include "common.h"
#include "identity_key.h"
#include "movie_player.h"

extern "C" void sly1_native_trace_movie_call(
    u32 self, u32 key, u32 key_lsn, u32 key_bytes,
    u32 key_word_2, u32 key_word_6,
    u32 queued, u32 queued_next) __attribute__((weak));
extern "C" int sly1_native_play_movie(u32 lsn, u32 bytes, u32 resource_id)
    __attribute__((weak));

void PlayMovie(MoviePlayer* self, IdentityKey* key)
{
    const u32 lsn = key != nullptr ? key->words[0] ^ key->words[5] : 0u;
    const u32 bytes = key != nullptr ? key->words[1] ^ key->words[7] : 0u;
    const u32 resource_id = key != nullptr ? key->words[2] ^ key->words[6] : 0u;
    if (sly1_native_trace_movie_call != nullptr) {
        sly1_native_trace_movie_call(
            static_cast<u32>(reinterpret_cast<uptr>(self)),
            static_cast<u32>(reinterpret_cast<uptr>(key)), lsn, bytes,
            key != nullptr ? key->words[2] : 0u,
            key != nullptr ? key->words[6] : 0u,
            self != nullptr ? static_cast<u32>(reinterpret_cast<uptr>(self->queued)) : 0u,
            self != nullptr ? static_cast<u32>(reinterpret_cast<uptr>(self->queuedNext)) : 0u);
    }
    // This Linux-only development path uses the installed ffmpeg tool to show
    // the retail PSS from the user's disc. The native movie decoder remains a
    // separate port task; unmatched movies keep the no-op behavior.
    if (sly1_native_play_movie != nullptr)
        (void)sly1_native_play_movie(lsn, bytes, resource_id);
}
