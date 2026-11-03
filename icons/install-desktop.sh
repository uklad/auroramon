#!/bin/sh
# Add (or with --uninstall, remove) the Aurora Monitor launcher and icon for the current user.
# usage: install-desktop.sh [path of the auroramon program]
set -e

here=$(cd "$(dirname "$0")" && pwd)
data=${XDG_DATA_HOME:-$HOME/.local/share}
desktop=$data/applications/auroramon.desktop

if [ "$1" = "--uninstall" ]; then
    rm -f "$desktop" "$data/icons/hicolor/scalable/apps/auroramon.svg"
    for size in 16 32 48 64 128 256; do
        rm -f "$data/icons/hicolor/${size}x${size}/apps/auroramon.png"
    done
    echo "Removed the Aurora Monitor launcher and icon"
    exit 0
fi

program=${1:-$here/../src/auroramon}
if [ ! -x "$program" ]; then
    echo "Can't find the program: $program" >&2
    exit 1
fi
program=$(cd "$(dirname "$program")" && pwd)/$(basename "$program")

for size in 16 32 48 64 128 256; do
    mkdir -p "$data/icons/hicolor/${size}x${size}/apps"
    cp "$here/auroramon_$size.png" "$data/icons/hicolor/${size}x${size}/apps/auroramon.png"
done
mkdir -p "$data/icons/hicolor/scalable/apps" "$data/applications"
cp "$here/auroramon.svg" "$data/icons/hicolor/scalable/apps/auroramon.svg"

cat > "$desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Aurora Monitor
Comment=Monitor Aurora solar power inverters
Exec="$program"
Icon=auroramon
Terminal=false
Categories=Utility;Monitor;
StartupWMClass=auroramon
EOF

echo "Installed: $desktop"
