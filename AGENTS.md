# Instructions for agents

This file is for AI agents (Claude, Codex or others) working in this repository. Read it
completely, then README.md, CONTRIBUTING.md, docs/LAYOUT.md and docs/policy/SOURCING.md. The
repository follows the structure of OpenRAC: keep it that way.

## What this repository is

The umbrella of the OpenThievius project: the game registry (`games/*/game.json`), the tools
that set up every game from your discs, the policies, and later the native PC port (`port/`).
The decompilation itself is a sister repository, sly1-decomp, checked out beside this one
(`../sly1-decomp`, or `OPENTHIEVIUS_SOURCE_SLY1`). Decompilation work happens there, under its
own AGENTS.md.

## Commands

| Command | What it does |
| --- | --- |
| `python3 tools/openthievius.py discs [PATH ...] [--full]` | which game and version each disc image is |
| `python3 tools/openthievius.py setup [GAME[/VERSION]] [--discs PATH ...]` | writes each retail boot executable into its project's `baserom/` (the prototype is skipped), links the toolchain |
| `python3 tools/openthievius.py tables [--check]` | regenerates the README tables from `games/*/game.json` |
| `python3 tools/openthievius.py check` | `game.json` against each project's `config/versions.json`, and the shared files |
| `python3 -m unittest discover -s tests` | the tools' tests (CI) |
| `python3 tools/proprietary.py` | scans what git would commit for game data and proprietary material |
| `python3 tests/check_repository.py` | `proprietary.py`, licence headers, tables current (CI) |

## Rules

- **Derive everything ourselves** (docs/policy/SOURCING.md). Work from the retail executable of
  your own disc and public hardware documentation. Take nothing from anyone else: no code,
  symbol maps, splits, names or notes from other decompilations, ports or research on the Sly
  games, and no third-party code. Never Sony SDK source, samples or headers, never leaked or NDA
  material.
- **OpenRAC is the exception.** Its own work may be used, openly credited: name it in the commit
  body and list it in THIRD_PARTY_NOTICES.md (docs/policy/SOURCING.md, "OpenRAC"). Not what
  OpenRAC took from others, and no Ratchet & Clank game code.
- **Nothing from the prototype**: no code, data, names, signatures, types or file lists, and no
  tool that extracts them. What you write after looking at it must be entirely your own.
- **No proprietary material ever touches the repository**: disc images, executables, extracted
  data, build output, dumps, compilers or links to them, SDK headers. `tools/proprietary.py`
  (run by `tests/check_repository.py` and CI) checks this; never weaken it to get a file in.
- **Licence.** Everything is AGPL-3.0-or-later. New source files start with
  `SPDX-License-Identifier: AGPL-3.0-or-later`.
- **Generated tables** are regenerated with `tools/openthievius.py tables`, never edited by hand.
- **Shared files** (`shared/files.json`) change in every copy at once.
- **English** everywhere. **Never delete what you did not create.**
- **The branch is `master`**, in every OpenThievius repository. Never rewrite its history and
  never force-push it.

## Commits

As in OpenRAC (CONTRIBUTING.md, "Commits"):

- Conventional Commits with a scope: `feat(tools): ...`, `docs(policy): ...`, `feat(sly1): ...`,
  `feat(port): ...`, `chore: ...`. The summary says what the change does or what is now true, in
  lower case, without a full stop.
- A prose body wrapped at 72 columns: what changed, why, and what you verified (the commands
  and what they printed).
- One logical change per commit, each leaving the tools and checks working.
- No `Co-authored-by:` trailers or other attribution lines for AI tools.
- Push and open pull requests only when asked.
