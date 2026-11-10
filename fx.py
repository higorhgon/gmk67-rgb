#!/usr/bin/python3
"""Software (host-animated) effects for the GMK67.

The keyboard has no built-in fire, plasma, etc. Here the PC renders each frame
and streams it with Keyboard.send_live() (command 04 20). Nothing is written to
the keyboard's flash; when streaming stops the keyboard returns to its stored
effect after a few seconds.

Run without the UI:  gmk67-fx flames [--speed 8] [--brightness 15] [--fps 60]
"""

import argparse
import colorsys
import math
import os
import random
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import gmk67

# The keyboard seems to drop some live frames; 60 fps looked smoother than 10-30 fps
FPS = 60


def _lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def _palette(stops, v):
    """Interpolate a list of (position, (r, g, b)) stops at v in 0..1."""
    v = min(max(v, 0.0), 1.0)
    for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
        if v <= p1:
            return _lerp(c0, c1, (v - p0) / (p1 - p0) if p1 > p0 else 0)
    return stops[-1][1]


def _hsv(h, s=1.0, v=1.0):
    return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(h % 1.0, s, v))


class Effect:
    """Base class: frame(t, dt) returns one (r, g, b) per gmk67.KEYS entry."""
    name = ""
    uses_color = False

    def __init__(self, speed=8, color=(255, 255, 255)):
        self.speed = speed      # 0..15
        self.color = color

    @property
    def rate(self):
        """Speed 0..15 mapped to a time multiplier around 1.0."""
        return 0.25 * 2 ** (self.speed / 5)

    def frame(self, t, dt):
        raise NotImplementedError


class Flames(Effect):
    """Classic fire simulation: heat rises from the space bar row and cools."""
    name = "Flames"
    W, H = 34, 14        # simulation grid (finer than the keys)
    SOURCE_ROWS = 3      # hidden rows below the bottom key row that feed the fire
    PALETTE = [(0.0, (0, 0, 0)), (0.18, (60, 0, 0)), (0.38, (200, 20, 0)),
               (0.6, (255, 90, 0)), (0.8, (255, 170, 10)), (1.0, (255, 240, 140))]

    def __init__(self, **kw):
        super().__init__(**kw)
        self.heat = [[0.0] * self.W for _ in range(self.H)]
        self.acc = 0.0
        self.cooling = [[random.uniform(0.0, 0.06) for _ in range(self.W)] for _ in range(self.H)]
        self.cool_shift = 0.0

    def step(self):
        W, H, heat = self.W, self.H, self.heat
        # Fuel: random hot spots along the bottom hidden rows
        for y in range(H - self.SOURCE_ROWS, H):
            for x in range(W):
                if random.random() < 0.35:
                    heat[y][x] = random.uniform(0.75, 1.0)
                else:
                    heat[y][x] *= 0.8
        # Rise: each cell takes the average of the cells below it, minus cooling.
        # The cooling map scrolls up so the flame tongues flicker instead of standing still.
        self.cool_shift = (self.cool_shift + 1) % H
        for y in range(H - self.SOURCE_ROWS - 1, -1, -1):
            below = heat[y + 1]
            below2 = heat[min(y + 2, H - 1)]
            row = heat[y]
            cool = self.cooling[int(y + self.cool_shift) % H]
            for x in range(W):
                v = (below[x - 1 if x else x] + below[x] + below[x + 1 if x < W - 1 else x] + below2[x]) / 4
                row[x] = max(v - 0.045 - cool[x], 0.0)

    def frame(self, t, dt):
        self.acc += dt * 22 * self.rate
        steps = min(int(self.acc), 6)
        self.acc -= steps
        for _ in range(steps):
            self.step()
        out = []
        visible = self.H - self.SOURCE_ROWS
        for _i, x, y in gmk67.KEYS:
            gx = min(int(x / gmk67.WIDTH * self.W), self.W - 1)
            # key row 0 (top) → upper part of the grid, row 4 (space) → just above the fuel
            gy = round(2 + y / (gmk67.HEIGHT - 1) * (visible - 3))
            out.append(_palette(self.PALETTE, self.heat[gy][gx]))
        return out


class RainbowWave(Effect):
    name = "Rainbow wave"

    def frame(self, t, dt):
        return [_hsv(x / gmk67.WIDTH - t * 0.25 * self.rate + y * 0.03) for _i, x, y in gmk67.KEYS]


class Plasma(Effect):
    name = "Plasma"

    def frame(self, t, dt):
        t *= self.rate
        out = []
        for _i, x, y in gmk67.KEYS:
            v = (math.sin(x * 0.55 + t)
                 + math.sin(y * 0.9 - t * 1.3)
                 + math.sin((x + y) * 0.35 + t * 0.7)
                 + math.sin(math.hypot(x - 8, y - 2) * 0.7 - t * 1.1))
            out.append(_hsv(v / 8 + t * 0.05))
        return out


class Matrix(Effect):
    """Drops falling down each column, leaving a fading trail."""
    name = "Matrix rain"
    uses_color = True

    def __init__(self, **kw):
        super().__init__(**kw)
        self.drops = {}  # column -> head y position

    def frame(self, t, dt):
        for col in range(int(gmk67.WIDTH) + 1):
            if col in self.drops:
                self.drops[col] += dt * 3.0 * self.rate
                if self.drops[col] > gmk67.HEIGHT + 3:
                    del self.drops[col]
            elif random.random() < dt * 0.6 * self.rate:
                self.drops[col] = -0.5
        head = _lerp(self.color, (255, 255, 255), 0.6)
        out = []
        for _i, x, y in gmk67.KEYS:
            d = self.drops.get(int(x))
            if d is None or y > d + 0.5:
                out.append((0, 0, 0))
            elif d - y < 0.6:
                out.append(head)
            else:
                out.append(_lerp(self.color, (0, 0, 0), min((d - y) / 3.5, 1.0)))
        return out


class Twinkle(Effect):
    """Keys fade in and out at random, like stars."""
    name = "Twinkle"
    uses_color = True

    def __init__(self, **kw):
        super().__init__(**kw)
        self.phase = [random.uniform(0, 2 * math.pi) for _ in gmk67.KEYS]
        self.freq = [random.uniform(0.4, 1.2) for _ in gmk67.KEYS]

    def frame(self, t, dt):
        t *= self.rate
        return [_lerp((0, 0, 0), self.color, max(math.sin(t * f + p), 0) ** 3)
                for p, f in zip(self.phase, self.freq)]


EFFECTS = [Flames, RainbowWave, Plasma, Matrix, Twinkle]


def scale(colors, brightness):
    """Brightness 0..15 → 0..1 multiplier."""
    k = (brightness + 1) / (gmk67.MAX_LEVEL + 1)
    return [tuple(round(c * k) for c in rgb) for rgb in colors]


class Streamer:
    """Runs an effect in a background thread, reconnecting if the keyboard is replugged."""

    def __init__(self, effect, brightness=gmk67.MAX_LEVEL, fps=FPS, on_error=None):
        self.effect = effect
        self.brightness = brightness
        self.fps = fps
        self.on_error = on_error
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._thread.start()
        return self

    def stop(self):
        self._stop.set()
        if self._thread.is_alive() and self._thread is not threading.current_thread():
            self._thread.join(timeout=2)

    def _run(self):
        period = 1 / self.fps
        reported = False
        while not self._stop.is_set():
            try:
                with gmk67.Keyboard() as kb:
                    reported = False
                    last = start = time.monotonic()
                    while not self._stop.is_set():
                        now = time.monotonic()
                        colors = self.effect.frame(now - start, now - last)
                        last = now
                        kb.send_live(scale(colors, self.brightness))
                        self._stop.wait(max(period - (time.monotonic() - now), 0))
            except (gmk67.DeviceNotFound, OSError) as e:
                if not reported and self.on_error:
                    self.on_error(e)
                    reported = True
                self._stop.wait(1)


def main():
    names = {cls.name.split()[0].lower(): cls for cls in EFFECTS}
    p = argparse.ArgumentParser(description="Stream a software lighting effect to the GMK67.")
    p.add_argument("effect", choices=sorted(names))
    p.add_argument("--speed", type=int, default=8, help="0-15 (default 8)")
    p.add_argument("--brightness", type=int, default=gmk67.MAX_LEVEL, help="0-15 (default 15)")
    p.add_argument("--color", default="00ff40", help="hex color for matrix/twinkle")
    p.add_argument("--fps", type=int, default=FPS)
    a = p.parse_args()
    color = tuple(int(a.color.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4))
    effect = names[a.effect](speed=a.speed, color=color)
    streamer = Streamer(effect, a.brightness, a.fps,
                        on_error=lambda e: print(f"gmk67-fx: {e}", file=sys.stderr)).start()
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        streamer.stop()


if __name__ == "__main__":
    main()
