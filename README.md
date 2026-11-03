# Aurora Monitor

A wxWidgets program which monitors Aurora solar power inverters through a serial connection, logging and charting their output.

This is a continuation of [Aurora Monitor](https://sourceforge.net/projects/auroramonitor/) by Jonathan Duddington, which is no longer being developed. Version 1.07 is the first commit here, so `git diff` against it shows every later change.

It's intended for domestic installations rather than commercial ones, so it only supports up to 2 inverters.

It has been updated to support current wxWidgets (3.1.5 or later), as wxWidgets 2.8, which version 1.07 used, is no longer available on newer Linux distributions. These are test builds, so they don't have a new version number yet.

## Building on Linux

You need wxWidgets 3.1.5 or later. Run `wx-config --version` to check what you have. The package below is for Debian and Ubuntu. It may have a different name on other distributions, and Ubuntu 22.04's default wxWidgets is too old.

```sh
sudo apt install build-essential git libwxgtk3.2-dev
git clone https://github.com/uklad/auroramon.git
cd auroramon/src
make -j"$(nproc)"
./auroramon
```

These steps haven't been tested on a clean machine.

### Setting up after building

Choose your data directory in Settings → Setup, then do any of these that you need. The commands run from `src`, and the example data directory is `~/aurora`.

- **Help:** copy the guide into the data directory's `system` folder:

  ```sh
  mkdir -p ~/aurora/system
  cp ../help.html ~/aurora/system/
  ```

- **Launcher and icon:** add them to your desktop's applications menu. Use `--uninstall` instead of the path to remove them.

  ```sh
  ../icons/install-desktop.sh "$PWD/auroramon"
  ```

- **Serial port:** if the inverter connects to this computer, you'll probably need access to the port. Log out and back in afterwards.

  ```sh
  sudo usermod -aG dialout "$USER"
  ```

### Moving from another computer

Copy `~/.aurora` and your data directory from the old computer. `~/.aurora` contains your PVOutput API key, so transfer it securely. If your username is different, update the `data=` path in it.

### Updating

Run `git pull` in the repository, then `make` again in `src`. If `help.html` has changed, copy it to the data directory again.

## Building on Windows

You don't need to build it yourself. Every push to GitHub builds a Windows version, see [windows-build.yml](.github/workflows/windows-build.yml). Download the zip file from the latest run's Artifacts on the [Actions page](https://github.com/uklad/auroramon/actions), unzip it and run `auroramon.exe`. GitHub asks you to log in to download Artifacts, and it deletes them after 90 days unless the repository is set to keep them for less.

To build it yourself, in the MSYS2 UCRT64 shell:

```sh
pacman -S make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-wxwidgets3.2-msw
cd src
make WX_CONFIG=wx-config-3.2
```

## PVOutput

To upload to [PVOutput](https://pvoutput.org), enter your System Id and API Key in Settings → PVOutput.org, and choose how often to send updates.

- **Test connection** checks the settings in the dialog with PVOutput's read-only Get System service, because PVOutput has no test mode. It sends no readings, but it counts towards PVOutput's hourly request limit. A read-only API key passes this test and then fails when uploading.
- **The line at the bottom of the window** shows the most recent result and how many readings are waiting, such as `PVOutput: failed at 08:25, 1 waiting`.
- **If an upload fails,** the reading is saved in `system/pvoutput_fail.txt` in the data directory. The program tries to send saved readings about every 2 minutes, up to 30 at a time, and discards any more than 13 days old.

These have been tested against a local server, not PVOutput itself.

## Testing without an inverter

`tests/fake_inverter.py` simulates inverters on a pseudo-terminal. Run it with `--help` for its options. The test builds have only been tested this way on Linux, on X11 and KDE Wayland, and not with a real inverter. On Windows the serial port hasn't been tested at all.

## More information

- [README.txt](README.txt): changes in each version
- [help.html](help.html): how to use the program
- [docs/inverter-clock.md](docs/inverter-clock.md): how the inverter's clock is updated, including daylight saving

## Licence

GPL version 3 or later, see [License.txt](License.txt). The icon in `icons/` is original artwork under the same licence.
