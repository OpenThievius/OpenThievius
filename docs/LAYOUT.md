# How the repository is organised

```
OpenThievius/
├── README.md, CONTRIBUTING.md, AGENTS.md
├── LICENSE, LICENSE.md, THIRD_PARTY_NOTICES.md, CREDITS.md
├── games/                  one directory per game
│   └── sly1/
│       └── game.json       discs, checksums, roles, where the decompilation lives
├── port/                   the native PC port (not started)
├── tools/                  repository-wide tools: openthievius.py, disc.py
├── tests/                  the tools' tests and the repository check
├── docs/                   knowledge and rules that span the project
│   ├── policy/             sourcing policy
│   └── decomp/             the decompilations: where they are, how they are checked
├── shared/                 files a game project keeps an identical copy of
├── baserom/                your own disc images (ignored, except its README)
└── toolchains/             compilers you supply (ignored, except its README)
```

Beside it, as a sister checkout:

```
sly1-decomp/                the matching decompilation of Sly Cooper (its own repository)
```

## The rules that keep it organised

**Each decompilation is its own repository.** sly1-decomp has its own README,
license file, tools, checks and docs, and its build never reaches into
OpenThievius. `games/sly1/game.json` names it (`project.dir`, `../sly1-decomp`
relative to this repository); `OPENTHIEVIUS_SOURCE_SLY1` points elsewhere.

**Only shared things live here**: the port, the tools that serve every game,
the policies, and the cross-game knowledge.

**Facts are kept once and checked.** A game's versions and checksums are in
its project's `config/versions.json` and in `game.json`;
`python3 tools/openthievius.py check` fails when they differ. A file copied
into a project is listed in `shared/files.json`, and the same check fails
when the copy drifts.

**Generated and personal files stay out of git.** Disc images, extracted
data, build output and toolchains are ignored. The tables in the READMEs are
generated (`python3 tools/openthievius.py tables`) but committed, because
they are what readers see.

## Where a new file goes

| You are adding | Put it in |
|---|---|
| Decompiled code, a symbol map, a build change | sly1-decomp |
| The native port's code | `port/` |
| A tool that serves several games or the repository | `tools/`, with a test in `tests/` |
| A decision or rule for everyone | `docs/policy/`, and CONTRIBUTING.md if it changes how people work |
| A new game version | its project, then its entry in `games/<game>/game.json`, then `tools/openthievius.py tables` |

## Names

- Games: `sly1` (*Sly Cooper and the Thievius Raccoonus*, 2002). Later games
  would be `sly2`, `sly3`.
- Versions: `us` (NTSC-U), `eu` (PAL), and prerelease builds by date
  (`may19`).
- Commit scopes: `sly1`, `port`, `tools`, `docs`, `policy`.

## The branch

Every repository's primary branch is `master`.
