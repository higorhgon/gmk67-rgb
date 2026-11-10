#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QString>

// The lighting protocol of the Zuoya GMK67 v1 (hfd.cn HFD801KJC firmware,
// which borrows Apple's 05ac:024f). Everything here is pure data and byte
// layout, so it can be tested without the keyboard; Keyboard does the I/O.
namespace Protocol {

constexpr int PacketSize = 64;
constexpr int MaxLevel = 15;          // brightness and speed, 0..15
constexpr int SoftwareBase = 0x100;   // ids of the effects this app animates
constexpr int LivePages = 8;          // a live frame is 8 pages, 512 bytes

enum DirectionKind { NoDirection, LeftRight, UpDown };

struct Mode {
    int value;
    const char *name;     // English, translated where it is shown
    const char *slug;     // the name on the command line
    bool color;           // takes a color
    bool speed;           // takes a speed
    DirectionKind direction;
};

// The firmware's 19 effects and Off, in the order of the vendor driver's
// mode table, then the effects this app animates over USB.
const QList<Mode> &modes();
const Mode *findMode(int value);
const Mode *findMode(const QString &slug);
inline bool isAnimated(int value) { return value >= SoftwareBase; }

// Raw direction values the firmware takes: 0 left→right, 1 right→left,
// 2 bottom→top, 3 top→bottom.
QList<int> directions(DirectionKind kind);

struct Effect {
    int value = 0x01;
    QColor color = Qt::white;
    bool random = false;
    int brightness = MaxLevel;
    int speed = 10;
    int direction = 0;

    // The page written with command 04 13: mode, R, G, B, 4 zero bytes,
    // random, brightness + 1, speed + 1, direction, 2 zero bytes, AA 55.
    QByteArray page() const;
    static Effect fromPage(const QByteArray &page);
    bool operator==(const Effect &) const = default;
};

// The LEDs are not linear like a screen. Effect pages go through the
// firmware's own color handling; live frames don't, and there green is far
// stronger (orange #ff8000 only looks orange with a raw green of about 14).
enum class Path { EffectPage, LiveFrame };
QColor toLed(const QColor &color, Path path);
QColor fromLed(const QColor &color);  // undoes the effect page correction

struct Key {
    int index;            // the firmware's light index
    double x, y, width;   // center and width in key units on the 65% layout
    const char *label;
};

// The 66 lit keys in the order a live frame packs them: row by row, left to
// right. Light indices come from the vendor KeyboardLayout.xml.
const QList<Key> &keys();
constexpr double LayoutWidth = 16.0, LayoutHeight = 5.0;

// The 512 bytes of a live frame (command 04 20): [index, R, G, B] per key in
// keys() order, corrected for the live path and capped at 0xfe like the
// vendor app.
QByteArray liveFrame(const QList<QColor> &colors);

// The udev rule that lets the logged-in user open the keyboard's hidraw node.
QString udevRule();

}  // namespace Protocol
