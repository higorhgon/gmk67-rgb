#include "macros.h"

#include <QHash>
#include <algorithm>

namespace Macros {

namespace {
// A character → its usage on a US layout, and whether Shift is needed.
struct Typed {
    int usage;
    bool shift;
};

const QHash<QChar, Typed> &usTable() {
    static const QHash<QChar, Typed> table = [] {
        QHash<QChar, Typed> t;
        for (int i = 0; i < 26; ++i) {
            t.insert(QChar('a' + i), {0x04 + i, false});
            t.insert(QChar('A' + i), {0x04 + i, true});
        }
        const QString digits = "1234567890", shifted = "!@#$%^&*()";
        for (int i = 0; i < 10; ++i) {
            t.insert(digits[i], {0x1E + i, false});
            t.insert(shifted[i], {0x1E + i, true});
        }
        const QString plain = " -=[]\;'`,./", upper = " _+{}|:\"~<>?";
        const int usages[] = {0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
        for (int i = 0; i < plain.size(); ++i) {
            t.insert(plain[i], {usages[i], false});
            if (i > 0)
                t.insert(upper[i], {usages[i], true});
        }
        t.insert(QChar('\n'), {0x28, false});
        t.insert(QChar('\t'), {0x2B, false});
        return t;
    }();
    return table;
}

constexpr int ModifierUsages[][2] = {{Ctrl, 0xE0}, {Shift, 0xE1}, {Alt, 0xE2}, {Super, 0xE3}};
}  // namespace

QList<Event> events(const Macro &macro) {
    QList<Event> out;
    for (const Step &step : macro.steps) {
        if (step.type == Step::Combo) {
            if (!step.usage)
                continue;
            for (const auto &[bit, usage] : ModifierUsages)
                if (step.modifiers & bit)
                    out.append({usage, true});
            out.append({step.usage, true});
            out.append({step.usage, false});
            for (int i = 3; i >= 0; --i)
                if (step.modifiers & ModifierUsages[i][0])
                    out.append({ModifierUsages[i][1], false});
            continue;
        }
        for (const QChar c : step.text) {
            const auto it = usTable().constFind(c);
            if (it == usTable().constEnd())
                continue;
            if (it->shift)
                out.append({0xE1, true});
            out.append({it->usage, true});
            out.append({it->usage, false});
            if (it->shift)
                out.append({0xE1, false});
        }
    }
    return out;
}

QByteArray body(const Macro &macro) {
    const QList<Event> list = events(macro);
    const int delay = std::clamp(macro.delay, MinDelay, MaxDelay);
    QByteArray items;
    int count = 0;
    for (int i = 0; i < list.size(); ++i) {
        if (i > 0) {
            items.append(char(delay & 0xFF)).append(char(delay >> 8)).append('\0').append(char(0x50)).append(2, '\0');
            ++count;
        }
        items.append(char(list[i].usage)).append(char(list[i].down ? 0xB0 : 0x30));
        ++count;
    }
    QByteArray header(10, '\0');
    header[0] = char(count & 0xFF);
    return header + items;
}

bool typeable(const QString &text) {
    for (const QChar c : text)
        if (!usTable().contains(c))
            return false;
    return true;
}

int used(const QList<Macro> &macros) {
    int total = 0;
    for (const Macro &m : macros)
        total += body(m).size();
    return total;
}

QByteArray area(const QList<Macro> &macros) {
    QByteArray a(AreaSize, '\0');
    int pos = BodiesStart;
    for (int slot = 0; slot < macros.size(); ++slot) {
        const QByteArray b = body(macros[slot]);
        if (pos + b.size() > BodiesEnd || slot * 2 + 2 > BodiesStart)
            return {};
        a[slot * 2] = char(pos & 0xFF);
        a[slot * 2 + 1] = char(pos >> 8);
        a.replace(pos, b.size(), b);
        pos += b.size();
    }
    a[AreaSize - 2] = char(0xAA);
    a[AreaSize - 1] = char(0x55);
    return a;
}

}  // namespace Macros
