#include "backend.h"

#include <QClipboard>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

namespace {
const QList<QColor> Presets = {QColor(255, 0, 0),   QColor(255, 128, 0), QColor(255, 255, 0), QColor(0, 255, 0),
                               QColor(0, 255, 255), QColor(0, 0, 255),   QColor(128, 0, 255), QColor(255, 255, 255)};
}  // namespace

Backend::Backend(const QString &configDirectory, bool probe, QObject *parent)
    : QObject(parent),
      m_configDirectory(configDirectory.isEmpty()
                            ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/gmk67"
                            : configDirectory),
      m_probe(probe), m_recent(Presets) {
    // Until the first look, say what will be needed rather than nothing; the
    // same words as Keyboard's, so one translation covers both.
    m_statusMessage = QCoreApplication::translate(
        "Keyboard", "Connect the GMK67 with its USB cable. The dongle and Bluetooth can't change the lights.");
    for (const auto &mode : Protocol::modes()) {
        Protocol::Effect e;
        e.value = mode.value;
        if (!Protocol::directions(mode.direction).isEmpty())
            e.direction = Protocol::directions(mode.direction).first();
        m_effects.insert(mode.value, e);
    }
    load();

    connect(&m_streamer, &Streamer::frame, this, [this](const QList<QColor> &colors) {
        m_preview = colors;
        emit previewChanged();
    });
    connect(&m_streamer, &Streamer::reachable, this, [this](bool ok, int status, const QString &message) {
        setStatus(ok ? Ready : status, message);
    });
    connect(&m_streamer, &QThread::finished, this, &Backend::activeChanged);
    connect(&m_applyWatcher, &QFutureWatcher<QPair<bool, QPair<int, QString>>>::finished, this, [this] {
        const auto result = m_applyWatcher.result();
        m_busy = false;
        emit busyChanged();
        if (result.first) {
            m_activeMode = m_mode;
            setStatus(Ready, {});
            emit activeChanged();
            emit notice(tr("Applied"));
        } else {
            setStatus(result.second.first, result.second.second);
        }
    });

    // Changes are kept a moment after they stop, so a dragged slider writes
    // the profile once.
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::save);

    m_previewTimer.setInterval(33);
    connect(&m_previewTimer, &QTimer::timeout, this, [this] {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_preview = dimmed(m_previewAnimation->frame((now - m_previewStart) / 1000.0, (now - m_previewLast) / 1000.0),
                           current().brightness);
        m_previewLast = now;
        emit previewChanged();
    });

    // The keyboard comes and goes with its cable; a cheap look through sysfs
    // every two seconds notices both.
    m_poll.setInterval(2000);
    connect(&m_poll, &QTimer::timeout, this, &Backend::poll);
    updateStaticPreview();
    if (m_probe) {
        refresh();
        m_poll.start();
    }
}

Backend::~Backend() {
    m_streamer.stop();
    if (m_saveTimer.isActive())
        save();
}

QVariantList Backend::modes() const {
    QVariantList list;
    for (const auto &mode : Protocol::modes())
        list.append(QVariantMap{{"value", mode.value},
                                {"name", QCoreApplication::translate("Protocol", mode.name)},
                                {"animated", Protocol::isAnimated(mode.value)}});
    return list;
}

QVariantList Backend::keys() const {
    QVariantList list;
    for (const auto &key : Protocol::keys())
        list.append(QVariantMap{{"x", key.x}, {"y", key.y}, {"width", key.width}, {"label", QString::fromUtf8(key.label)}});
    return list;
}

void Backend::setMode(int value) {
    if (value == m_mode || !Protocol::findMode(value))
        return;
    m_mode = value;
    changed();
}

QVariantMap Backend::modeInfo() const {
    const Protocol::Mode *mode = Protocol::findMode(m_mode);
    if (!mode)
        return {};
    return {{"value", mode->value},
            {"name", QCoreApplication::translate("Protocol", mode->name)},
            {"color", mode->color},
            {"speed", mode->speed},
            {"brightness", mode->value != 0x00},
            {"random", mode->color && !Protocol::isAnimated(mode->value)},
            {"animated", Protocol::isAnimated(mode->value)}};
}

Protocol::Effect &Backend::current() { return m_effects[m_mode]; }
const Protocol::Effect &Backend::current() const { return *m_effects.constFind(m_mode); }

void Backend::changed() {
    updateStaticPreview();
    m_saveTimer.start();
    emit settingsChanged();
}

void Backend::setColor(const QColor &color) {
    if (!color.isValid() || color.rgb() == current().color.rgb())
        return;
    current().color = color.toRgb();
    changed();
}

void Backend::setRandom(bool random) {
    if (random == current().random)
        return;
    current().random = random;
    changed();
}

void Backend::setBrightness(int value) {
    value = std::clamp(value, 0, Protocol::MaxLevel);
    if (value == current().brightness)
        return;
    current().brightness = value;
    changed();
}

void Backend::setSpeed(int value) {
    value = std::clamp(value, 0, Protocol::MaxLevel);
    if (value == current().speed)
        return;
    current().speed = value;
    changed();
}

void Backend::setDirection(int value) {
    if (value == current().direction)
        return;
    current().direction = value;
    changed();
}

QVariantList Backend::directions() const {
    const Protocol::Mode *mode = Protocol::findMode(m_mode);
    QVariantList list;
    if (!mode)
        return list;
    for (int value : Protocol::directions(mode->direction)) {
        static const char *labels[] = {QT_TR_NOOP("Left to right"), QT_TR_NOOP("Right to left"),
                                       QT_TR_NOOP("Bottom to top"), QT_TR_NOOP("Top to bottom")};
        list.append(QVariantMap{{"value", value}, {"label", tr(labels[value])}});
    }
    return list;
}

QVariantList Backend::recentColors() const {
    QVariantList list;
    for (const QColor &c : m_recent)
        list.append(c);
    return list;
}

QString Backend::udevCommand() const {
    return QStringLiteral("echo '%1' | sudo tee /etc/udev/rules.d/70-gmk67.rules\n"
                          "sudo udevadm control --reload-rules\n"
                          "sudo udevadm trigger --subsystem-match=hidraw")
        .arg(Protocol::udevRule());
}

void Backend::copyUdevCommand() {
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(udevCommand());
    emit notice(tr("Commands copied"));
}

QVariantList Backend::preview() const {
    QVariantList list;
    for (const QColor &c : m_preview)
        list.append(c);
    return list;
}

// What a firmware effect roughly looks like, since only animated effects
// have frames to show: its color, or a rainbow for the multicolor ones.
void Backend::updateStaticPreview() {
    // While it plays on the keyboard, the streamer's frames are the preview.
    if (Protocol::isAnimated(m_mode) && animating() && m_activeMode == m_mode) {
        m_previewTimer.stop();
        return;
    }
    if (Protocol::isAnimated(m_mode)) {
        const Protocol::Effect &e = current();
        m_previewAnimation = Animation::create(m_mode, e.speed, e.color);
        m_previewStart = m_previewLast = QDateTime::currentMSecsSinceEpoch();
        m_previewTimer.start();
        return;
    }
    m_previewTimer.stop();
    const Protocol::Mode *mode = Protocol::findMode(m_mode);
    const Protocol::Effect &e = current();
    QList<QColor> colors;
    for (const auto &key : Protocol::keys()) {
        if (!mode || mode->value == 0x00)
            colors.append(Qt::black);
        else if (!mode->color || e.random)
            colors.append(QColor::fromHsvF(float(key.x / Protocol::LayoutWidth * 0.9), 1, 1).toRgb());
        else
            colors.append(e.color);
    }
    m_preview = dimmed(colors, mode && mode->value != 0x00 ? e.brightness : 0);
    emit previewChanged();
}

void Backend::rememberColor(const QColor &color) {
    m_recent.removeAll(color);
    m_recent.prepend(color);
    while (m_recent.size() > Presets.size())
        m_recent.removeLast();
    emit recentColorsChanged();
}

void Backend::apply() {
    if (m_busy)
        return;
    const Protocol::Mode *mode = Protocol::findMode(m_mode);
    if (!mode)
        return;
    const Protocol::Effect effect = current();
    if (mode->color && !effect.random)
        rememberColor(effect.color);
    save();

    // A running animation holds the keyboard and would paint over a firmware
    // effect a moment later, so it always stops first.
    m_streamer.stop();
    if (Protocol::isAnimated(m_mode)) {
        m_activeMode = m_mode;
        m_previewTimer.stop();
        m_streamer.play(Animation::create(m_mode, effect.speed, effect.color), effect.brightness);
        emit activeChanged();
        emit notice(tr("Animating. It runs while %1 stays open.").arg("GMK67"));
        return;
    }

    emit activeChanged();
    updateStaticPreview();
    m_busy = true;
    emit busyChanged();
    m_applyWatcher.setFuture(QtConcurrent::run([effect] {
        Keyboard keyboard;
        const bool ok = keyboard.setEffect(effect);
        return qMakePair(ok, qMakePair(int(keyboard.status()), keyboard.errorString()));
    }));
}

void Backend::stopAnimation() {
    if (!animating())
        return;
    m_streamer.stop();
    m_activeMode = -1;
    emit activeChanged();
    updateStaticPreview();
}

void Backend::setStatus(int status, const QString &message) {
    if (status == m_status && message == m_statusMessage)
        return;
    m_status = status;
    m_statusMessage = message;
    emit statusChanged();
}

void Backend::refresh() {
    // While animating, the streamer owns the keyboard and reports on it.
    if (animating() || m_busy)
        return;
    Keyboard keyboard;
    Protocol::Effect effect;
    if (!keyboard.readEffect(&effect)) {
        setStatus(keyboard.status(), keyboard.errorString());
        return;
    }
    setStatus(Ready, {});
    m_readOnce = true;
    // The keyboard's own effect wins over the profile: it is what's lit.
    if (Protocol::findMode(effect.value) && !Protocol::isAnimated(effect.value)) {
        m_effects[effect.value] = effect;
        m_activeMode = effect.value;
        m_mode = effect.value;
        emit activeChanged();
        changed();
    }
}

void Backend::poll() {
    if (animating() || m_busy)
        return;
    const bool present = !Keyboard::find().isEmpty();
    if (!present) {
        Keyboard keyboard;
        keyboard.open();
        setStatus(keyboard.status(), keyboard.errorString());
    } else if (m_status != Ready || !m_readOnce) {
        refresh();
    }
}

void Backend::retranslate() {
    emit modesChanged();
    emit settingsChanged();
}

Protocol::Effect Backend::settings(int value) const { return m_effects.value(value); }

void Backend::load() {
    QFile file(m_configDirectory + "/profile.json");
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject effects = root.value("effects").toObject();
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        bool ok = false;
        const int value = it.key().toInt(&ok);
        if (!ok || !m_effects.contains(value))
            continue;
        const QJsonObject o = it.value().toObject();
        Protocol::Effect &e = m_effects[value];
        if (QColor c(o.value("color").toString()); c.isValid())
            e.color = c;
        e.random = o.value("random").toBool(e.random);
        e.brightness = std::clamp(o.value("brightness").toInt(e.brightness), 0, Protocol::MaxLevel);
        e.speed = std::clamp(o.value("speed").toInt(e.speed), 0, Protocol::MaxLevel);
        e.direction = o.value("direction").toInt(e.direction);
    }
    if (const int active = root.value("mode").toInt(-1); Protocol::findMode(active))
        m_mode = active;
    const QJsonArray recent = root.value("recent").toArray();
    if (recent.size() == Presets.size()) {
        QList<QColor> colors;
        for (const auto &v : recent)
            if (QColor c(v.toString()); c.isValid())
                colors.append(c);
        if (colors.size() == Presets.size())
            m_recent = colors;
    }
}

// Written beside the target and renamed over it, so a crash never leaves
// half a profile.
void Backend::save() const {
    QJsonObject effects;
    for (auto it = m_effects.begin(); it != m_effects.end(); ++it)
        effects.insert(QString::number(it.key()), QJsonObject{{"color", it->color.name()},
                                                              {"random", it->random},
                                                              {"brightness", it->brightness},
                                                              {"speed", it->speed},
                                                              {"direction", it->direction}});
    QJsonArray recent;
    for (const QColor &c : m_recent)
        recent.append(c.name());
    QDir().mkpath(m_configDirectory);
    QSaveFile file(m_configDirectory + "/profile.json");
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{"mode", m_mode}, {"effects", effects}, {"recent", recent}}).toJson());
    file.commit();
}
