# skylanders-gamepad-daemon
Userspace daemon providing support for the Skylanders Trap Team mobile gamepad controller (BLE) on Linux through uinput.

## Installation
### From a package manager
At the moment, we only have an AUR package, [skylanders-gamepad-daemon-git](https://aur.archlinux.org/packages/skylanders-gamepad-daemon-git). You can install this manually or from an AUR helper, for example `yay`:
```
$ yay -S skylanders-gamepad-daemon-git
```
### From source
1. Run `make` at the project root
2. Run `make PREFIX=/usr/local install` as `root` (i.e with `sudo`).

## Usage
First, ensure that the systemd unit is started with the following command:
```
# systemctl enable --now skylanders-gamepad-daemon.service
```
Then, when the device is connected over Bluetooth, after a short wait a new virtual input device should be created that works with all programs. Note that the "pause" button on the controller is bound to `START` and that there are no stick buttons on the controller.

## Troubleshooting
If you are unable to connect the controller through a graphical interface (i.e `bluedevil` from KDE Plasma), try connecting through the command line via `bluetoothctl`.

First, enter the interactive shell (scanning for devices may not work as intended if running commands directly)
```
$ bluetoothctl
```
Then, enable scanning:
```
scan on
```
A large list of nearby Bluetooth devices will appear. Enable pairing on your device and find its MAC address in the list (will be formatted like AA:BB:CC:DD:EE:FF). Connect to your device with the following command using your MAC address from before:
```
connect AA:BB:CC:DD:EE:FF
```
Finally, disable scanning:
```
scan off
```
You may now exit the interactive shell by pressing Ctrl+D or by typing `exit`.
