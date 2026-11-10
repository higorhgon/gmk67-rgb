# GMK67

Lighting for the Zuoya GMK67 keyboard on [Omarchy](https://omarchy.org): the
keyboard's own 19 effects, plus effects it can't do by itself — flames, plasma,
a rainbow wave, Matrix rain, and twinkling stars — drawn on the PC and sent to
the keys live.

![GMK67 playing Flames](docs/screenshot.png)

A C++/Qt rewrite of [aloran-dev/gmk67-rgb](https://github.com/aloran-dev/gmk67-rgb),
in the style of Omarchy's own apps: one window, driven from the keyboard,
following the theme's colors, rounding and language.

## Compatibility

Only the **GMK67 v1** connected **by USB cable**. The 2.4 GHz dongle and
Bluetooth don't carry the lighting commands. It identifies like this:

```
$ lsusb
... ID 05ac:024f Apple, Inc. Aluminium Keyboard (ANSI)   ← ID borrowed by the firmware
$ grep hfd /sys/class/hidraw/hidraw*/device/uevent
HID_NAME=hfd.cn USB DEVICE
```

The GMK67 **v2** (`36b0:3068`) runs QMK/VIA and works with [VIA](https://usevia.app).

## Install

From the [higorhgon arch-repo](https://github.com/higorhgon/arch-repo):

```sh
sudo pacman -S gmk67-git
```

The package installs a udev rule that lets the logged-in user use the keyboard;
unplug and replug the cable once after installing. Then open **GMK67** from the
launcher (`Super + Space`).

To build it yourself: `bin/build` (needs `qt6-base`, `qt6-declarative`, `qmake6`
and `make`), `bin/test` runs the tests, and `bin/install` builds and installs the
package with `makepkg`.

## Keyboard effects and animated effects

- **Keyboard effects** are saved on the keyboard. They keep running after you
  close the app or unplug the cable.
- **Animated effects** are drawn by this app, 60 frames a second, and play while
  it's open. Nothing is written to the keyboard's memory: when they stop, it goes
  back to its own effect after a few seconds. To keep one playing without the
  window, use `gmk67 animate` (below).

## Remapping keys

The **Keys** tab (`Tab` switches to it) gives any key another function: function
keys up to F24, media and browser keys, navigation, modifiers, symbols, letters
and numbers. Click a key, pick its new function from the list, then **Save to
keyboard**. The table is saved on the keyboard and works on any computer.

- Fn can't be remapped, and the Fn layer keeps its stock functions.
- The keyboard can't report its table, so the tab shows what this app saved
  last (kept in `~/.config/gmk67/profile.json`).
- If a remap makes typing hard, `gmk67 keys reset` gives every key back its own
  function. Restore all keys and Save does the same with only the mouse.

## Shortcuts

| Key | Action |
|---|---|
| `↑` / `↓`, `k` / `j` | Previous or next effect |
| `Space` / `Enter` | Apply the effect (play, for animated ones) |
| `Esc` | Stop the animation |
| `←` / `→` | Less or more brightness |
| `Shift + ←` / `Shift + →` | Slower or faster |
| `c` | Pick a color |
| `1` … `8` | Use a recent color |
| `r` | Random colors on or off |
| `Tab` | Switch between lighting, keys and macros |
| `Delete` | Restore the picked key (Keys tab) |
| `?` / `F1` | Show the shortcuts |
| `q` | Quit |

## Command line

Every command works without a window, for scripts and agents. Errors go to
stderr and the exit code is 0 or 1.

```sh
gmk67 list [--json]                       # every effect; animated ones marked
gmk67 status [--json]                     # the keyboard and its current effect
gmk67 set wave --color '#ff8000' --speed 12 --direction right-left
gmk67 set breathing --random --brightness 8
gmk67 animate flames --speed 10           # plays until Ctrl+C or SIGTERM
gmk67 keys reset                          # undo every key remap
```

`gmk67 animate` can run from Hyprland's autostart to keep an animation going
all the time:

```lua
-- ~/.config/hypr/autostart.lua
o.exec_on_start("gmk67 animate plasma --brightness 10")
```

## Protocol

64-byte HID feature reports with no report ID, on interface 0 of the wired
keyboard.

| Packets | Meaning |
|---|---|
| `04 18`, `04 13 … [8]=01`, page, `04 02`, `04 F0` | Save an effect. Each command is acknowledged with `04 <cmd> 00 01`. |
| `[mode, R, G, B, 0, 0, 0, 0, random, brightness+1, speed+1, direction, 0, 0, AA, 55]` | The effect page. Modes `0x01`–`0x13`; `0x00` is off. |
| `04 12 … [8]=01`, page, `04 02` | Read the saved effect. |
| `04 02`, `04 20 … [8]=08`, 8 pages | One live frame: `[light index, R, G, B]` for each of the 66 keys, in row order. Not saved. |
| `04 18`, `04 11 … [8]=09`, 9 pages, `04 F0` | The base layer's remap table, then the same with `04 27` for the Fn layer. 576 bytes ending in `AA 55`, four per key at its light index × 4: `[2, 0, usage, 0]` for a keyboard key, `[3, lo, hi, 0]` for a consumer key, zeros for the stock function. |

Live frames skip the firmware's color handling, so green needs much less power
there: the app corrects colors separately for effect pages and live frames.
Details are in [`src/protocol.cpp`](src/protocol.cpp) and
[`src/keyboard.cpp`](src/keyboard.cpp).

`04 11` and `04 27` each replace a whole table: a key left out goes back to its
stock function. The app always writes both, the Fn one empty.

## Credits

- [aloran-dev/gmk67-rgb](https://github.com/aloran-dev/gmk67-rgb) (GPL-3.0), the
  original app and the effect protocol, taken from the official Windows driver.
- [yasasgit/gmk67-openrgb](https://github.com/yasasgit/gmk67-openrgb)'s
  `docs/PROTOCOL.md`: the live frame command and the keys' light indices.
- [bitten-security/zuoya-gmk67](https://github.com/bitten-security/zuoya-gmk67)
  (GPL-3.0)'s `docs/PROTOCOL.md` and `protocol.py`: the remap tables, verified
  there on the keyboard.
- The theme and translation classes and the build scripts follow Omacut and
  Monologue, and the look of the controls follows Hype (MIT, David Heinemeier
  Hansson), by way of [Omajim](https://github.com/higorhgon/omajim), whose
  `SmoothWheel.qml` also scrolls the lists here.

## License

[GPL-3.0](LICENSE)
