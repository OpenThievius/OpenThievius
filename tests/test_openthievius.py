# SPDX-License-Identifier: AGPL-3.0-or-later
"""Tests for tools/openthievius.py. They need no disc and no sister checkout."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import disc  # noqa: E402
import openthievius  # noqa: E402
import proprietary  # noqa: E402


class GamesTest(unittest.TestCase):
    def test_game_json(self):
        table = openthievius.games()
        self.assertIn("sly1", table)
        for gid, game in table.items():
            self.assertIn(game["primary"], game["versions"])
            for field in ("name", "repository", "dir", "env", "versions_file", "license"):
                self.assertIn(field, game["project"])
            self.assertEqual(game["project"]["license"], "AGPL-3.0-or-later")
            for key, v in game["versions"].items():
                for field in ("label", "title", "serial", "role", "boot", "disc", "baserom"):
                    self.assertIn(field, v, "%s/%s" % (gid, key))
                self.assertRegex(v["boot"]["sha1"], "^[0-9a-f]{40}$")
                self.assertTrue(v["baserom"].startswith("baserom/%s/" % key))

    def test_tables_render_between_markers(self):
        text = "a\n<!-- t:begin -->\nold\n<!-- t:end -->\nb\n"
        out = openthievius.render(text, "t", ["| x |"])
        self.assertEqual(out, "a\n<!-- t:begin -->\n%s\n| x |\n<!-- t:end -->\nb\n" % openthievius.NOTE)
        self.assertEqual(openthievius.render(out, "t", ["| x |"]), out)
        with self.assertRaises(ValueError):
            openthievius.render("no markers", "t", [])

    def test_images_skip_small_files_and_checkouts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "small.bin").write_bytes(b"x" * 100)
            (root / "disc.iso").write_bytes(bytes(17 * disc.SECTOR))
            (root / "repo" / ".git").mkdir(parents=True)
            (root / "repo" / "inner.iso").write_bytes(bytes(17 * disc.SECTOR))
            (root / "folder").mkdir()
            (root / "folder" / "dump.img").write_bytes(bytes(17 * disc.SECTOR))
            found = sorted(p.name for p in openthievius.images([root]))
            self.assertEqual(found, ["disc.iso", "dump.img"])

    def test_unknown_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "blank.iso"
            path.write_bytes(bytes(17 * disc.SECTOR))
            gid, key, reason = openthievius.identify(path, openthievius.games())
            self.assertIsNone(gid)
            self.assertIn("no ISO 9660", reason)


class ProprietaryTest(unittest.TestCase):
    def test_rejects_proprietary_material(self):
        self.assertEqual(proprietary.check_file("tools/x.py", b"print(1)\n"), [])
        self.assertTrue(proprietary.check_file("toolchains/sn-prodg-24/ee-gcc.exe", b"MZ"))
        self.assertTrue(proprietary.check_file("port/gs.cpp", b"#include <libgraph.h>\n"))
        self.assertTrue(proprietary.check_file("docs/f.md", b"/* 1008 00100008 27BDFFF0 " + b"*/\n"))

    def test_shared_copy_is_identical(self):
        theirs = Path(openthievius.project_dir(openthievius.games()["sly1"])) / "tools" / "proprietary.py"
        if not theirs.exists():
            self.skipTest("sly1-decomp is not checked out")
        self.assertEqual(theirs.read_bytes(), (Path(openthievius.HERE) / "proprietary.py").read_bytes())


if __name__ == "__main__":
    unittest.main()
