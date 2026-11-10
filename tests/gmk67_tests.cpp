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
