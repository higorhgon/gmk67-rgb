#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>

// Key remapping, from bitten-security/zuoya-gmk67's notes (verified there
// on the keyboard). The base layer and the Fn layer each have a 576-byte
// table, written with commands 04 11 and 04 27 in nine pages, holding four
// bytes per key at its light index * 4. A zero entry means the key does
// what it always does, so an empty table undoes every remap.
namespace Keymap {

constexpr int TableSize = 576;
constexpr int Pages = TableSize / 64;
constexpr int FnLightIndex = 96;  // never remapped: it reaches the Fn layer

enum Kind { Key = 2, Media = 3, Macro = 6 };

// A function a key can be given. code packs kind and usage: kind << 16 |
// usage, with a HID keyboard usage for Key and a consumer usage for Media.
struct Function {
    int code;
    const char *name;   // English; the ones worth it are translated
    const char *label;  // short, for the key cap
    const char *group;
};

constexpr int code(Kind kind, int usage) { return kind << 16 | usage; }

const QList<Function> &functions();
const Function *find(int code);
bool remappable(int lightIndex);

// A code a key can be given: a listed function, or a macro slot.
bool valid(int code);

// The 4-byte entry for a function: [2, 0, usage, 0], [3, lo, hi, 0], or
// [6, 0, slot, 0] for a macro.
QByteArray entry(int code);
// The whole base-layer table for lightIndex → code, ending in AA 55.
QByteArray table(const QHash<int, int> &remaps);

}  // namespace Keymap
