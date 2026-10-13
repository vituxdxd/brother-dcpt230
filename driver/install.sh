#!/bin/bash
#
# Install the open DCP-T230 driver and create (or update) a CUPS queue.
#
# Usage:  sudo ./install.sh [device-uri]
#
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }
DIR="$(dirname "$(readlink -f "$0")")"

[ -f "$DIR/ppd/brother-t230.ppd" ] || { echo "PPD missing; run make_ppd.py" >&2; exit 1; }
[ -x "$DIR/t230_encode" ] || { echo "t230_encode missing; run make" >&2; exit 1; }

echo "== installing files =="
install -d /usr/lib/brother-t230 /usr/share/brother-t230 /usr/share/ppd/Brother
install -m755 "$DIR/brother-t230" /usr/lib/cups/filter/brother-t230
install -m755 "$DIR/t230_encode" /usr/lib/brother-t230/t230_encode
install -m644 "$DIR/papers.tsv" /usr/share/brother-t230/papers.tsv
install -m644 "$DIR/ppd/brother-t230.ppd" /usr/share/ppd/Brother/brother-t230.ppd
install -m755 "$DIR/dcpt230-setup" /usr/bin/dcpt230-setup

echo "== configuring the CUPS queue =="
exec /usr/bin/dcpt230-setup "$@"
