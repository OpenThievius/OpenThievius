<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 disc assets

Sly's ISO filesystem contains boot files and IOP modules. Most game media is
stored in raw sectors after the filesystem; the retail loader's index and the
level/model formats are still under investigation. See the decompilation's
[disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md).

## Recover standalone audio samples

The raw data area contains sector-aligned `VAGp` samples. The public VAG
header has a 48-byte header with big-endian version, data length and sample
rate fields; its ADPCM payload follows the header. The extractor checks these
fields and requires the payload length to be a multiple of 16 bytes, as
expected for PS-ADPCM blocks. See the [VAG format reference](https://psx-spx.consoledev.net/cdromfileformats/).

Run the scanner against a disc image you own:

```sh
python3 port/tools/extract_sly1_vag.py /path/to/Sly1.iso
```

It accepts cooked 2048-byte ISO images and raw 2352-byte BIN images. By
default it scans from logical sector 12345 and writes the VAG files and
`manifest.tsv` to `build/port/extracted/vag/`. Filenames use sector numbers
because this pass does not recover the game's resource names or index.
Outputs stay in the ignored build tree; no disc data or extracted media is
committed.

On the locally supplied NTSC-U image, the scanner found 1,409 VAGp samples
with 215,281,856 bytes of declared ADPCM payload. This is a signature-based
audio recovery path; it does not locate or extract the level/model data needed
to render and play a level.
