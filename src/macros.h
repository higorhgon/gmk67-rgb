#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// Macros stored on the keyboard, from bitten-security/zuoya-gmk67's notes:
// written with 04 19, 04 15 … [8]=09, nine pages and 04 02. The 576-byte
// area holds a little-endian u16 offset per slot in bytes 0..399 (0 = no
// macro) and the bodies in 400..573, before AA 55: 174 bytes for all of
// them. A body is [items, 0 × 9] and then its items, key events
// [usage, B0 down / 30 up] and delays [lo, hi, 0, 50, 0, 0] in ms, which
// must alternate or a key stays held.
namespace Macros {

constexpr int AreaSize = 576;
constexpr int Pages = AreaSize / 64;
constexpr int BodiesStart = 400, BodiesEnd = 574;
constexpr int Space = BodiesEnd - BodiesStart;
constexpr int MinDelay = 10, MaxDelay = 1000;

// Modifier bits of a combo, held around its key.
enum Modifier { Ctrl = 1, Shift = 2, Alt = 4, Super = 8 };

struct Step {
    enum Type { Combo, Text };
    Type type = Combo;
    int modifiers = 0;  // Combo: Modifier bits
    int usage = 0;      // Combo: HID keyboard usage
    QString text;       // Text: typed as on a US layout
    bool operator==(const Step &) const = default;
};

struct Macro {
    QString name;
    QList<Step> steps;
    int delay = MinDelay;  // between events, in ms
    bool operator==(const Macro &) const = default;
};

struct Event {
    int usage;
    bool down;
};

// The key events a macro plays. Text characters a US layout can't type
// (accents, for one) are skipped.
QList<Event> events(const Macro &macro);
QByteArray body(const Macro &macro);
// Whether text can be typed as given, character by character.
bool typeable(const QString &text);
// The whole area for macros in slot order, or empty if they don't fit.
QByteArray area(const QList<Macro> &macros);
int used(const QList<Macro> &macros);

}  // namespace Macros
