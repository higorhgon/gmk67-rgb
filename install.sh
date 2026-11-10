#!/bin/sh
# Install from source for the current user: the app in ~/.local
# and the udev rule on the system (asks for sudo). To uninstall: ./install.sh --uninstall
set -e
cd "$(dirname "$(realpath "$0")")"

if [ "$1" = "--uninstall" ]; then
    make uninstall PREFIX="$HOME/.local" UDEVDIR=
    sudo rm -f /etc/udev/rules.d/70-gmk67.rules
    exit
fi

sudo install -m 644 70-gmk67.rules /etc/udev/rules.d/70-gmk67.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=hidraw

make install PREFIX="$HOME/.local" UDEVDIR=
# Launcher from older versions (before the app ID change)
rm -f ~/.local/share/applications/dev.aloran.Gmk67Rgb.desktop
echo "Done. Open \"GMK67\" from the menu or run gmk67-rgb"
