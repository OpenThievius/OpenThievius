# SPDX-License-Identifier: AGPL-3.0-or-later
"""The pre-commit check: no game data or proprietary material, licence headers, tables current.

    python3 tests/check_repository.py

CI runs it on every push. It looks at what git would commit (tracked files and
untracked ones that are not ignored), and needs no disc and no compiler.

1. tools/proprietary.py: no binaries, disc images, game data, toolchains,
   Sony SDK headers or banners, or dumps of retail data.
2. Project source uses AGPL; credited OpenRAC components keep their upstream
   GPL-3.0-or-later notices.
3. LICENSE is the GNU Affero General Public License.
4. The README tables are current (tools/openthievius.py tables --check).
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import proprietary  # noqa: E402

SPDX = "SPDX-License-Identifier: AGPL-3.0-or-later"
OPENRAC_SPDX = "SPDX-License-Identifier: GPL-3.0-or-later"
NEEDS_SPDX = {".py", ".sh", ".c", ".cpp", ".h", ".hpp", ".s", ".inc", ".ld"}
LOCAL_RUNTIME = {
    "port/runtime/disc_image.cpp", "port/runtime/dma.cpp", "port/runtime/sif.cpp",
    "port/runtime/include/openrac/disc_image.h",
    "port/runtime/include/openrac/native_dma.h",
    "port/runtime/include/openrac/native_sif.h",
}


def main():
    errors = proprietary.scan(ROOT)
    for rel in proprietary.tracked(ROOT):
        path = ROOT / rel
        if path.is_file() and path.suffix in NEEDS_SPDX:
            header = path.read_text(errors="replace")[:400]
            imported = rel.startswith(("port/tools/hostgen/", "tools/openrac_common/",
                                       "port/common/", "port/runtime/")) and rel not in LOCAL_RUNTIME
            expected = OPENRAC_SPDX if imported else SPDX
            if expected not in header:
                errors.append("%s: no '%s' line at the top" % (rel, expected))
    if "GNU AFFERO GENERAL PUBLIC LICENSE" not in (ROOT / "LICENSE").read_text()[:200]:
        errors.append("LICENSE: not the GNU Affero General Public License")
    tables = subprocess.run([sys.executable, str(ROOT / "tools" / "openthievius.py"), "tables", "--check"],
                            capture_output=True, text=True)
    if tables.returncode:
        errors.append("README tables are stale; run python3 tools/openthievius.py tables\n" + tables.stdout)
    for e in errors:
        print("error: " + e)
    print("%s: %d problem(s)" % ("FAIL" if errors else "ok", len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
