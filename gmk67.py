"""RGB protocol of the Zuoya GMK67 (v1, hfd.cn HFD801KJC firmware, 05ac:024f).

Taken from the official driver (DeviceDriver.exe, FUN_0041db20): 64-byte
feature reports with no report ID on interface 0. To set an effect, send:

    04 18                       enable the software effect
    04 13 .. .. .. .. .. .. 01  1 page is about to be written
    [effect page]
    04 02                       end of write
    04 F0                       apply

Each command is acknowledged by a GET_FEATURE that returns "04 <cmd> 00 01".
"""

import fcntl
import glob
import os
import time
from dataclasses import dataclass, field

VID, PID = 0x05AC, 0x024F
# Must match 70-gmk67.rules; the app shows it when there is no permission on the hidraw
UDEV_RULE = ('SUBSYSTEM=="hidraw", ATTRS{idVendor}=="05ac", ATTRS{idProduct}=="024f", '
             'ATTRS{manufacturer}=="hfd.cn", TAG+="uaccess"')
PACKET_LEN = 64
HEADER = 0x04
CHECK_L, CHECK_H = 0xAA, 0x55
DELAY = 0.015  # the driver waits 15 ms between packets

CMD_COMM_END = 0x02
CMD_READ_EFFECT = 0x12
CMD_WRITE_EFFECT = 0x13
CMD_SOFTWARE_EFFECT = 0x18
CMD_LIVE_FRAME = 0x20
CMD_EFFECT_START = 0xF0

LIVE_PAGES = 8   # a live frame is 8 pages = 512 bytes, only the first 66 * 4 are used
LIVE_MAX = 0xFE  # the vendor app never sends 0xff in a live frame


def _row(indices, xs, y):
    return [(i, x, y) for i, x in zip(indices, xs)]


# Every lit key as (light_index, x, y), in the packed order of a live frame
# (row by row, left to right). x/y are the key centers in key units on the 65%
# layout; light_index values come from the vendor KeyboardLayout.xml.
KEYS = (
    _row([1, *range(20, 32), 103],
         [0.5, *(1.5 + i for i in range(12)), 14.0], 0)
    + _row([37, *range(38, 50), 67, 119],
           [0.75, *(2.0 + i for i in range(12)), 14.25, 15.5], 1)
    + _row([55, *range(56, 67), 85, 118],
           [0.875, *(2.25 + i for i in range(11)), 13.875, 15.5], 2)
    + _row([73, *range(74, 84), 84, 101, 121],
           [1.125, *(2.75 + i for i in range(10)), 13.125, 14.5, 15.5], 3)
    + _row([91, 92, 93, 94, 95, 96, 99, 100, 102],
           [0.625, 1.875, 3.125, 6.875, 10.5, 11.5, 13.5, 14.5, 15.5], 4)
)
WIDTH, HEIGHT = 16.0, 5.0  # layout size in key units

# The LEDs are not linear like a screen: without gamma, orange ff8000 looks
# yellow, and blue dominates so much that mint 1deeb2 looks blue. Corrected on
# send and undone on read. Values tuned by eye on the keyboard.
GAMMA = 2.2
GAIN = (0.97, 1.0, 0.33)  # R, G, B after gamma
# Live frames (04 20) bypass the firmware's own color handling and green is far
# stronger there: orange ff8000 only looks orange with a raw green of about 14.
LIVE_GAIN = (1.0, 0.26, 0.33)

MAX_LEVEL = 0x0F  # brightness and speed go from 0 to 15; on the page they are +1

DIR_NONE, DIR_LR, DIR_UD = 0, 1, 2

# (value, name, supports color, supports speed, direction kind)
# Values from the driver's mode table (1033.lan, ids 200-219).
MODES = [
    (0x01, "Static", True, False, DIR_NONE),
    (0x02, "Light on press", True, True, DIR_NONE),
    (0x03, "Light off on press", True, True, DIR_NONE),
    (0x04, "Glittering", True, True, DIR_NONE),
    (0x05, "Raindrops", True, True, DIR_NONE),
    (0x06, "Random colors", False, True, DIR_NONE),
    (0x07, "Breathing", True, True, DIR_NONE),
    (0x08, "Spectrum cycle", False, True, DIR_NONE),
    (0x09, "Outward", True, True, DIR_NONE),
    (0x0A, "Scrolling", True, True, DIR_UD),
    (0x0B, "Wave", True, True, DIR_LR),
    (0x0C, "Rotating", True, True, DIR_NONE),
    (0x0D, "Explode", True, True, DIR_NONE),
    (0x0E, "Launch", True, True, DIR_NONE),
    (0x0F, "Ripples", True, True, DIR_NONE),
    (0x10, "Flowing", True, True, DIR_LR),
    (0x11, "Pulsating", True, True, DIR_NONE),
    (0x12, "Tilt", True, True, DIR_NONE),
    (0x13, "Shuttle", True, True, DIR_LR),
    (0x00, "Off", False, False, DIR_NONE),
]

# Raw directions: 0 left→right, 1 right→left, 2 bottom→top, 3 top→bottom
DIRECTIONS = {
    DIR_LR: [(0, "Left → right"), (1, "Right → left")],
    DIR_UD: [(2, "Bottom → top"), (3, "Top → bottom")],
}


def to_led(color, gain=GAIN):
    linear = [(c / 255) ** GAMMA for c in color]
    balanced = [v * k for v, k in zip(linear, gain)]
    # Rescale so the strongest channel keeps its intensity (pure blue does not go dark)
    scale = max(linear) / max(balanced) if max(balanced) else 0
    return tuple(round(255 * v * scale) for v in balanced)


def from_led(color):
    balanced = [c / 255 for c in color]
    linear = [v / k for v, k in zip(balanced, GAIN)]
    scale = max(balanced) / max(linear) if max(linear) else 0
    return tuple(min(round(255 * (v * scale) ** (1 / GAMMA)), 255) for v in linear)


def _ioc(direction, nr, size):
    return (direction << 30) | (size << 16) | (ord("H") << 8) | nr


def HIDIOCSFEATURE(size):
    return _ioc(3, 0x06, size)


def HIDIOCGFEATURE(size):
    return _ioc(3, 0x07, size)


@dataclass
class Effect:
    value: int
    color: tuple = (255, 255, 255)
    random: bool = False
    brightness: int = MAX_LEVEL
    speed: int = 10
    direction: int = 0

    def page(self):
        b = bytearray(PACKET_LEN)
        b[0] = self.value
        b[1:4] = bytes(to_led(self.color))
        b[8] = int(self.random)
        b[9] = self.brightness + 1
        b[10] = self.speed + 1
        b[11] = self.direction
        b[14], b[15] = CHECK_L, CHECK_H
        return bytes(b)


@dataclass
class Profile:
    """Settings for each effect, remembered when switching modes in the app."""
    effects: dict = field(default_factory=lambda: {m[0]: Effect(m[0]) for m in MODES})
    active: int = 0x01


class DeviceNotFound(Exception):
    pass


class ProtocolError(Exception):
    pass


def find_hidraw():
    """Return /dev/hidrawN for interface 0 of the GMK67."""
    for sysdir in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(os.path.join(sysdir, "device/uevent")) as f:
                uevent = dict(l.strip().split("=", 1) for l in f if "=" in l)
            with open(os.path.join(sysdir, "device/report_descriptor"), "rb") as f:
                desc = f.read()
        except OSError:
            continue
        if uevent.get("HID_ID", "").upper() != f"0003:{VID:08X}:{PID:08X}":
            continue
        # The control interface has a 64-byte feature report (95 40 75 08 b1 02)
        # and is the keyboard one (05 01 09 06); a real Apple keyboard does not have it.
        if desc.startswith(b"\x05\x01\x09\x06") and b"\x95\x40\x75\x08\xb1\x02" in desc:
            return "/dev/" + os.path.basename(sysdir)
    raise DeviceNotFound("No GMK67 connected by USB cable was found")


class Keyboard:
    def __init__(self, path=None):
        self.path = path or find_hidraw()
        self.fd = os.open(self.path, os.O_RDWR)

    def close(self):
        os.close(self.fd)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def send(self, data):
        buf = bytearray(PACKET_LEN + 1)  # byte 0 = report ID 0
        buf[1:1 + len(data)] = data
        time.sleep(DELAY)
        fcntl.ioctl(self.fd, HIDIOCSFEATURE(len(buf)), buf)

    def read(self):
        buf = bytearray(PACKET_LEN + 1)
        time.sleep(DELAY)
        fcntl.ioctl(self.fd, HIDIOCGFEATURE(len(buf)), buf, True)
        return bytes(buf[1:])

    def command(self, cmd, pages=0):
        pkt = bytearray(PACKET_LEN)
        pkt[0], pkt[1], pkt[8] = HEADER, cmd, pages
        self.send(pkt)
        reply = self.read()
        if reply[:2] != bytes([HEADER, cmd]) or reply[3] != 0x01:
            raise ProtocolError(f"The keyboard did not acknowledge command {cmd:02x}: {reply[:9].hex(' ')}")

    def set_effect(self, effect: Effect):
        self.command(CMD_SOFTWARE_EFFECT)
        self.command(CMD_WRITE_EFFECT, 1)
        self.send(effect.page())
        self.command(CMD_COMM_END)
        self.command(CMD_EFFECT_START)

    def send_live(self, colors, correct=True):
        """Show one temporary frame: colors is a list of (R, G, B), one per KEYS entry.

        Nothing is saved: if frames stop, the keyboard goes back to its stored
        effect after a few seconds, so animations must keep sending.
        """
        frame = bytearray(PACKET_LEN * LIVE_PAGES)
        for n, ((index, _x, _y), color) in enumerate(zip(KEYS, colors)):
            r, g, b = to_led(color, LIVE_GAIN) if correct else color
            frame[n * 4:n * 4 + 4] = bytes((index, min(r, LIVE_MAX), min(g, LIVE_MAX), min(b, LIVE_MAX)))
        self.send_fast(bytes((HEADER, CMD_COMM_END)))
        self.read_fast()
        self.send_fast(bytes((HEADER, CMD_LIVE_FRAME, 0, 0, 0, 0, 0, 0, LIVE_PAGES)))
        self.read_fast()
        for page in range(LIVE_PAGES):
            self.send_fast(frame[page * PACKET_LEN:(page + 1) * PACKET_LEN])

    def send_fast(self, data):
        """Like send() but without the fixed delay, for live frames."""
        buf = bytearray(PACKET_LEN + 1)
        buf[1:1 + len(data)] = data
        fcntl.ioctl(self.fd, HIDIOCSFEATURE(len(buf)), buf)

    def read_fast(self):
        buf = bytearray(PACKET_LEN + 1)
        fcntl.ioctl(self.fd, HIDIOCGFEATURE(len(buf)), buf, True)
        return bytes(buf[1:])

    def read_effect(self):
        """Effect saved by software (does not reflect changes made with Fn)."""
        pkt = bytearray(PACKET_LEN)
        pkt[0], pkt[1], pkt[8] = HEADER, CMD_READ_EFFECT, 1
        self.send(pkt)
        # It usually replies with the page directly, but sometimes the ack arrives first
        page = self.read()
        if page[:4] == bytes([HEADER, CMD_READ_EFFECT, 0, 1]):
            page = self.read()
        self.command(CMD_COMM_END)
        return Effect(value=page[0], color=from_led(page[1:4]), random=bool(page[8]),
                      brightness=max(page[9] - 1, 0), speed=max(page[10] - 1, 0),
                      direction=page[11])


if __name__ == "__main__":
    with Keyboard() as kb:
        print(kb.path, kb.read_effect())
