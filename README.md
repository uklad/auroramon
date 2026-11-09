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

Copy `~/.aurora` and your data directory from the old computer. `~/.aurora` contains your PVOutput API key and MQTT password, so transfer it securely. If your username is different, update the `data=` path in it.

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

## MQTT and Home Assistant

The program can publish the readings to an MQTT broker, such as Home Assistant's Mosquitto broker add-on. It uses Home Assistant's MQTT discovery, so each inverter appears as a device with its sensors, without any configuration in Home Assistant. Its MQTT integration must already be set up.

1. Open Settings → MQTT (Home Assistant).
2. Enter the broker's address and port, usually 1883. For the Mosquitto add-on, the address is Home Assistant's.
3. Enter a user name and password that the broker accepts, or leave them blank if it allows anonymous connections.
4. Press **Test connection**, then tick **Publish the readings to an MQTT broker** and press **OK**.

- **In Home Assistant,** the devices are called `Aurora inverter 2`, with the inverter's address. Their sensors are the power, each input's voltage, current and power, efficiency, temperature, peak power today, and the energy totals. The readings chosen in Settings → Extra readings are added too. With two inverters there's also an `Aurora Monitor total` device.
- **Availability:** an inverter shows as unavailable while it isn't responding, such as at night. Everything shows as unavailable when the program isn't running or has lost the broker.
- **The line at the bottom of the window** shows `MQTT: connected`, `connecting`, `off`, or `failed at` a time. The reason for a failure is in the message log. The program tries again after 5 seconds, then less often, up to every 2 minutes.
- **The connection isn't encrypted,** so use a broker on your own network. The password is saved in `~/.aurora`, which only you can read.

The topics, with the default topic prefix `auroramon` and the inverter's address as `N`, are all retained:

| Topic | Contents |
| --- | --- |
| `auroramon/status` | `online` or `offline`, also the last will |
| `auroramon/inverterN/state` | the inverter's readings, as JSON |
| `auroramon/inverterN/availability` | `online` or `offline`, whether the inverter is responding |
| `auroramon/total/state` | the totals, when there are two inverters |

The discovery configs are under `homeassistant/sensor/`, unless the discovery prefix is changed.

## Testing without an inverter

`tests/fake_inverter.py` simulates inverters on a pseudo-terminal. Run it with `--help` for its options.

## More information

- [README.txt](README.txt): changes in each version
- [help.html](help.html): how to use the program
- [docs/inverter-clock.md](docs/inverter-clock.md): how the inverter's clock is updated, including daylight saving

## Licence

GPL version 3 or later, see [License.txt](License.txt). The icon in `icons/` is original artwork under the same licence.
