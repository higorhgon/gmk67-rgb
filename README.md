# gmk67-rgb

> English fork of [aloran-dev/gmk67-rgb](https://github.com/aloran-dev/gmk67-rgb).

The program to change the backlight of the Zuoya GMK67 only exists for Windows, and OpenRGB does not recognize this version of the keyboard, so I decided to vibecode this small app.

The app aims to be simple: you pick an effect and a color, adjust brightness and speed, and that's it. I have only tested it with my own keyboard, so don't expect broad compatibility.

## What it does

- 19 effects plus "Off": static, breathing, wave, flowing, ripples, etc.
- Color, random colors, brightness, speed and direction.
- History of the last 8 applied colors.
- Color correction for the keyboard LEDs: without it, orange looks yellow and bluish greens look blue.
- On startup, it reads the effect currently set on the keyboard.

## Compatibility

Only the **GMK67 v1** connected **by USB cable**. It identifies like this:

```
$ lsusb
... ID 05ac:024f Apple, Inc. Aluminium Keyboard (ANSI)   ← ID borrowed by the firmware
$ cat /sys/class/hidraw/hidraw*/device/uevent | grep hfd
HID_NAME=hfd.cn USB DEVICE
```

The GMK67 **v2** (`36b0:3068`) uses QMK/VIA and does not need this: it works with [VIA](https://usevia.app).

## Installation

Download the package for your distribution from the upstream [Releases](https://github.com/aloran-dev/gmk67-rgb/releases) (Spanish UI):

| Distribution | Install |
|---|---|
| Ubuntu 24.04+, Debian 13+, Mint, Pop!_OS | `sudo apt install ./gmk67-rgb_*_all.deb` |
| Fedora 39+ | `sudo dnf install ./gmk67-rgb-*.noarch.rpm` |
| Any with Flatpak | `flatpak install --user gmk67-rgb-*.flatpak` |

The `.deb` and `.rpm` install a udev rule that gives the logged-in user access to the keyboard. A Flatpak cannot install rules on the system, so with Flatpak you have to add it once by hand. The app shows the commands the first time it cannot access the keyboard.

### From source

You need Python 3, PyGObject, GTK 4.12+ and libadwaita 1.4+ (on Ubuntu/Debian: `python3-gi gir1.2-gtk-4.0 gir1.2-adw-1`; on Arch: `python-gobject gtk4 libadwaita`).

```sh
git clone https://github.com/higorhgon/gmk67-rgb.git
cd gmk67-rgb
./install.sh            # app in ~/.local + udev rule (asks for sudo)
./install.sh --uninstall
```

## Protocol

It was obtained by analyzing the official Windows driver. They are 64-byte HID *feature reports*, with no report ID, on interface 0 of the keyboard. To set an effect, send:

| Packet | Meaning |
|---|---|
| `04 18` | enable the software effect |
| `04 13 00 00 00 00 00 00 01` | 1 page is about to be written |
| `[mode, R, G, B, 0, 0, 0, 0, random, brightness+1, speed+1, direction, 0, 0, AA, 55]` | the effect page |
| `04 02` | end of write |
| `04 F0` | apply |

Each command is acknowledged by a GET_FEATURE that returns `04 <cmd> 00 01`. Modes go from `0x01` to `0x13` and `0x00` turns the lights off. Brightness and speed go from 0 to 15. Details are in [`gmk67.py`](gmk67.py).

It looks like the Keychron K3 V2 protocol supported by OpenRGB, but this keyboard accepts and ignores the OpenRGB sequence (`04 19` + 18 pages).

Changes made with the Fn keys are not reflected in the effect saved by software.

It can also be used without the UI:

```python
import gmk67
with gmk67.Keyboard() as kb:
    kb.set_effect(gmk67.Effect(0x07, color=(255, 128, 0), speed=12))  # orange breathing
```

## License

[GPL-3.0](LICENSE)
