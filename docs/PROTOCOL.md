# DCP-T230 print stream format

This document describes the byte stream the printer consumes. It was derived
from the data the printer accepts; the implementation in `driver/` is
independent and contains no vendor code.

## Overview

A job is a PJL preamble followed by raster pages. Each page carries a
1796-byte header, then one block per scanline. Scanlines are run-length coded.
The stream ends with a PJL reset sequence.

```
ESC%-12345X@PJL ...        job header (text)
@PJL ENTER LANGUAGE=PWGRASTER
RaS2                        magic, once per job
┌ page 1 ──────────────────────────────────────────────┐
│ <1796-byte page header>                              │
│ [rc][RLE scanline] [rc][RLE scanline] ...            │
└──────────────────────────────────────────────────────┘
┌ page 2 (no PJL, no RaS2) ────────────────────────────┐
│ <1796-byte page header>                              │
│ ...                                                  │
└──────────────────────────────────────────────────────┘
ESC%-12345X [@PJL EOJ NAME="..."] ESC%-12345X
```

## Job header (PJL)

One text block at the start of the job. Lines used by this driver:

```
ESC%-12345X@PJL
@PJL SET HOLD=OFF
@PJL SET USERNAME="<owner>"        (optional)
@PJL SET JOBNAME="<title>"         (optional)
@PJL SET LOGINUSER="<owner>"       (optional)
@PJL JOB NAME="<title>"            (optional)
@PJL SET STRINGCODESET=UTF8
@PJL SET PAPER=<name>
@PJL SET BORDERLESS=ON|OFF
@PJL SET JTTOPMARGIN=<0|300|1200>
@PJL SET JTBOTMARGIN=<0|300|1200>
@PJL SET JTLEFTMARGIN=<0|300>
@PJL SET JTRIGHTMARGIN=<0|300>
@PJL SET RENDERMODE=COLOR|GRAYSCALE
@PJL SET PRINTQUALITY=DRAFT|NORMAL|HIGH
@PJL SET DUPLEX=ON|OFF
@PJL SET MEDIATYPE=<REGULAR|THICK|THICK2|BOND|RECYCLED|ENVELOPES|ENVTHIN|ENVTHICK|LABEL|POSTCARD|INKJET|GLOSSY>
@PJL SET MANUALFEED=OFF|ON
@PJL SET SOURCETRAY=<AUTO|TRAY1|TRAY2|MPTRAY>   (omitted for manual feed)
@PJL JOBSETTINGLOG DRIVER=<38 hex chars>
@PJL SET FIDELITY=TRUE
@PJL ENTER LANGUAGE=PWGRASTER
RaS2
```

`JTTOPMARGIN`/`JTBOTMARGIN` are 300 for normal paper, 1200 for envelopes and
small photo sizes, and 0 for the borderless "B" sizes. Left and right margins
are 300, or 0 when borderless.

`JOBSETTINGLOG` is a fixed-width hex string:

```
I + model(7) + "11" + media(8) + quality(4) + paper(4) + color(4) + duplex(4) + slot(4)
```

- quality: 1 Draft, 2 Normal, 3 High
- color: 1 grayscale, 2 color
- duplex: 0 off, 0x0100 long-edge, 0x0102 short-edge
- slot: 1 Tray1, 2 Tray2, 3 MP Tray, 4 Manual, 7 Auto
- media: 1 plain/thick, 2 thicker, 5 bond, 6 envelopes, 7 env. thick,
  8 env. thin, 9 recycled, 0x0A postcard, 0x0B label, 0x0105 inkjet,
  0x0107 glossy

The model field for the DCP-T230 is `0004036`. The paper field is the paper ID
from `papers.tsv` (`paper_id` column).

## Page header

1796 bytes, big-endian 32-bit fields at fixed offsets. All other bytes are
zero.

| Offset | Size | Value |
|---|---|---|
| 0x000 | 9 | `PwgRaster` |
| 0x080 | 4 | `auto` |
| 0x114 | 4 | horizontal resolution (300 or 600) |
| 0x118 | 4 | vertical resolution (300 or 600) |
| 0x160 | 4 | paper width in points |
| 0x164 | 4 | paper height in points |
| 0x174 | 4 | raster width in pixels |
| 0x178 | 4 | raster height in pixels |
| 0x180 | 4 | bits per channel (8) |
| 0x184 | 4 | bits per pixel (24 color, 8 grayscale) |
| 0x188 | 4 | bytes per scanline (3 × width, or width) |
| 0x190 | 4 | 19 color, 18 grayscale |
| 0x1a4 | 4 | 3 color, 1 grayscale |

The header is repeated for every page.

## Scanline blocks

Each block is one byte of repeat count followed by the encoded scanline:

- `rc` is the number of **additional** identical lines. The encoder emits 0
  for a single line or 1 when the next line is byte-identical, which is the
  only repeat value used in practice. Blocks are capped at two lines.
- The encoded scanline always expands to exactly `raster width` pixels, each
  pixel 3 bytes (RGB) or 1 byte (grayscale).

Run-length codes:

| Code | Meaning |
|---|---|
| `0x00` | one literal pixel |
| `0x01`–`0x7F` | repeat the next pixel `code + 1` times (2–128) |
| `0x81`–`0xFF` | `0x101 - code` literal pixels (2–128) |

`0x80` is never emitted.

The encoder picks a repeat run when two consecutive pixels are equal, and a
literal run otherwise, with runs capped at 128 pixels.

## Grayscale

For `BRMonoColor=Mono`, each RGB pixel is converted to one byte:

```
gray = 0.3 * R + 0.59 * G + 0.11 * B
```

The header then reports 8 bits per pixel and one byte per scanline.

## Trailer

After the last page:

```
ESC%-12345X
@PJL EOJ NAME="<title>"     (only when a title is known)
ESC%-12345X
```

## Paper data

`driver/papers.tsv` maps each PPD page size to the values above:

| Column | Meaning |
|---|---|
| `ppd` | PageSize name in the PPD |
| `rc` | internal paper name |
| `ptw`, `pth` | paper size in points |
| `pxw`, `pxh` | raster size in pixels at 300 dpi (double for 600 dpi) |
| `pjl` | `PAPER=` value |
| `borderless` | `ON`/`OFF` |
| `top` … `right` | `JT*MARGIN` values |
| `iml` … `imb` | image box used for the Ghostscript preamble |
| `paper_id` | value used in `JOBSETTINGLOG` |

## Resolution

The vendor's Linux driver always sends 300 dpi raster. This driver sends
600 dpi for Normal and Fine quality: the raster doubles in size, the resolution
fields and pixel dimensions in the page header follow, and the printer accepts
it without any other change.
