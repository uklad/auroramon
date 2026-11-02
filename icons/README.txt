Aurora Monitor icon
===================

A sun above a solar panel.

This is original artwork made for this program, it isn't derived from any other
icon or image.  make_icon.py defines the shapes and generates the other files:
auroramon.svg, the auroramon_NN.png files and ../src/auroramon_icon.h.

Licence: GNU General Public License, version 3 or (at your option) any later
version, the same as the program.  See ../License.txt.

To change the icon, edit make_icon.py and run:  python3 make_icon.py

install-desktop.sh adds a launcher and the icon for the current user, which the
desktop needs in order to show the icon on Wayland:

    icons/install-desktop.sh [path of the auroramon program]
    icons/install-desktop.sh --uninstall
