# Contributing to OpenThievius

Thanks for helping. This guide follows
[OpenRAC's](https://github.com/OpenRAC/OpenRAC/blob/master/CONTRIBUTING.md), the sister project
of the same maintainers. Decompilation work happens in
[sly1-decomp](https://github.com/OpenThievius/sly1-decomp) (its CONTRIBUTING.md applies there);
this repository holds the tools, the policies and, later, the native port.

## Before you start

1. Read the [sourcing policy](docs/policy/SOURCING.md). It is short, and it is the one rule
   nobody may bend: we derive everything ourselves from the retail disc. Nothing from other
   projects, nothing from the prototype, and no proprietary material in the repositories.
2. See where things go: [docs/LAYOUT.md](docs/LAYOUT.md).

## Ground rules

- **Never commit or post game data or proprietary material**: disc images, executables,
  extracted data, generated assembly, build output, toolchains, SDK files. Check `git status`
  before every commit; `tests/check_repository.py` must pass.
- **Regenerate generated files with their tools**, never by hand
  (`python3 tools/openthievius.py tables`).
- **A shared file changes everywhere at once** (`shared/files.json`;
  `python3 tools/openthievius.py check`).
- **No third-party code.** Write it yourself. The one exception is OpenRAC's own work, used
  openly and credited ([docs/policy/SOURCING.md](docs/policy/SOURCING.md#openrac)).
- **AGPL-3.0-or-later.** By contributing you license your work under it.
- **Keep docs short and plain.** Plain English, short sentences, tables where they help.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/), as in OpenRAC:

```
type(scope): short summary

What changed and why, wrapped at 72 columns, and what you verified.
```

- **Types**: `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`
  (regenerated files, imports), `style` (formatting only), `revert`.
- **Scopes**: a part (`port`, `tools`, `docs`, `policy`, `shared`) or a game (`sly1`). Narrower
  is fine (`port/renderer`).
- **The summary** says what the change does or what is now true, in lower case, without a full
  stop.
- **The body** is prose: what changed, why, and what you ran to verify it, with what it printed.
- **One logical change per commit**, each leaving the tools and checks working. Regenerated
  tables go in the commit that changes what they are made from.
- **No attribution lines**: no `Co-authored-by:` or other trailers for AI tools.

```
fix(tools): skip a reference disc in setup
feat(sly1): the PAL disc's checksums
docs(policy): what may come from OpenRAC
```

## Pull requests

- One topic per pull request, titled like a commit summary, against `master`.
- Say what you verified and how: the commands and their results.
- Never include generated or disc-derived files.

## Tests

```sh
python3 -m unittest discover -s tests      # the repository's tools
python3 tests/check_repository.py          # no proprietary material, licence headers, tables
python3 tools/openthievius.py check        # game.json and sly1-decomp agree, shared files
```

sly1-decomp has its own checks; its README names them.

## Working with other people

Disagree kindly, share what you find, welcome newcomers, and be patient with people's work.

## Working with an AI assistant

The rules above apply the same way. On top of them, an assistant:

- pushes, opens pull requests or publishes only when asked;
- asks before automated game runs (scripted play, screenshots, sweeps), once there is a port to
  run;
- reports what it checked as it is, failures included;
- follows the `AGENTS.md` of the repository it works in.
