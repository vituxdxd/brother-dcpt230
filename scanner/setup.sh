#!/bin/bash
#
# Scanner setup for the Brother DCP-T230 on Arch Linux.
#
# Installs SANE with Brother's brscan5 backend and a udev rule that gives
# regular users access to the scanner's USB interface. Run as your normal
# user, not root. sudo is requested when needed.
#
set -euo pipefail

echo "== 1. packages (sane + brscan5) =="
if ! command -v scanimage >/dev/null 2>&1 || ! pacman -Qq brscan5 >/dev/null 2>&1; then
    yay -S --needed --noconfirm sane libusb-compat brscan5
else
    echo "(already installed)"
fi

echo "== 2. udev rule for USB access =="
sudo tee /etc/udev/rules.d/99-brother-dcp-t230-scanner.rules >/dev/null <<'RULE'
# Brother DCP-T230: allow user access to the scanner interface
SUBSYSTEM=="usb", ATTR{idVendor}=="04f9", ATTR{idProduct}=="0716", MODE="0666"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb

echo "== 3. enable the brother5 backend =="
if ! grep -q '^brother5' /etc/sane.d/dll.conf 2>/dev/null; then
    echo "brother5" | sudo tee -a /etc/sane.d/dll.conf >/dev/null
    echo "(added)"
else
    echo "(already present)"
fi

echo "== 4. test =="
sleep 2
if scanimage -L; then
    echo
    echo "Scanner detected. Example scan:"
    echo "  scanimage --format=png --resolution 300 > /tmp/scan.png"
else
    echo
    echo "WARNING: scanner not listed. Check the USB cable and 'lsusb | grep 04f9'."
fi
