# Third-party notices

OpenThievius holds **no third-party code** apart from what comes from OpenRAC, the sister
project of the same maintainers ([docs/policy/SOURCING.md](docs/policy/SOURCING.md#openrac)).
The free tools it runs are installed by each contributor and are not distributed with it, and
the compiler is proprietary and never part of it ([toolchains/README.md](toolchains/README.md)).

## From OpenRAC

Written by OpenRAC's maintainers and used here under the AGPL-3.0-or-later.

| Here | Adapted from OpenRAC's | What changed |
|---|---|---|
| `docs/policy/SOURCING.md` | `docs/policy/SOURCING.md` | stricter: nothing from other projects or the prototype; the OpenRAC exception; the proprietary scan |
| `docs/LAYOUT.md`, `games/README.md` | `docs/LAYOUT.md`, `games/README.md` | this repository's layout, with the decompilation as a sister checkout |
| `CONTRIBUTING.md`, `AGENTS.md` | `CONTRIBUTING.md`, rac3-decomp's `AGENTS.md` | the commit, pull request and assistant conventions kept; the rules rewritten |
| `LICENSE.md` | `LICENSE.md` | one license, the AGPL-3.0-or-later |
| `tools/openthievius.py` | `tools/openrac.py` (`discs`, `setup`, `tables`) | written anew for game.json's shape here, with `check` |
| `baserom/README.md`, `toolchains/README.md`, `.gitignore` | the same files | this project's discs, compiler and paths |
| `.github/workflows/checks.yml` | rac3-decomp's `checks.yml` | this repository's checks |
