// The protocol was worked out by aloran-dev/gmk67-rgb (effect pages, from the
// official Windows driver) and yasasgit/gmk67-openrgb's docs/PROTOCOL.md
// (live frames and the light indices).
#include "protocol.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace Protocol {

const QList<Mode> &modes() {
    static const QList<Mode> list = {
        {0x01, QT_TRANSLATE_NOOP("Protocol", "Static"), "static", true, false, NoDirection},
        {0x02, QT_TRANSLATE_NOOP("Protocol", "Light on press"), "light-on-press", true, true, NoDirection},
        {0x03, QT_TRANSLATE_NOOP("Protocol", "Light off on press"), "light-off-on-press", true, true, NoDirection},
        {0x04, QT_TRANSLATE_NOOP("Protocol", "Glittering"), "glittering", true, true, NoDirection},
        {0x05, QT_TRANSLATE_NOOP("Protocol", "Raindrops"), "raindrops", true, true, NoDirection},
        {0x06, QT_TRANSLATE_NOOP("Protocol", "Random colors"), "random-colors", false, true, NoDirection},
        {0x07, QT_TRANSLATE_NOOP("Protocol", "Breathing"), "breathing", true, true, NoDirection},
        {0x08, QT_TRANSLATE_NOOP("Protocol", "Spectrum cycle"), "spectrum", false, true, NoDirection},
        {0x09, QT_TRANSLATE_NOOP("Protocol", "Outward"), "outward", true, true, NoDirection},
        {0x0A, QT_TRANSLATE_NOOP("Protocol", "Scrolling"), "scrolling", true, true, UpDown},
        {0x0B, QT_TRANSLATE_NOOP("Protocol", "Wave"), "wave", true, true, LeftRight},
        {0x0C, QT_TRANSLATE_NOOP("Protocol", "Rotating"), "rotating", true, true, NoDirection},
        {0x0D, QT_TRANSLATE_NOOP("Protocol", "Explode"), "explode", true, true, NoDirection},
        {0x0E, QT_TRANSLATE_NOOP("Protocol", "Launch"), "launch", true, true, NoDirection},
        {0x0F, QT_TRANSLATE_NOOP("Protocol", "Ripples"), "ripples", true, true, NoDirection},
        {0x10, QT_TRANSLATE_NOOP("Protocol", "Flowing"), "flowing", true, true, LeftRight},
        {0x11, QT_TRANSLATE_NOOP("Protocol", "Pulsating"), "pulsating", true, true, NoDirection},
        {0x12, QT_TRANSLATE_NOOP("Protocol", "Tilt"), "tilt", true, true, NoDirection},
        {0x13, QT_TRANSLATE_NOOP("Protocol", "Shuttle"), "shuttle", true, true, LeftRight},
        {0x00, QT_TRANSLATE_NOOP("Protocol", "Off"), "off", false, false, NoDirection},
        {SoftwareBase + 0, QT_TRANSLATE_NOOP("Protocol", "Flames"), "flames", false, true, NoDirection},
        {SoftwareBase + 1, QT_TRANSLATE_NOOP("Protocol", "Rainbow wave"), "rainbow-wave", false, true, NoDirection},
        {SoftwareBase + 2, QT_TRANSLATE_NOOP("Protocol", "Plasma"), "plasma", false, true, NoDirection},
        {SoftwareBase + 3, QT_TRANSLATE_NOOP("Protocol", "Matrix rain"), "matrix-rain", true, true, NoDirection},
        {SoftwareBase + 4, QT_TRANSLATE_NOOP("Protocol", "Twinkle"), "twinkle", true, true, NoDirection},
    };
    return list;
}

const Mode *findMode(int value) {
    for (const auto &mode : modes())
        if (mode.value == value)
            return &mode;
    return nullptr;
}

const Mode *findMode(const QString &slug) {
    for (const auto &mode : modes())
        if (slug == QLatin1String(mode.slug))
            return &mode;
    return nullptr;
}

QList<int> directions(DirectionKind kind) {
    switch (kind) {
    case LeftRight: return {0, 1};
    case UpDown: return {2, 3};
    case NoDirection: break;
    }
    return {};
}

QByteArray Effect::page() const {
    QByteArray b(PacketSize, '\0');
    const QColor led = toLed(color, Path::EffectPage);
    b[0] = char(value);
    b[1] = char(led.red());
    b[2] = char(led.green());
    b[3] = char(led.blue());
    b[8] = char(random ? 1 : 0);
    b[9] = char(std::clamp(brightness, 0, MaxLevel) + 1);
    b[10] = char(std::clamp(speed, 0, MaxLevel) + 1);
    b[11] = char(direction);
    b[14] = char(0xAA);
    b[15] = char(0x55);
    return b;
}

Effect Effect::fromPage(const QByteArray &page) {
    Effect e;
    if (page.size() < 12)
        return e;
    auto byte = [&](int i) { return int(quint8(page[i])); };
    e.value = byte(0);
    e.color = fromLed(QColor(byte(1), byte(2), byte(3)));
    e.random = byte(8) != 0;
    e.brightness = std::max(byte(9) - 1, 0);
    e.speed = std::max(byte(10) - 1, 0);
    e.direction = byte(11);
    return e;
}

namespace {
constexpr double Gamma = 2.2;
// R, G, B after gamma, tuned by eye on the keyboard.
constexpr double PageGain[3] = {0.97, 1.0, 0.33};
constexpr double LiveGain[3] = {1.0, 0.26, 0.33};
}  // namespace

QColor toLed(const QColor &color, Path path) {
    const double *gain = path == Path::LiveFrame ? LiveGain : PageGain;
    const double linear[3] = {std::pow(color.redF(), Gamma), std::pow(color.greenF(), Gamma),
                              std::pow(color.blueF(), Gamma)};
    double balanced[3];
    for (int i = 0; i < 3; ++i)
        balanced[i] = linear[i] * gain[i];
    // Rescale so the strongest channel keeps its intensity: pure blue would
    // otherwise come out dim.
    const double top = *std::max_element(balanced, balanced + 3);
    const double scale = top > 0 ? *std::max_element(linear, linear + 3) / top : 0;
    return QColor(int(std::lround(255 * balanced[0] * scale)), int(std::lround(255 * balanced[1] * scale)),
                  int(std::lround(255 * balanced[2] * scale)));
}

QColor fromLed(const QColor &color) {
    const double balanced[3] = {color.redF(), color.greenF(), color.blueF()};
    double linear[3];
    for (int i = 0; i < 3; ++i)
        linear[i] = balanced[i] / PageGain[i];
    const double top = *std::max_element(linear, linear + 3);
    const double scale = top > 0 ? *std::max_element(balanced, balanced + 3) / top : 0;
    auto channel = [&](int i) {
        return std::min(int(std::lround(255 * std::pow(linear[i] * scale, 1 / Gamma))), 255);
    };
    return QColor(channel(0), channel(1), channel(2));
}

namespace {
QList<Key> buildKeys() {
    QList<Key> k;
    auto row = [&](double y, std::initializer_list<Key> items) {
        for (Key key : items) {
            key.y = y;
            k.append(key);
        }
    };
    static const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
    static const char *top[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]"};
    static const char *home[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'"};
    static const char *bottom[] = {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"};

    row(0, {{1, 0.5, 0, 1, "Esc"}});
    for (int i = 0; i < 12; ++i)
        row(0, {{20 + i, 1.5 + i, 0, 1, digits[i]}});
    row(0, {{103, 14.0, 0, 2, "⌫"}});

    row(1, {{37, 0.75, 0, 1.5, "Tab"}});
    for (int i = 0; i < 12; ++i)
        row(1, {{38 + i, 2.0 + i, 0, 1, top[i]}});
    row(1, {{67, 14.25, 0, 1.5, "\\"}, {119, 15.5, 0, 1, "Del"}});

    row(2, {{55, 0.875, 0, 1.75, "Caps"}});
    for (int i = 0; i < 11; ++i)
        row(2, {{56 + i, 2.25 + i, 0, 1, home[i]}});
    row(2, {{85, 13.875, 0, 2.25, "Enter"}, {118, 15.5, 0, 1, "PgUp"}});

    row(3, {{73, 1.125, 0, 2.25, "Shift"}});
    for (int i = 0; i < 10; ++i)
        row(3, {{74 + i, 2.75 + i, 0, 1, bottom[i]}});
    row(3, {{84, 13.125, 0, 1.75, "Shift"}, {101, 14.5, 0, 1, "↑"}, {121, 15.5, 0, 1, "PgDn"}});

    row(4, {{91, 0.625, 0, 1.25, "Ctrl"}, {92, 1.875, 0, 1.25, "Super"}, {93, 3.125, 0, 1.25, "Alt"},
            {94, 6.875, 0, 6.25, ""}, {95, 10.5, 0, 1, "Alt"}, {96, 11.5, 0, 1, "Fn"},
            {99, 13.5, 0, 1, "←"}, {100, 14.5, 0, 1, "↓"}, {102, 15.5, 0, 1, "→"}});
    return k;
}
}  // namespace

const QList<Key> &keys() {
    static const QList<Key> list = buildKeys();
    return list;
}

QByteArray liveFrame(const QList<QColor> &colors) {
    QByteArray frame(PacketSize * LivePages, '\0');
    const auto &k = keys();
    for (int n = 0; n < k.size() && n < colors.size(); ++n) {
        const QColor led = toLed(colors[n], Path::LiveFrame);
        frame[n * 4] = char(k[n].index);
        frame[n * 4 + 1] = char(std::min(led.red(), 0xFE));
        frame[n * 4 + 2] = char(std::min(led.green(), 0xFE));
        frame[n * 4 + 3] = char(std::min(led.blue(), 0xFE));
    }
    return frame;
}

QString udevRule() {
    return QStringLiteral(R"(SUBSYSTEM=="hidraw", ATTRS{idVendor}=="05ac", ATTRS{idProduct}=="024f", )"
                          R"(ATTRS{manufacturer}=="hfd.cn", TAG+="uaccess")");
}

}  // namespace Protocol
