#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include "animations.h"
#include "backend.h"
#include "keyboard.h"
#include "keymap.h"
#include "macros.h"
#include "protocol.h"
#include "theme.h"
#include "translator.h"

class Gmk67Tests : public QObject {
    Q_OBJECT

private slots:
    // The page layout aloran-dev/gmk67-rgb worked out from the vendor driver.
    void effectPageLayout() {
        Protocol::Effect e;
        e.value = 0x07;
        e.color = QColor(255, 0, 0);
        e.random = true;
        e.brightness = 15;
        e.speed = 3;
        e.direction = 1;
        const QByteArray page = e.page();
        QCOMPARE(page.size(), 64);
        QCOMPARE(quint8(page[0]), quint8(0x07));
        QCOMPARE(quint8(page[1]), quint8(255));
        QCOMPARE(quint8(page[8]), quint8(1));
        QCOMPARE(quint8(page[9]), quint8(16));
        QCOMPARE(quint8(page[10]), quint8(4));
        QCOMPARE(quint8(page[11]), quint8(1));
        QCOMPARE(page.mid(14, 2), QByteArray::fromHex("aa55"));
        QCOMPARE(page.mid(16), QByteArray(48, '\0'));
    }

    void effectPageRoundTrips() {
        Protocol::Effect e;
        e.value = 0x0B;
        e.color = QColor(255, 128, 0);
        e.brightness = 9;
        e.speed = 12;
        const Protocol::Effect back = Protocol::Effect::fromPage(e.page());
        QCOMPARE(back.value, 0x0B);
        QCOMPARE(back.brightness, 9);
        QCOMPARE(back.speed, 12);
        // The correction is undone on read, within rounding.
        QVERIFY(qAbs(back.color.green() - 128) <= 3);
        QCOMPARE(back.color.red(), 255);
    }

    // Calibrated on the keyboard: orange only looks orange in a live frame
    // with a raw green near 14, and pure colors keep full strength.
    void liveColorCorrection() {
        using Protocol::Path;
        const int green = Protocol::toLed(QColor(255, 128, 0), Path::LiveFrame).green();
        QVERIFY2(green >= 13 && green <= 16, qPrintable(QString::number(green)));
        QCOMPARE(Protocol::toLed(QColor(0, 255, 0), Path::LiveFrame), QColor(0, 255, 0));
        QCOMPARE(Protocol::toLed(QColor(0, 0, 255), Path::LiveFrame), QColor(0, 0, 255));
        QCOMPARE(Protocol::toLed(Qt::black, Path::LiveFrame), QColor(0, 0, 0));
    }

    void keyMapCoversTheBoard() {
        const auto &keys = Protocol::keys();
        QCOMPARE(keys.size(), 66);
        QSet<int> indices;
        for (const auto &k : keys) {
            indices.insert(k.index);
            QVERIFY(k.x - k.width / 2 >= -0.01 && k.x + k.width / 2 <= Protocol::LayoutWidth + 0.01);
            QVERIFY(k.y >= 0 && k.y <= 4);
        }
        QCOMPARE(indices.size(), 66);
        // The status LEDs next to Fn can't be set, so they're not in the map.
        QVERIFY(!indices.contains(97) && !indices.contains(98));
        // Packed in row order: Esc first, Right last.
        QCOMPARE(keys.first().index, 1);
        QCOMPARE(keys.last().index, 102);
    }

    void liveFramePacksKeysInOrder() {
        QList<QColor> colors(Protocol::keys().size(), QColor(255, 255, 255));
        colors[0] = QColor(255, 0, 0);
        const QByteArray frame = Protocol::liveFrame(colors);
        QCOMPARE(frame.size(), 512);
        QCOMPARE(frame.mid(0, 4), QByteArray::fromHex("01fe0000"));  // Esc, red capped at fe
        QCOMPARE(quint8(frame[4]), quint8(20));                      // then "1"
        QCOMPARE(frame.mid(66 * 4), QByteArray(512 - 66 * 4, '\0'));
    }

    void modesHaveUniqueIdsAndSlugs() {
        QSet<int> values;
        QSet<QString> slugs;
        for (const auto &m : Protocol::modes()) {
            values.insert(m.value);
            slugs.insert(m.slug);
        }
        QCOMPARE(values.size(), Protocol::modes().size());
        QCOMPARE(slugs.size(), Protocol::modes().size());
        QCOMPARE(Protocol::findMode(QString("flames"))->value, Protocol::SoftwareBase);
        QVERIFY(Protocol::isAnimated(Protocol::findMode(QString("plasma"))->value));
        QVERIFY(!Protocol::isAnimated(Protocol::findMode(QString("wave"))->value));
    }

    // Only the interface with the 64-byte feature report is the control one;
    // a real Apple keyboard (same IDs, no such report) is left alone.
    void findsOnlyTheControlInterface() {
        QTemporaryDir dir;
        const QByteArray control = QByteArray::fromHex("05010906a101") + QByteArray::fromHex("95407508b102c0");
        addHidraw(dir.path(), "hidraw3", "HID_ID=0003:000005AC:0000024F\n", QByteArray::fromHex("05010906a101c0"));
        addHidraw(dir.path(), "hidraw4", "HID_ID=0003:000005AC:0000024F\n", control);
        addHidraw(dir.path(), "hidraw5", "HID_ID=0003:00001234:00005678\n", control);
        QCOMPARE(Keyboard::find(dir.path(), "/dev"), QString("/dev/hidraw4"));

        QTemporaryDir none;
        addHidraw(none.path(), "hidraw0", "HID_ID=0003:000005AC:0000024F\n", QByteArray::fromHex("05010906a101c0"));
        QVERIFY(Keyboard::find(none.path(), "/dev").isEmpty());
        // The same IDs without the control interface: the dongle, or
        // Bluetooth (bus 0005).
        QVERIFY(Keyboard::wirelessPresent(none.path()));
        QVERIFY(!Keyboard::wirelessPresent(dir.path()));
        QTemporaryDir bluetooth;
        addHidraw(bluetooth.path(), "hidraw1", "HID_ID=0005:000005AC:0000024F\n", QByteArray::fromHex("05010906a101c0"));
        QVERIFY(Keyboard::wirelessPresent(bluetooth.path()));
        QTemporaryDir other;
        addHidraw(other.path(), "hidraw0", "HID_ID=0003:00001234:00005678\n", control);
        QVERIFY(!Keyboard::wirelessPresent(other.path()));
    }

    void everyAnimationFillsEveryKey() {
        for (const auto &m : Protocol::modes()) {
            if (!Protocol::isAnimated(m.value))
                continue;
            auto animation = Animation::create(m.value, 8, QColor(0, 255, 64), 1);
            QVERIFY2(animation, m.slug);
            for (int i = 0; i < 90; ++i)
                QCOMPARE(animation->frame(i / 30.0, 1 / 30.0).size(), Protocol::keys().size());
        }
        QVERIFY(!Animation::create(0x07, 8, Qt::white));
    }

    // Fire burns at the bottom and fades towards the top row.
    void flamesAreHotterAtTheBottom() {
        auto flames = Animation::create(Protocol::SoftwareBase, 8, Qt::white, 7);
        double top = 0, bottom = 0;
        for (int i = 0; i < 300; ++i) {
            const QList<QColor> frame = flames->frame(i / 30.0, 1 / 30.0);
            if (i < 60)
                continue;
            for (int k = 0; k < frame.size(); ++k) {
                const double light = frame[k].red() + frame[k].green() + frame[k].blue();
                if (Protocol::keys()[k].y == 0)
                    top += light;
                if (Protocol::keys()[k].y == 4)
                    bottom += light;
            }
        }
        QVERIFY(bottom > 4 * top);
    }

    void dimmingScalesBrightness() {
        const QList<QColor> full{QColor(255, 128, 0)};
        QCOMPARE(dimmed(full, 15).first(), QColor(255, 128, 0));
        QCOMPARE(dimmed(full, 7).first(), QColor(128, 64, 0));
    }

    // Settings are kept per effect, and survive a restart.
    void profileKeepsSettingsPerEffect() {
        QTemporaryDir dir;
        {
            Backend backend(dir.path(), false);
            backend.setMode(0x07);
            backend.setColor(QColor("#123456"));
            backend.setSpeed(3);
            backend.setMode(0x0B);
            backend.setBrightness(5);
            QCOMPARE(backend.settings(0x07).speed, 3);
            backend.setMode(Protocol::SoftwareBase + 1);
            // Saved shortly after each change and when the backend goes away.
        }
        QVERIFY(QFile::exists(dir.filePath("profile.json")));
        Backend again(dir.path(), false);
        QCOMPARE(again.mode(), Protocol::SoftwareBase + 1);
        QCOMPARE(again.settings(0x07).color, QColor("#123456"));
        QCOMPARE(again.settings(0x07).speed, 3);
        QCOMPARE(again.settings(0x0B).brightness, 5);
    }

    // bitten-security's layout: four bytes per key at its light index * 4,
    // [2, 0, usage, 0] or [3, lo, hi, 0], AA 55 at the end, zeros elsewhere.
    void keymapTableMatchesTheVerifiedLayout() {
        const QByteArray empty = Keymap::table({});
        QCOMPARE(empty.size(), 576);
        QCOMPARE(empty.left(574), QByteArray(574, '\0'));
        QCOMPARE(empty.right(2), QByteArray::fromHex("aa55"));

        const int f5 = Keymap::code(Keymap::Key, 0x3E), volumeUp = Keymap::code(Keymap::Media, 0xE9);
        const int calculator = Keymap::code(Keymap::Media, 0x192);
        const QByteArray t = Keymap::table({{1, f5}, {20, volumeUp}, {21, calculator}, {Keymap::FnLightIndex, f5}});
        QCOMPARE(t.mid(4, 4), QByteArray::fromHex("02003e00"));
        QCOMPARE(t.mid(80, 4), QByteArray::fromHex("03e90000"));
        QCOMPARE(t.mid(84, 4), QByteArray::fromHex("03920100"));
        // Fn is never remapped, or its layer would be out of reach.
        QCOMPARE(t.mid(Keymap::FnLightIndex * 4, 4), QByteArray(4, '\0'));
        // An unknown code is left out rather than sent.
        QCOMPARE(Keymap::table({{1, 0x20099}}).mid(4, 4), QByteArray(4, '\0'));
    }

    void everyKeymapFunctionIsUnique() {
        QSet<int> codes;
        for (const auto &f : Keymap::functions()) {
            QVERIFY2(!codes.contains(f.code), f.name);
            codes.insert(f.code);
            QVERIFY(Keymap::entry(f.code).size() == 4);
        }
        QVERIFY(Keymap::find(Keymap::code(Keymap::Key, 0x35)));  // grave, for Super+`
    }

    // Edits stay pending until saved; only what was written goes in the profile.
    void remapsArePendingUntilSaved() {
        QTemporaryDir dir;
        const int esc = 0;  // keys()[0] is Esc
        const int f5 = Keymap::code(Keymap::Key, 0x3E);
        {
            Backend backend(dir.path(), false);
            QVERIFY(!backend.keymapDirty());
            backend.remapKey(esc, f5);
            QVERIFY(backend.keymapDirty());
            QCOMPARE(backend.remaps().at(esc).toInt(), f5);
            backend.discardKeymap();
            QVERIFY(!backend.keymapDirty());
            QCOMPARE(backend.remaps().at(esc).toInt(), 0);
            // Fn can't be picked.
            for (int i = 0; i < Protocol::keys().size(); ++i)
                if (Protocol::keys()[i].index == Keymap::FnLightIndex)
                    QVERIFY(!backend.remappable(i));
        }
        write(dir.filePath("profile.json"), R"({"keymap": {"1": 131134, "96": 131134, "2": 7}})");
        Backend loaded(dir.path(), false);
        QCOMPARE(loaded.remaps().at(esc).toInt(), f5);  // Esc is light index 1
        QVERIFY(!loaded.keymapDirty());
        QCOMPARE(loaded.remaps().count(QVariant(0)), Protocol::keys().size() - 1);
        loaded.resetAllKeys();
        QVERIFY(loaded.keymapDirty());
    }

    // Ctrl + C: four events with the pause between each, as bitten-security
    // builds them: [usage, B0 / 30] and [lo, hi, 0, 50, 0, 0].
    void comboMacroMatchesTheVerifiedLayout() {
        Macros::Macro macro;
        macro.steps.append({Macros::Step::Combo, Macros::Ctrl, 0x06, {}});
        macro.delay = 10;
        const QByteArray body = Macros::body(macro);
        QCOMPARE(body.toHex(), QByteArray("07000000000000000000"
                                          "e0b0" "0a0000500000" "06b0" "0a0000500000"
                                          "0630" "0a0000500000" "e030"));
        const QByteArray area = Macros::area({macro, macro});
        QCOMPARE(area.size(), 576);
        QCOMPARE(quint8(area[0]) | quint8(area[1]) << 8, 400);
        QCOMPARE(quint8(area[2]) | quint8(area[3]) << 8, 400 + body.size());
        QCOMPARE(area.mid(400, body.size()), body);
        QCOMPARE(area.right(2), QByteArray::fromHex("aa55"));
    }

    void textMacroTypesWithShift() {
        Macros::Macro macro;
        macro.steps.append({Macros::Step::Text, 0, 0, "aB"});
        const auto events = Macros::events(macro);
        QCOMPARE(events.size(), 6);  // a down/up, Shift down, b down/up, Shift up
        QCOMPARE(events[2].usage, 0xE1);
        QCOMPARE(events[3].usage, 0x05);
        QVERIFY(Macros::typeable("Hello, world!"));
        QVERIFY(!Macros::typeable("ção"));
    }

    void macrosThatDontFitAreRefused() {
        Macros::Macro macro;
        macro.steps.append({Macros::Step::Text, 0, 0, "abcdefghijklmnop"});  // 32 events, 63 items
        QVERIFY(Macros::used({macro}) > Macros::Space);
        QVERIFY(Macros::area({macro}).isEmpty());
    }

    // Deleting a macro frees keys given it and moves later ones up a slot.
    void removingAMacroFollowsTheKeys() {
        QTemporaryDir dir;
        Backend backend(dir.path(), false);
        QCOMPARE(backend.addMacro(), 0);
        QCOMPARE(backend.addMacro(), 1);
        backend.remapKey(0, Keymap::code(Keymap::Macro, 0));
        backend.remapKey(1, Keymap::code(Keymap::Macro, 1));
        backend.remapKey(2, Keymap::code(Keymap::Macro, 5));  // no such macro
        QCOMPARE(backend.remaps().at(2).toInt(), 0);
        backend.removeMacro(0);
        QCOMPARE(backend.remaps().at(0).toInt(), 0);
        QCOMPARE(backend.remaps().at(1).toInt(), Keymap::code(Keymap::Macro, 0));
        QCOMPARE(Keymap::table({{2, Keymap::code(Keymap::Macro, 1)}}).mid(8, 4), QByteArray::fromHex("06000100"));
    }

    void directionsFollowTheEffect() {
        QTemporaryDir dir;
        Backend backend(dir.path(), false);
        backend.setMode(0x0A);  // Scrolling: up and down
        QCOMPARE(backend.directions().size(), 2);
        QCOMPARE(backend.directions().first().toMap().value("value").toInt(), 2);
        backend.setMode(0x07);
        QVERIFY(backend.directions().isEmpty());
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.colors().contains("selectionText"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Apply").isNull());
    }

    void bundledCatalogTranslatesTheApp() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        Translator translator(dir.path() + "/language", QFINDTESTDATA("../src/i18n"));
        QCOMPARE(translator.translate("Main", "Apply"), QString("Aplicar"));
        QCOMPARE(translator.translate("Protocol", "Flames"), QString("Chamas"));
    }

    // Every string the app shows has a Brazilian Portuguese translation.
    void catalogCoversEveryString() {
        QFile catalog(QFINDTESTDATA("../src/i18n/pt-BR.json"));
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const QJsonObject strings = QJsonDocument::fromJson(catalog.readAll()).object();
        const QDir src(QFINDTESTDATA("../src"));
        // Not a raw string: moc does not parse those.
        static const QRegularExpression call(
            "(?:qsTr|\\btr|QT_TR_NOOP|QT_TRANSLATE_NOOP\\(\"\\w+\",)\\s*\\(?\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
        QStringList missing;
        for (const QString &name : src.entryList({"*.qml", "*.cpp"})) {
            QFile file(src.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            auto it = call.globalMatch(QString::fromUtf8(file.readAll()));
            while (it.hasNext()) {
                const QString text = it.next().captured(1).replace("\\\"", "\"");
                if (!strings.contains(text))
                    missing << name + ": " + text;
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join("\n")));
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static void addHidraw(const QString &root, const QString &name, const QByteArray &uevent, const QByteArray &descriptor) {
        QDir(root).mkpath(name + "/device");
        write(root + "/" + name + "/device/uevent", uevent);
        write(root + "/" + name + "/device/report_descriptor", descriptor);
    }
};

QTEST_MAIN(Gmk67Tests)
#include "gmk67_tests.moc"
