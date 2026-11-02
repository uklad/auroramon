# Aurora Monitor

A wxWidgets program which monitors Aurora solar power inverters through a serial connection, logging and charting their output.

This is a continuation of [Aurora Monitor](https://sourceforge.net/projects/auroramonitor/) by Jonathan Duddington, which is no longer being developed. Version 1.07 is the first commit here, so `git diff` against it shows every later change.

## Building on Linux

Version 1.08 needs wxWidgets 3.1.5 or later.

```sh
sudo apt install build-essential libwxgtk3.2-dev
cd src
make
./auroramon
```

To add a launcher and icon to your desktop's applications menu, run `icons/install-desktop.sh path/to/auroramon`. Add `--uninstall` instead of the path to remove them.

## Building on Windows

Every push to GitHub builds a Windows version, see [windows-build.yml](.github/workflows/windows-build.yml). Download the zip file from the run's Artifacts on the Actions page.

To build it yourself, in the MSYS2 UCRT64 shell:

```sh
pacman -S make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-wxwidgets3.2-msw
cd src
make WX_CONFIG=wx-config-3.2
```

## Testing without an inverter

`tests/fake_inverter.py` simulates inverters on a pseudo-terminal. Run it with `--help` for its options. Version 1.08 has only been tested this way on Linux, and not with a real inverter. On Windows the serial port hasn't been tested at all.

## More information

- [README.txt](README.txt): changes in each version
- [help.html](help.html): how to use the program

## Licence

GPL version 3 or later, see [License.txt](License.txt). The icon in `icons/` is original artwork under the same licence.
