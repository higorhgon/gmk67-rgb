"""Compact color picker in a popover (replaces Gtk.ColorDialog)."""

import colorsys
import math

import gi
gi.require_version("Gtk", "4.0")
from gi.repository import GObject, Gdk, Gtk

SV_WIDTH, SV_HEIGHT = 240, 150
PAD = 8  # margin so the indicator is not clipped at the edges

_CSS = """
.cp-swatch { border-radius: 6px; box-shadow: inset 0 0 0 1px alpha(black, 0.3); }
.cp-hue trough {
  min-height: 12px; border-radius: 6px;
  background-image: linear-gradient(to right, #f00, #ff0, #0f0, #0ff, #00f, #f0f, #f00);
}
.cp-hue highlight { background: none; }
"""
_css_loaded = False


def _load_css():
    global _css_loaded
    if _css_loaded:
        return
    provider = Gtk.CssProvider()
    provider.load_from_string(_CSS)
    Gtk.StyleContext.add_provider_for_display(Gdk.Display.get_default(), provider,
                                              Gtk.STYLE_PROVIDER_PRIORITY_USER + 1)
    _css_loaded = True


class ColorPickerButton(Gtk.MenuButton):
    __gsignals__ = {"color-changed": (GObject.SignalFlags.RUN_FIRST, None, ())}

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        _load_css()
        self.h, self.s, self.v = 0.0, 0.0, 1.0
        self._updating = False

        self.swatch = Gtk.DrawingArea(content_width=34, content_height=22, css_classes=["cp-swatch"])
        self.swatch.set_draw_func(self._draw_swatch)
        self.set_child(self.swatch)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10,
                      margin_top=6, margin_bottom=6, margin_start=6, margin_end=6)

        self.sv = Gtk.DrawingArea(content_width=SV_WIDTH + 2 * PAD,
                                 content_height=SV_HEIGHT + 2 * PAD)
        self.sv.set_draw_func(self._draw_sv)
        drag = Gtk.GestureDrag()
        drag.connect("drag-begin", lambda g, x, y: self._pick_sv(x, y))
        drag.connect("drag-update", self._on_drag_update)
        self.sv.add_controller(drag)
        box.append(self.sv)

        self.hue = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 360, 1)
        self.hue.set_draw_value(False)
        self.hue.add_css_class("cp-hue")
        self.hue.connect("value-changed", self._on_hue)
        box.append(self.hue)

        self.entry = Gtk.Entry(max_length=7, width_chars=8, halign=Gtk.Align.CENTER, xalign=0.5)
        self.entry.connect("activate", self._on_entry)
        box.append(self.entry)

        self.set_popover(Gtk.Popover(child=box))
        self._sync()

    # --- API ---

    def get_color(self):
        return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(self.h, self.s, self.v))

    def set_color(self, rgb):
        h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
        if s > 0:  # for grays the hue does not matter: keep the previous one
            self.h = h
        self.s, self.v = s, v
        self._sync(emit=False)

    # --- internal ---

    def _sync(self, emit=True):
        self._updating = True
        self.hue.set_value(self.h * 360)
        self.entry.set_text("#%02x%02x%02x" % self.get_color())
        self._updating = False
        self.sv.queue_draw()
        self.swatch.queue_draw()
        if emit:
            self.emit("color-changed")

    def _pick_sv(self, x, y):
        self._drag_origin = (x, y)
        self.s = min(max((x - PAD) / SV_WIDTH, 0), 1)
        self.v = 1 - min(max((y - PAD) / SV_HEIGHT, 0), 1)
        self._sync()

    def _on_drag_update(self, _gesture, dx, dy):
        x0, y0 = self._drag_origin
        self.s = min(max((x0 + dx - PAD) / SV_WIDTH, 0), 1)
        self.v = 1 - min(max((y0 + dy - PAD) / SV_HEIGHT, 0), 1)
        self._sync()

    def _on_hue(self, scale):
        if self._updating:
            return
        self.h = scale.get_value() / 360
        self._sync()

    def _on_entry(self, entry):
        rgba = Gdk.RGBA()
        if rgba.parse(entry.get_text().strip()):
            self.set_color(tuple(round(c * 255) for c in (rgba.red, rgba.green, rgba.blue)))
            self.emit("color-changed")
        else:
            self._sync(emit=False)

    def _draw_swatch(self, _area, cr, w, h):
        cr.set_source_rgb(*colorsys.hsv_to_rgb(self.h, self.s, self.v))
        _rounded(cr, w, h, 6)
        cr.fill()

    def _draw_sv(self, _area, cr, w, h):
        import cairo
        w, h = w - 2 * PAD, h - 2 * PAD
        cr.translate(PAD, PAD)
        cr.save()
        _rounded(cr, w, h, 8)
        cr.clip()
        cr.set_source_rgb(*colorsys.hsv_to_rgb(self.h, 1, 1))
        cr.paint()
        white = cairo.LinearGradient(0, 0, w, 0)
        white.add_color_stop_rgba(0, 1, 1, 1, 1)
        white.add_color_stop_rgba(1, 1, 1, 1, 0)
        cr.set_source(white)
        cr.paint()
        black = cairo.LinearGradient(0, 0, 0, h)
        black.add_color_stop_rgba(0, 0, 0, 0, 0)
        black.add_color_stop_rgba(1, 0, 0, 0, 1)
        cr.set_source(black)
        cr.paint()
        cr.restore()
        x, y = self.s * w, (1 - self.v) * h
        cr.arc(x, y, 7, 0, 2 * math.pi)
        cr.set_source_rgb(1, 1, 1)
        cr.set_line_width(2.5)
        cr.stroke_preserve()
        cr.set_source_rgba(0, 0, 0, 0.4)
        cr.set_line_width(1)
        cr.stroke()


def _rounded(cr, w, h, r):
    cr.new_sub_path()
    cr.arc(w - r, r, r, -math.pi / 2, 0)
    cr.arc(w - r, h - r, r, 0, math.pi / 2)
    cr.arc(r, h - r, r, math.pi / 2, math.pi)
    cr.arc(r, r, r, math.pi, 3 * math.pi / 2)
    cr.close_path()
