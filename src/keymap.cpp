#include "keymap.h"

#include <QCoreApplication>
#include <algorithm>
#include <cstring>

namespace Keymap {

namespace {
QList<Function> build() {
    QList<Function> list;
    static const char *letters[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
                                    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
    for (int i = 0; i < 26; ++i)
        list.append({code(Key, 0x04 + i), letters[i], letters[i], QT_TRANSLATE_NOOP("Keymap", "Letters")});
    static const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
    for (int i = 0; i < 10; ++i)
        list.append({code(Key, 0x1E + i), digits[i], digits[i], QT_TRANSLATE_NOOP("Keymap", "Numbers")});
    static const char *fkeys[] = {"F1",  "F2",  "F3",  "F4",  "F5",  "F6",  "F7",  "F8",
                                  "F9",  "F10", "F11", "F12", "F13", "F14", "F15", "F16",
                                  "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24"};
    for (int i = 0; i < 24; ++i)
        list.append({code(Key, i < 12 ? 0x3A + i : 0x68 + i - 12), fkeys[i], fkeys[i],
                     QT_TRANSLATE_NOOP("Keymap", "Function keys")});

    const char *symbols = QT_TRANSLATE_NOOP("Keymap", "Symbols");
    list.append({code(Key, 0x2D), "-", "-", symbols});
    list.append({code(Key, 0x2E), "=", "=", symbols});
    list.append({code(Key, 0x2F), "[", "[", symbols});
    list.append({code(Key, 0x30), "]", "]", symbols});
    list.append({code(Key, 0x31), "\\", "\\", symbols});
    list.append({code(Key, 0x33), ";", ";", symbols});
    list.append({code(Key, 0x34), "'", "'", symbols});
    list.append({code(Key, 0x35), "`", "`", symbols});
    list.append({code(Key, 0x36), ",", ",", symbols});
    list.append({code(Key, 0x37), ".", ".", symbols});
    list.append({code(Key, 0x38), "/", "/", symbols});

    const char *editing = QT_TRANSLATE_NOOP("Keymap", "Editing and navigation");
    list.append({code(Key, 0x29), "Esc", "Esc", editing});
    list.append({code(Key, 0x2B), "Tab", "Tab", editing});
    list.append({code(Key, 0x28), "Enter", "Enter", editing});
    list.append({code(Key, 0x2C), QT_TRANSLATE_NOOP("Keymap", "Space"), "Space", editing});
    list.append({code(Key, 0x2A), "Backspace", "⌫", editing});
    list.append({code(Key, 0x4C), "Delete", "Del", editing});
    list.append({code(Key, 0x49), "Insert", "Ins", editing});
    list.append({code(Key, 0x4A), "Home", "Home", editing});
    list.append({code(Key, 0x4D), "End", "End", editing});
    list.append({code(Key, 0x4B), "Page Up", "PgUp", editing});
    list.append({code(Key, 0x4E), "Page Down", "PgDn", editing});
    list.append({code(Key, 0x52), QT_TRANSLATE_NOOP("Keymap", "Up"), "↑", editing});
    list.append({code(Key, 0x51), QT_TRANSLATE_NOOP("Keymap", "Down"), "↓", editing});
    list.append({code(Key, 0x50), QT_TRANSLATE_NOOP("Keymap", "Left"), "←", editing});
    list.append({code(Key, 0x4F), QT_TRANSLATE_NOOP("Keymap", "Right"), "→", editing});
    list.append({code(Key, 0x46), "Print Screen", "PrtSc", editing});
    list.append({code(Key, 0x47), "Scroll Lock", "ScrLk", editing});
    list.append({code(Key, 0x48), "Pause", "Pause", editing});
    list.append({code(Key, 0x39), "Caps Lock", "Caps", editing});
    list.append({code(Key, 0x65), QT_TRANSLATE_NOOP("Keymap", "Menu"), "Menu", editing});

    const char *modifiers = QT_TRANSLATE_NOOP("Keymap", "Modifiers");
    list.append({code(Key, 0xE0), QT_TRANSLATE_NOOP("Keymap", "Left Ctrl"), "Ctrl", modifiers});
    list.append({code(Key, 0xE1), QT_TRANSLATE_NOOP("Keymap", "Left Shift"), "Shift", modifiers});
    list.append({code(Key, 0xE2), QT_TRANSLATE_NOOP("Keymap", "Left Alt"), "Alt", modifiers});
    list.append({code(Key, 0xE3), QT_TRANSLATE_NOOP("Keymap", "Left Super"), "Super", modifiers});
    list.append({code(Key, 0xE4), QT_TRANSLATE_NOOP("Keymap", "Right Ctrl"), "Ctrl", modifiers});
    list.append({code(Key, 0xE5), QT_TRANSLATE_NOOP("Keymap", "Right Shift"), "Shift", modifiers});
    list.append({code(Key, 0xE6), QT_TRANSLATE_NOOP("Keymap", "Right Alt"), "AltGr", modifiers});
    list.append({code(Key, 0xE7), QT_TRANSLATE_NOOP("Keymap", "Right Super"), "Super", modifiers});

    // The vendor app's media list, as bitten-security captured it.
    const char *media = QT_TRANSLATE_NOOP("Keymap", "Media");
    list.append({code(Media, 0xE9), QT_TRANSLATE_NOOP("Keymap", "Volume up"), "Vol+", media});
    list.append({code(Media, 0xEA), QT_TRANSLATE_NOOP("Keymap", "Volume down"), "Vol−", media});
    list.append({code(Media, 0xE2), QT_TRANSLATE_NOOP("Keymap", "Mute"), "Mute", media});
    list.append({code(Media, 0xCD), QT_TRANSLATE_NOOP("Keymap", "Play / Pause"), "⏯", media});
    list.append({code(Media, 0xB7), QT_TRANSLATE_NOOP("Keymap", "Stop"), "⏹", media});
    list.append({code(Media, 0xB6), QT_TRANSLATE_NOOP("Keymap", "Previous track"), "⏮", media});
    list.append({code(Media, 0xB5), QT_TRANSLATE_NOOP("Keymap", "Next track"), "⏭", media});
    list.append({code(Media, 0x192), QT_TRANSLATE_NOOP("Keymap", "Calculator"), "Calc", media});
    list.append({code(Media, 0x223), QT_TRANSLATE_NOOP("Keymap", "Browser home"), "Home", media});
    list.append({code(Media, 0x224), QT_TRANSLATE_NOOP("Keymap", "Browser back"), "Back", media});
    list.append({code(Media, 0x225), QT_TRANSLATE_NOOP("Keymap", "Browser forward"), "Fwd", media});
    list.append({code(Media, 0x227), QT_TRANSLATE_NOOP("Keymap", "Browser refresh"), "Rfsh", media});
    list.append({code(Media, 0x221), QT_TRANSLATE_NOOP("Keymap", "Search"), "Find", media});

    // The usual remaps first: function keys, media, navigation, modifiers.
    static const char *order[] = {"Function keys", "Media", "Editing and navigation", "Modifiers",
                                  "Symbols", "Letters", "Numbers"};
    auto rank = [](const char *group) {
        return int(std::find_if(std::begin(order), std::end(order),
                                [group](const char *g) { return std::strcmp(g, group) == 0; })
                   - std::begin(order));
    };
    std::stable_sort(list.begin(), list.end(),
                     [&](const Function &a, const Function &b) { return rank(a.group) < rank(b.group); });
    return list;
}
}  // namespace

const QList<Function> &functions() {
    static const QList<Function> list = build();
    return list;
}

const Function *find(int code) {
    for (const Function &f : functions())
        if (f.code == code)
            return &f;
    return nullptr;
}

bool valid(int code) {
    return find(code) || (code >> 16 == Macro && (code & 0xFFFF) < 200);
}

bool remappable(int lightIndex) {
    return lightIndex != FnLightIndex && lightIndex >= 0 && lightIndex * 4 + 4 <= TableSize - 2;
}

QByteArray entry(int code) {
    const int kind = code >> 16, usage = code & 0xFFFF;
    QByteArray e(4, '\0');
    e[0] = char(kind);
    if (kind == Media) {
        e[1] = char(usage & 0xFF);
        e[2] = char(usage >> 8);
    } else {
        e[2] = char(usage);
    }
    return e;
}

QByteArray table(const QHash<int, int> &remaps) {
    QByteArray t(TableSize, '\0');
    for (auto it = remaps.begin(); it != remaps.end(); ++it)
        if (remappable(it.key()) && valid(it.value()))
            t.replace(it.key() * 4, 4, entry(it.value()));
    t[TableSize - 2] = char(0xAA);
    t[TableSize - 1] = char(0x55);
    return t;
}

}  // namespace Keymap
