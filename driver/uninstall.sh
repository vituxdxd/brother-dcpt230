#!/bin/bash
#
# Remove the open DCP-T230 driver and its CUPS queue.
#
# Usage:  sudo ./uninstall.sh
#
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "run with sudo: sudo $0" >&2; exit 1; }
QUEUE="${T230_QUEUE:-DCP-T230}"

echo "== removing queue '$QUEUE' =="
lpadmin -x "$QUEUE" 2>/dev/null || echo "(queue not found)"

echo "== removing files =="
rm -f /usr/lib/cups/filter/brother-t230
rm -rf /usr/lib/brother-t230
rm -rf /usr/share/brother-t230
rm -f /usr/share/ppd/Brother/brother-t230.ppd

systemctl restart cups

echo "Done."
