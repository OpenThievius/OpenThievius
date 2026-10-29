# SPDX-License-Identifier: AGPL-3.0-or-later
"""OpenThievius's repository tool: discs, setup, tables and consistency checks.

    python3 tools/openthievius.py discs [PATH ...] [--full]   # identify disc images
    python3 tools/openthievius.py setup [GAME[/VERSION]] [--discs PATH]
    python3 tools/openthievius.py tables [--check]           # the generated README tables
    python3 tools/openthievius.py check                      # game.json against each project, shared files

Each game is described by games/<game>/game.json. Its decompilation is a sister
checkout beside OpenThievius (the game.json's project.dir, "../sly1-decomp"),
or wherever the variable its project.env names points.
"""

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import disc  # noqa: E402

ROOT = HERE.parent
GAMES = ROOT / "games"
BASEROM = ROOT / "baserom"
TOOLCHAINS = ROOT / "toolchains"
SHARED = ROOT / "shared" / "files.json"
IMAGE_SUFFIXES = {".iso", ".img", ".bin"}

# The toolchain link each project expects, and what it points at under toolchains/.
TOOLCHAIN_LINKS = {"sly1": ("toolchain", "sn-prodg-24/local/sce/ee/gcc")}


def games():
    out = {}
    for path in sorted(GAMES.glob("*/game.json")):
        with open(path, encoding="utf-8") as f:
            game = json.load(f)
        out[game["id"]] = game
    return out


def project_dir(game):
    env = os.environ.get(game["project"]["env"])
    return Path(env).expanduser() if env else (ROOT / game["project"]["dir"]).resolve()


def images(paths):
    """Disc images in these files and folders, and one folder level down (not in git checkouts)."""
    def in_dir(d):
        return sorted(c for c in d.iterdir() if c.is_file() and c.suffix.lower() in IMAGE_SUFFIXES
                      and c.stat().st_size >= 17 * disc.SECTOR)

    for p in paths:
        p = Path(p)
        if p.is_dir():
            yield from in_dir(p)
            for sub in sorted(c for c in p.iterdir() if c.is_dir() and not (c / ".git").exists()):
                yield from in_dir(sub)
        elif p.is_file():
            yield p


def identify(image, table):
    """(game, version, boot bytes) for a disc image, or (None, None, reason)."""
    try:
        with disc.Disc(str(image)) as d:
            name = d.boot_name()
            data = d.read_file(name)
    except disc.DiscError as e:
        return None, None, str(e)
    sha1 = hashlib.sha1(data).hexdigest()
    for gid, game in table.items():
        for key, v in game["versions"].items():
            if v["boot"]["name"] == name and v["boot"]["sha1"] == sha1:
                return gid, key, data
    return None, None, "boots %s (sha1 %s), which no game.json knows" % (name, sha1)


def cmd_discs(args):
    table = games()
    found = 0
    for image in images(args.paths or [BASEROM]):
        gid, key, data = identify(image, table)
        if gid is None:
            print("unknown  %s: %s" % (image.name, data))
            continue
        found += 1
        v = table[gid]["versions"][key]
        line = "%-6s %-6s %-12s %s" % (gid, key, v["serial"], image.name)
        if args.full:
            c = disc.checksums(str(image))
            ok = all(c[k] == v["disc"][k] for k in ("size", "crc32", "md5", "sha1", "sha256"))
            line += "  disc %s" % ("matches" if ok else "differs from game.json (sha1 %s)" % c["sha1"])
        print(line)
    return 0 if found else 1


def cmd_setup(args):
    table = games()
    want_game, _, want_version = (args.target or "").partition("/")
    if want_game and want_game not in table:
        print("error: unknown game %r (known: %s)" % (want_game, ", ".join(table)))
        return 1
    done = set()
    for image in images(args.discs or [BASEROM]):
        gid, key, data = identify(image, table)
        if gid is None or (want_game and gid != want_game) or (want_version and key != want_version):
            continue
        game = table[gid]
        if game["versions"][key]["role"] == "reference":
            print("skip: %s/%s %s: a reference disc; nothing is taken from it" % (gid, key, image.name))
            continue
        dest_root = project_dir(game)
        if not dest_root.is_dir():
            print("missing: %s is not checked out at %s (clone %s there, or set %s)"
                  % (game["project"]["name"], dest_root, game["project"]["repository"], game["project"]["env"]))
            return 1
        out = dest_root / game["versions"][key]["baserom"]
        if out.exists():
            state = "already there" if hashlib.sha1(out.read_bytes()).hexdigest() == game["versions"][key]["boot"]["sha1"] \
                else "EXISTS WITH OTHER CONTENT, left alone"
        else:
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_bytes(data)
            state = "written from %s" % image.name
        print("ok:   %s/%s %s (%s)" % (gid, key, out, state))
        done.add(gid)
    for gid in sorted(done):
        link_toolchain(gid, project_dir(table[gid]))
        print("next: cd %s && python3 tools/setup_asm.py" % project_dir(table[gid]))
    if not done:
        print("no disc for %s found in %s" % (args.target or "any game", ", ".join(map(str, args.discs or [BASEROM]))))
        return 1
    return 0


def link_toolchain(gid, dest_root):
    if gid not in TOOLCHAIN_LINKS:
        return
    name, target = TOOLCHAIN_LINKS[gid]
    link, src = dest_root / name, TOOLCHAINS / target
    if link.exists() or link.is_symlink():
        print("ok:   %s exists, left alone" % link)
    elif src.is_dir():
        link.symlink_to(src)
        print("ok:   %s -> %s" % (link, src))
    else:
        print("note: no toolchain at %s (toolchains/README.md)" % src)


# Generated tables: rewritten between <!-- NAME:begin --> and <!-- NAME:end -->.
NOTE = "<!-- Generated by tools/openthievius.py tables from games/*/game.json; do not edit. -->"


def table_discs(table):
    rows = ["| Game | Version | Serial | Role | Size (bytes) | CRC32 | MD5 | SHA-1 |", "|---|---|---|---|---:|---|---|---|"]
    for game in table.values():
        for v in game["versions"].values():
            d = v["disc"]
            rows.append("| %s | %s | `%s` | %s | %s | `%s` | `%s` | `%s` |" % (
                v["title"], v["label"], v["serial"], v["role"], format(d["size"], ","),
                d["crc32"], d["md5"], d["sha1"]))
    return rows


def table_boot(table):
    rows = ["| Game | Version | Boot executable | Size (bytes) | SHA-1 | SHA-256 |", "|---|---|---|---:|---|---|"]
    for game in table.values():
        for v in game["versions"].values():
            b = v["boot"]
            rows.append("| %s | %s | `%s` | %s | `%s` | `%s` |" % (
                v["title"], v["label"], b["name"], format(b["size"], ","), b["sha1"], b["sha256"]))
    return rows


def table_games(table):
    rows = ["| Game | Version | Project | Role | Decompiles |", "|---|---|---|---|---|"]
    for game in table.values():
        p = game["project"]
        for v in game["versions"].values():
            rows.append("| %s (%d) | %s `%s` | [%s](%s) | %s | %s |" % (
                game["title"], game["year"], v["label"], v["serial"], p["name"], p["repository"], v["role"],
                v.get("decompiles", "")))
    return rows


TABLES = {
    "baserom/README.md": {"discs": table_discs, "boot": table_boot},
    "games/README.md": {"games": table_games},
    "README.md": {"games": table_games},
}


def render(text, name, rows):
    begin, end = "<!-- %s:begin -->" % name, "<!-- %s:end -->" % name
    if begin not in text or end not in text:
        raise ValueError("no %s ... %s markers" % (begin, end))
    head, rest = text.split(begin, 1)
    _, tail = rest.split(end, 1)
    return head + begin + "\n" + NOTE + "\n" + "\n".join(rows) + "\n" + end + tail


def cmd_tables(args):
    table = games()
    stale = []
    for rel, makers in TABLES.items():
        path = ROOT / rel
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        new = text
        for name, make in makers.items():
            new = render(new, name, make(table))
        if new != text:
            stale.append(rel)
            if not args.check:
                path.write_text(new, encoding="utf-8")
    for rel in stale:
        print("%s %s" % ("stale:" if args.check else "wrote:", rel))
    return 1 if (args.check and stale) else 0


def cmd_check(args):
    errors = []
    for gid, game in games().items():
        root = project_dir(game)
        path = root / game["project"]["versions_file"]
        if not path.exists():
            print("skip: %s not checked out at %s" % (game["project"]["name"], root))
            continue
        with open(path, encoding="utf-8") as f:
            theirs = json.load(f)
        if theirs.get("primary") != game["primary"]:
            errors.append("%s: primary is %r here, %r in %s" % (gid, game["primary"], theirs.get("primary"), path))
        for key in sorted(set(theirs["versions"]) | set(game["versions"])):
            a, b = game["versions"].get(key), theirs["versions"].get(key)
            if a is None or b is None:
                errors.append("%s/%s: only in %s" % (gid, key, "the project" if a is None else "game.json"))
                continue
            for field in ("serial", "role", "boot", "disc"):
                if a.get(field) != b.get(field):
                    errors.append("%s/%s: %s differs from %s" % (gid, key, field, path))
    with open(SHARED, encoding="utf-8") as f:
        shared = json.load(f)
    for entry in shared["files"]:
        original = ROOT / entry["path"]
        for copy in entry["copies"]:
            gid, _, rel = copy.partition(":")
            target = project_dir(games()[gid]) / rel
            if target.exists() and target.read_bytes() != original.read_bytes():
                errors.append("%s and %s:%s differ (shared/files.json)" % (entry["path"], gid, rel))
    for e in errors:
        print("error: " + e)
    print("%s: %d problem(s)" % ("FAIL" if errors else "ok", len(errors)))
    return 1 if errors else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("discs", help="identify disc images (default: baserom/)")
    p.add_argument("paths", nargs="*")
    p.add_argument("--full", action="store_true", help="also check every disc checksum (reads the whole image)")
    p = sub.add_parser("setup", help="put each game's boot executable in its project, link its toolchain")
    p.add_argument("target", nargs="?", help="GAME or GAME/VERSION, e.g. sly1/us")
    p.add_argument("--discs", nargs="*", help="where the images are (default: baserom/)")
    p = sub.add_parser("tables", help="regenerate the README tables from games/*/game.json")
    p.add_argument("--check", action="store_true", help="fail if a table is stale, write nothing")
    sub.add_parser("check", help="game.json against each project's own table, and the shared files")
    args = parser.parse_args(argv)
    return {"discs": cmd_discs, "setup": cmd_setup, "tables": cmd_tables, "check": cmd_check}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
