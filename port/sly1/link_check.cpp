// SPDX-License-Identifier: AGPL-3.0-or-later
// Link-only entry point. This deliberately never calls openrac_game_run().
#include "openrac/game_host.h"

int main() {
    openrac_game_register_functions();
    return openrac_game.id && openrac_game.entry ? 0 : 1;
}
