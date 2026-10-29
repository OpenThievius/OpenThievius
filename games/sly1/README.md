# Sly Cooper and the Thievius Raccoonus (2002)

Decompiled in [sly1-decomp](https://github.com/OpenThievius/sly1-decomp), a
sister checkout beside OpenThievius (`../sly1-decomp`, or
`OPENTHIEVIUS_SOURCE_SLY1`).

| Version | Serial | Role |
|---|---|---|
| NTSC-U v1.00 | `SCUS_971.98` | primary target |
| PAL v1.00, *Sly Raccoon* (En, Fr, De, Es, It) | `SCES_509.17` | second target |
| E3 build 5-19D, 2002-05-19 | `SCUS_971.98` | recognised and skipped: nothing is taken from it |

[game.json](game.json) has every checksum. To set up the decompilation from
your discs:

```sh
python3 tools/openthievius.py setup sly1 --discs ~/path/to/discs
```
