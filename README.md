# Brother DCP-T230 driver for Arch Linux

An open CUPS driver for the Brother DCP-T230 inkjet printer. It replaces the
vendor's Linux driver with a small C encoder and a shell wrapper, and prints at
600 dpi where the vendor driver stays at 300 dpi.

The scanner works through SANE and Brother's `brscan5` backend. See
[Scanner setup](#scanner-setup).

## Why this exists

Brother publishes Linux drivers as .deb and .rpm packages only. On Arch, the
usual route is the AUR package `brother-dcp-t230`, which repacks the Debian
driver: two Perl scripts, a closed-source filter binary and a configuration
tool. That driver rasterises every job at 300 dpi regardless of the quality
setting, and in testing some jobs failed with `No pages found!`.

This project reimplements the print stream. The output is the same wire format
the vendor driver produces, verified byte for byte for the same page and
settings, with one difference: Normal and Fine quality render at 600 dpi.

## Requirements

- Arch Linux (the install scripts assume pacman, the driver itself is portable)
- `cups`, `cups-filters`, `ghostscript`

## Install

```bash
git clone https://github.com/vituxdxd/brother-dcpt230.git
cd brother-dcpt230/driver
make
sudo ./install.sh
```

`install.sh` copies the filter, the encoder and the PPD, then creates the CUPS
queue `DCP-T230`. It auto-detects the USB device URI. Pass it as an argument if
detection fails.

Print a test page:

```bash
lp -d DCP-T230 -t test /usr/share/cups/data/testprint
```

If you already have the vendor package installed, remove it afterwards:

```bash
sudo pacman -Rns brother-dcp-t230
```

## Options

| PPD option | Values | Effect |
|---|---|---|
| `BRResolution` | Draft, Normal, Fine | Raster resolution: 300 dpi for Draft, 600 dpi for Normal and Fine |
| `BRMonoColor` | Auto, FullColor, Mono | Mono converts to grayscale before sending |
| `BRMediaType` | Plain, Thick, Thicker, BOND, Recycled, Env, EnvThin, EnvThick, Label, PostCard, Inkjet, Glossy | Paper type reported to the printer |
| `BRInputSlot` | AutoSelect, Tray1, Manual | Manual feed disables the tray setting |
| `BRDuplex` | None, DuplexTumble, DuplexNoTumble | Manual duplex hints, the printer is single-sided |

The resolution can also be overridden with the environment variables
`T230_DRAFT_DPI`, `T230_NORMAL_DPI` and `T230_FINE_DPI` when running the filter
by hand.

## Scanner setup

The printer's scanner is a separate USB interface and does not need this
driver. Brother's `brscan5` backend plus SANE handles it:

```bash
cd ../scanner
./setup.sh
```

The script installs `sane`, `libusb-compat` and `brscan5`, adds a udev rule so
regular users can reach the scanner, and runs `scanimage -L`. A test scan:

```bash
scanimage --format=png --resolution 300 > /tmp/scan.png
```

## Troubleshooting

- **Nothing prints, jobs stay queued.** Check `lpstat -p DCP-T230 -v` and the
  CUPS log at `/var/log/cups/error_log`.
- **USB disconnects during a job.** Connect the printer to a port on the
  computer itself. Bus-powered hubs, especially full-speed ones, cause
  reconnects that the kernel logs as `usblp0: removed`.
- **`unsupported PageSize` in the CUPS log.** The PPD and `papers.tsv` list 27
  paper sizes. Custom sizes are not supported yet.
- **Wrong colors or grayscale output.** Check `BRMonoColor`. `Mono` prints in
  grayscale, `Auto` and `FullColor` in color.

## Limitations

- Custom page sizes are not implemented.
- Only the DCP-T230 was tested. Brother's driver package covers more "23B"
  models (DCP-T236, DCP-T238, DCP-T430W, DCP-T530DW, MFC-T580DW and others);
  they use the same protocol but need their own paper tables and a test run.
- The scanner still uses Brother's proprietary `brscan5` backend.
- Copies are produced by CUPS, not by the printer.

## How it works

The printer accepts a PJL preamble followed by a PWG-raster variant with
run-length coded lines. `driver/t230_encode.c` builds that stream from PPM
images produced by Ghostscript. `driver/brother-t230` maps CUPS options to the
PJL settings and drives the pipeline. The format is documented in
[docs/PROTOCOL.md](docs/PROTOCOL.md).

## Legal

This is an independent implementation for interoperability. It contains no
vendor code or binaries. Brother and DCP-T230 are trademarks of Brother
Industries, Ltd. This project is not affiliated with Brother.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
