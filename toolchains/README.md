# toolchains: compilers you supply

The PS2 compilers, assemblers and linkers are proprietary. They are never
committed (this directory is ignored by git, apart from this file).

## Sly Cooper (`sly1`)

The game is C++ built with Sony's GNU EE GCC 2.9-ee-991111b. Its Windows build
is in a public community mirror:

```sh
git clone https://github.com/AngheloAlf/sce_ps2_sdk_24 toolchains/sn-prodg-24
python3 tools/openthievius.py setup sly1   # links sly1-decomp/toolchain to toolchains/sn-prodg-24/local/sce/ee/gcc
```

The compiler is `local/sce/ee/gcc/lib/gcc-lib/ee/2.9-ee-991111/cc1plus.exe`
(`GNU C++ version 2.9-ee-991111b/r4`). On macOS and Linux it runs through
[wibo](https://github.com/decompals/wibo) or Wine. If you have an OpenRAC
checkout, its `toolchains/sn-prodg-24` is the same mirror: link it here
instead of cloning again.

What is known about the compiler, and what is still to qualify, is in
[sly1-decomp's docs/TOOLCHAIN.md](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/TOOLCHAIN.md).
