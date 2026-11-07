<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Retained port C references

This directory holds port-only C reconstructions that have been replaced in
the active launcher by exact US retail assembly. Hostgen does not copy this
directory into its generated sources. Keep these files for comparison and
remove them only when they are no longer useful; they are not matching
decompilation sources.

`func_001E9108.c` is the earlier frame-state dispatcher reconstruction. The
launcher now executes the translation of `asm/us/text.s` for that address.

`func_001662A8.cpp` is an earlier scene-packet C prototype. The launcher now
uses the US retail assembly translation at the same address; this C prototype
is no longer compiled into the runtime.
