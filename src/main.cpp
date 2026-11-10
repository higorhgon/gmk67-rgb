// gmk67: the lights of the Zuoya GMK67 keyboard, for Omarchy.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

// The desktop's interface font, e.g. "Adwaita Sans 11", from the settings
// portal, as Hype does; without it Qt's default font stays.
static void adoptDesktopFont() {
    auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                               "org.freedesktop.portal.Settings", "ReadOne");
    call.setArguments({"org.gnome.desktop.interface", "font-name"});
    const auto reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 500);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;
    const QString name = reply.arguments().first().value<QDBusVariant>().variant().toString();
    const int space = name.lastIndexOf(' ');
    const double size = name.mid(space + 1).toDouble();
    if (space <= 0 || size <= 0)
        return;
    QFont font(name.left(space));
    font.setPointSizeF(size);
    QGuiApplication::setFont(font);
}

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    // Commands need no display.
    if (!Cli::wantsWindow(args)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("gmk67");
        app.setApplicationVersion(GMK67_VERSION);
        Translator translator;
        app.installTranslator(&translator);
        return Cli::run(app.arguments());
    }

    // The app draws every control in the Omarchy theme's colors itself; the
    // gtk3 platform theme would only bring GTK's own look and font rules.
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("gmk67");
    app.setApplicationVersion(GMK67_VERSION);

    // Matches the window to gmk67.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("gmk67");
    app.setWindowIcon(QIcon::fromTheme("gmk67"));

    adoptDesktopFont();
    // The plain Basic style, restyled in QML with the theme's palette, the
    // way Omarchy's own apps (Hype) look.
    QQuickStyle::setStyle("Basic");
    // Text drawn by FreeType with the desktop's hinting, like every other app.
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
