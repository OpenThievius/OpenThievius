#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Embed the two OpenRAC direct-renderer GLSL programs in a C++ translation unit."""

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("vertex", type=Path)
    parser.add_argument("fragment", type=Path)
    parser.add_argument("upscaled", type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    shaders = {
        "direct_vert": args.vertex.read_text(encoding="utf-8"),
        "direct_frag": args.fragment.read_text(encoding="utf-8"),
        "upscaled_texture_colours_glsl": args.upscaled.read_text(encoding="utf-8"),
    }
    header = args.out / "renderer_shaders.h"
    source = args.out / "renderer_shaders.cpp"
    header.write_text(
        "// SPDX-License-Identifier: GPL-3.0-or-later\n"
        "#pragma once\n\n"
        "namespace openrac::renderer::shaders {\n"
        "extern const char* const direct_vert;\n"
        "extern const char* const direct_frag;\n"
        "extern const char* const upscaled_texture_colours_glsl;\n"
        "}\n",
        encoding="utf-8",
    )
    definitions = "\n".join(
        f"const char* const {name} = {json.dumps(text)};" for name, text in shaders.items()
    )
    source.write_text(
        "// SPDX-License-Identifier: GPL-3.0-or-later\n"
        '#include "renderer_shaders.h"\n\n'
        "namespace openrac::renderer::shaders {\n"
        f"{definitions}\n"
        "}\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
