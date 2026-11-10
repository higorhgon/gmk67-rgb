// The command-line side follows Hype's src/cli.cpp: plain text or --json on
// stdout, errors on stderr, exit 0 or 1.
#include "cli.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QTextStream>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "animations.h"
#include "keyboard.h"
#include "keymap.h"
#include "protocol.h"
#include "streamer.h"

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

int fail(const QString &message) {
    err() << "gmk67: " << message << Qt::endl;
    return 1;
}

QJsonObject modeJson(const Protocol::Mode &mode) {
    return {{"name", mode.slug},
            {"title", QCoreApplication::translate("Protocol", mode.name)},
            {"animated", Protocol::isAnimated(mode.value)},
            {"color", mode.color},
            {"speed", mode.speed},
            {"directions", mode.direction == Protocol::LeftRight ? "left-right"
                           : mode.direction == Protocol::UpDown  ? "up-down" : ""}};
}

int list(bool json) {
    QJsonArray array;
    for (const auto &mode : Protocol::modes()) {
        if (json)
            array.append(modeJson(mode));
        else
            out() << mode.slug << (Protocol::isAnimated(mode.value) ? "  (animated)" : "") << Qt::endl;
    }
    if (json)
        out() << QJsonDocument(array).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}

int status(bool json) {
    Keyboard keyboard;
    Protocol::Effect effect;
    const bool ok = keyboard.readEffect(&effect);
    const Protocol::Mode *mode = ok ? Protocol::findMode(effect.value) : nullptr;
    if (json) {
        QJsonObject o{{"connected", ok}};
        if (!ok)
            o.insert("error", keyboard.errorString());
        if (mode)
            o.insert("effect", QJsonObject{{"name", mode->slug},
                                           {"color", effect.color.name()},
                                           {"random", effect.random},
                                           {"brightness", effect.brightness},
                                           {"speed", effect.speed},
                                           {"direction", effect.direction}});
        out() << QJsonDocument(o).toJson(QJsonDocument::Compact) << Qt::endl;
        return ok ? 0 : 1;
    }
    if (!ok)
        return fail(keyboard.errorString());
    out() << "connected  " << Keyboard::find() << Qt::endl
          << "effect     " << (mode ? mode->slug : "unknown") << Qt::endl
          << "color      " << effect.color.name() << (effect.random ? " (random)" : "") << Qt::endl
          << "brightness " << effect.brightness << Qt::endl
          << "speed      " << effect.speed << Qt::endl;
    return 0;
}

// Ctrl+C and SIGTERM end "animate" through the event loop, so the streamer
// stops cleanly and the keyboard falls back to its own effect.
int signalPipe[2];
void onSignal(int) { [[maybe_unused]] auto n = ::write(signalPipe[1], "x", 1); }

int animate(const Protocol::Mode &mode, const Protocol::Effect &effect, int fps) {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalPipe) != 0)
        return fail("can't watch for signals");
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    QSocketNotifier notifier(signalPipe[0], QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, qApp, &QCoreApplication::quit);

    Streamer streamer;
    QObject::connect(&streamer, &Streamer::reachable, qApp, [](bool ok, int, const QString &message) {
        if (ok)
            err() << "gmk67: playing; Ctrl+C stops" << Qt::endl;
        else
            err() << "gmk67: " << message << Qt::endl;
    });
    streamer.play(Animation::create(mode.value, effect.speed, effect.color), effect.brightness, fps);
    const int code = QCoreApplication::exec();
    streamer.stop();
    return code;
}

// Gives every key its own function back, on both layers, and forgets the
// remaps in the profile so the window agrees. Works with only a mouse and a
// terminal, for when a remap left the keyboard hard to type on.
int resetKeys(bool json) {
    Keyboard keyboard;
    if (!keyboard.writeKeymap(Keymap::table({})))
        return fail(keyboard.errorString());
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/gmk67/profile.json";
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        root.remove("keymap");
        QSaveFile save(path);
        if (save.open(QIODevice::WriteOnly)) {
            save.write(QJsonDocument(root).toJson());
            save.commit();
        }
    }
    if (json)
        out() << QJsonDocument(QJsonObject{{"keys", "reset"}}).toJson(QJsonDocument::Compact) << Qt::endl;
    else
        out() << "every key is back to its own function" << Qt::endl;
    return 0;
}

}  // namespace

bool Cli::wantsWindow(const QStringList &args) {
    return args.size() < 2 || args.at(1) == "open";
}

int Cli::run(const QStringList &args) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Lighting for the Zuoya GMK67 keyboard (wired USB).\n\n"
                                     "Commands:\n"
                                     "  open                 the window (also with no command)\n"
                                     "  list                 every effect, animated ones marked\n"
                                     "  status               the keyboard and its current effect\n"
                                     "  set <effect>         save a keyboard effect\n"
                                     "  animate <effect>     play an animated effect until stopped\n"
                                     "  keys reset           undo every key remap");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "open, list, status, set, animate, or keys");
    QCommandLineOption json("json", "Machine-readable output.");
    QCommandLineOption color("color", "Color as #rrggbb.", "color");
    QCommandLineOption random("random", "Random colors (keyboard effects that take a color).");
    QCommandLineOption brightness("brightness", "0 to 15.", "level", "15");
    QCommandLineOption speed("speed", "0 to 15.", "level", "10");
    QCommandLineOption direction("direction", "left-right, right-left, bottom-top, or top-bottom.", "direction");
    QCommandLineOption fps("fps", "Frames a second for animate.", "fps", "60");
    parser.addOptions({json, color, random, brightness, speed, direction, fps});
    parser.process(args);

    const QStringList positional = parser.positionalArguments();
    const QString command = positional.value(0);
    if (command == "list")
        return list(parser.isSet(json));
    if (command == "status")
        return status(parser.isSet(json));
    if (command == "keys") {
        if (positional.value(1) != "reset")
            return fail("keys takes reset; see gmk67 --help");
        return resetKeys(parser.isSet(json));
    }
    if (command != "set" && command != "animate")
        return fail(QString("unknown command \"%1\"; see gmk67 --help").arg(command));

    const Protocol::Mode *mode = Protocol::findMode(positional.value(1));
    if (!mode)
        return fail(QString("unknown effect \"%1\"; see gmk67 list").arg(positional.value(1)));
    if ((command == "animate") != Protocol::isAnimated(mode->value))
        return fail(command == "set" ? QString("%1 is animated; use gmk67 animate %1").arg(mode->slug)
                                     : QString("%1 is a keyboard effect; use gmk67 set %1").arg(mode->slug));

    Protocol::Effect effect;
    effect.value = mode->value;
    if (parser.isSet(color)) {
        effect.color = QColor(parser.value(color));
        if (!effect.color.isValid())
            return fail("--color takes #rrggbb");
    }
    effect.random = parser.isSet(random);
    bool ok1 = false, ok2 = false;
    effect.brightness = parser.value(brightness).toInt(&ok1);
    effect.speed = parser.value(speed).toInt(&ok2);
    if (!ok1 || !ok2 || effect.brightness < 0 || effect.brightness > 15 || effect.speed < 0 || effect.speed > 15)
        return fail("--brightness and --speed take 0 to 15");
    const QStringList directionNames{"left-right", "right-left", "bottom-top", "top-bottom"};
    const QList<int> allowed = Protocol::directions(mode->direction);
    effect.direction = allowed.value(0, 0);
    if (parser.isSet(direction)) {
        const int value = directionNames.indexOf(parser.value(direction));
        if (!allowed.contains(value))
            return fail(QString("%1 takes no direction \"%2\"").arg(mode->slug, parser.value(direction)));
        effect.direction = value;
    }

    if (command == "animate")
        return animate(*mode, effect, std::max(parser.value(fps).toInt(), 1));

    Keyboard keyboard;
    if (!keyboard.setEffect(effect))
        return fail(keyboard.errorString());
    if (parser.isSet(json))
        out() << QJsonDocument(QJsonObject{{"applied", mode->slug}}).toJson(QJsonDocument::Compact) << Qt::endl;
    return 0;
}
