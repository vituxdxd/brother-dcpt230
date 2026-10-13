#!/bin/bash
#
# Install the open DCP-T230 driver and create (or update) a CUPS queue.
#
# Usage:  sudo ./install.sh [device-uri]
#
# If no URI is given, the script looks for a queue already using this printer
# and otherwise asks the USB backend. Queue name: DCP-T230 (override with
# T230_QUEUE).
#
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }
DIR="$(dirname "$(readlink -f "$0")")"
QUEUE="${T230_QUEUE:-DCP-T230}"

[ -f "$DIR/ppd/brother-t230.ppd" ] || { echo "PPD missing; run make_ppd.py" >&2; exit 1; }
[ -x "$DIR/t230_encode" ] || { echo "t230_encode missing; run make" >&2; exit 1; }

echo "== installing files =="
install -d /usr/lib/brother-t230 /usr/share/brother-t230 /usr/share/ppd/Brother
install -m755 "$DIR/brother-t230" /usr/lib/cups/filter/brother-t230
install -m755 "$DIR/t230_encode" /usr/lib/brother-t230/t230_encode
install -m644 "$DIR/papers.tsv" /usr/share/brother-t230/papers.tsv
install -m644 "$DIR/ppd/brother-t230.ppd" /usr/share/ppd/Brother/brother-t230.ppd

echo "== locating the printer =="
URI="${1:-}"
if [ -z "$URI" ]; then
    URI="$(lpstat -v 2>/dev/null | grep -i 'DCP-T230' | head -1 | sed 's/^.*: //' || true)"
fi
if [ -z "$URI" ]; then
    URI="$(lpinfo -v 2>/dev/null | awk '/usb:\/\/Brother/ {print $2; exit}' || true)"
fi
if [ -z "$URI" ]; then
    echo "could not detect the printer; pass the device URI as an argument" >&2
    echo "example: sudo $0 'usb://Brother/DCP-T230?serial=XXXXXXXX'" >&2
    exit 1
fi
echo "URI: $URI"

echo "== configuring queue '$QUEUE' =="
if lpstat -p "$QUEUE" >/dev/null 2>&1; then
    lpadmin -p "$QUEUE" -E -v "$URI" -P /usr/share/ppd/Brother/brother-t230.ppd
else
    lpadmin -p "$QUEUE" -E -v "$URI" -P /usr/share/ppd/Brother/brother-t230.ppd \
        -o printer-is-shared=false
fi

systemctl restart cups
sleep 1

echo
echo "Done. Queue '$QUEUE' uses the open driver."
lpstat -p "$QUEUE" -v
