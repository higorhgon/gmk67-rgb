#include "backend.h"

#include "keyboard.h"
#include "keymap.h"

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

    connect(&m_keymapWatcher, &QFutureWatcher<QPair<bool, QPair<int, QString>>>::finished, this, [this] {
        const auto result = m_keymapWatcher.result();
        m_busy = false;
        emit busyChanged();
        if (result.first) {
            m_savedRemaps = m_remaps;
            m_savedMacros = m_macros;
            save();
            setStatus(Ready, {});
            emit keymapChanged();
            emit notice(tr("Keys saved on the keyboard"));
        } else {
            setStatus(result.second.first, result.second.second);
        }
        // The animation gave the keyboard up for the write; it carries on.
        if (m_resumeAnimation && Protocol::isAnimated(m_activeMode)) {
            const Protocol::Effect e = settings(m_activeMode);
            m_streamer.play(Animation::create(m_activeMode, e.speed, e.color), e.brightness);
            emit activeChanged();
        }
        m_resumeAnimation = false;
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

// The preview always shows the selected effect, not the one on the keyboard:
// animated ones play their own copy, and a firmware effect shows roughly what
// it looks like, its color or a rainbow for the multicolor ones.
void Backend::updateStaticPreview() {
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
    emit keyFunctionsChanged();
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
    const QJsonObject keymap = root.value("keymap").toObject();
    for (auto it = keymap.begin(); it != keymap.end(); ++it) {
        bool ok = false;
        const int index = it.key().toInt(&ok);
        const int code = it.value().toInt();
        if (ok && Keymap::remappable(index) && Keymap::find(code))
            m_savedRemaps.insert(index, code);
    }
    for (const auto &value : root.value("macros").toArray()) {
        const QJsonObject o = value.toObject();
        Macros::Macro macro;
        macro.name = o.value("name").toString();
        macro.delay = std::clamp(o.value("delay").toInt(Macros::MinDelay), Macros::MinDelay, Macros::MaxDelay);
        for (const auto &stepValue : o.value("steps").toArray()) {
            const QJsonObject so = stepValue.toObject();
            Macros::Step step;
            if (so.value("type").toString() == "text") {
                step.type = Macros::Step::Text;
                step.text = so.value("text").toString();
            } else {
                step.modifiers = so.value("modifiers").toInt() & 0xF;
                step.usage = so.value("usage").toInt();
                if (!Keymap::find(Keymap::code(Keymap::Key, step.usage)))
                    continue;
            }
            macro.steps.append(step);
        }
        m_savedMacros.append(macro);
    }
    m_macros = m_savedMacros;
    m_remaps = m_savedRemaps;
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
    QJsonObject keymap;
    for (auto it = m_savedRemaps.begin(); it != m_savedRemaps.end(); ++it)
        keymap.insert(QString::number(it.key()), it.value());
    QJsonArray macros;
    for (const Macros::Macro &macro : m_savedMacros) {
        QJsonArray steps;
        for (const Macros::Step &step : macro.steps)
            steps.append(step.type == Macros::Step::Text
                             ? QJsonObject{{"type", "text"}, {"text", step.text}}
                             : QJsonObject{{"type", "combo"}, {"modifiers", step.modifiers}, {"usage", step.usage}});
        macros.append(QJsonObject{{"name", macro.name}, {"delay", macro.delay}, {"steps", steps}});
    }
    QDir().mkpath(m_configDirectory);
    QSaveFile file(m_configDirectory + "/profile.json");
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject{{"mode", m_mode}, {"effects", effects}, {"recent", recent}, {"keymap", keymap}, {"macros", macros}}).toJson());
    file.commit();
}

QVariantList Backend::keyFunctions() const {
    QVariantList list;
    for (const Keymap::Function &f : Keymap::functions())
        list.append(QVariantMap{{"code", f.code},
                                {"name", QCoreApplication::translate("Keymap", f.name)},
                                {"label", QString::fromUtf8(f.label)},
                                {"group", QCoreApplication::translate("Keymap", f.group)}});
    for (int slot = 0; slot < m_macros.size(); ++slot) {
        const int code = Keymap::code(Keymap::Macro, slot);
        list.append(QVariantMap{{"code", code}, {"name", functionName(code)}, {"label", functionLabel(code)},
                                {"group", tr("Macros")}});
    }
    return list;
}

QVariantList Backend::remaps() const {
    QVariantList list;
    for (const auto &key : Protocol::keys())
        list.append(m_remaps.value(key.index, 0));
    return list;
}

bool Backend::remappable(int key) const {
    return key >= 0 && key < Protocol::keys().size() && Keymap::remappable(Protocol::keys()[key].index);
}

void Backend::remapKey(int key, int code) {
    if (!remappable(key) || !(Keymap::find(code) || (code >> 16 == Keymap::Macro && (code & 0xFFFF) < m_macros.size())))
        return;
    m_remaps.insert(Protocol::keys()[key].index, code);
    emit keymapChanged();
}

void Backend::resetKey(int key) {
    if (key >= 0 && key < Protocol::keys().size() && m_remaps.remove(Protocol::keys()[key].index))
        emit keymapChanged();
}

void Backend::resetAllKeys() {
    if (m_remaps.isEmpty())
        return;
    m_remaps.clear();
    emit keymapChanged();
}

void Backend::discardKeymap() {
    m_remaps = m_savedRemaps;
    m_macros = m_savedMacros;
    emit keymapChanged();
    emit macrosChanged();
    emit keyFunctionsChanged();
}

void Backend::saveKeymap() {
    if (m_busy)
        return;
    if (m_macros != m_savedMacros && Macros::area(m_macros).isEmpty()) {
        emit notice(tr("The macros don't fit in the keyboard's memory."));
        return;
    }
    // Only one program may talk to the keyboard at a time, or packets mix:
    // a playing animation pauses for the write.
    m_resumeAnimation = animating();
    m_streamer.stop();
    m_busy = true;
    emit busyChanged();
    const QByteArray table = Keymap::table(m_remaps);
    // Macros are written only when they changed: the area is small and
    // their write is the less tried one.
    const QByteArray area = m_macros != m_savedMacros ? Macros::area(m_macros) : QByteArray();
    m_keymapWatcher.setFuture(QtConcurrent::run([table, area] {
        Keyboard keyboard;
        const bool ok = (area.isEmpty() || keyboard.writeMacros(area)) && keyboard.writeKeymap(table);
        return qMakePair(ok, qMakePair(int(keyboard.status()), keyboard.errorString()));
    }));
}

QString Backend::functionLabel(int code) const {
    if (code >> 16 == Keymap::Macro)
        return QString("M%1").arg((code & 0xFFFF) + 1);
    const Keymap::Function *f = Keymap::find(code);
    return f ? QString::fromUtf8(f->label) : QString();
}

QString Backend::functionName(int code) const {
    if (code >> 16 == Keymap::Macro) {
        const int slot = code & 0xFFFF;
        const QString name = slot < m_macros.size() ? m_macros[slot].name : QString();
        return name.isEmpty() ? tr("Macro %1").arg(slot + 1) : QString("%1. %2").arg(slot + 1).arg(name);
    }
    const Keymap::Function *f = Keymap::find(code);
    return f ? QCoreApplication::translate("Keymap", f->name) : QString();
}

void Backend::macrosEdited() {
    emit macrosChanged();
    emit keymapChanged();
    emit keyFunctionsChanged();
}

QVariantList Backend::macros() const {
    QVariantList list;
    for (int slot = 0; slot < m_macros.size(); ++slot) {
        const Macros::Macro &m = m_macros[slot];
        QVariantList steps;
        for (int i = 0; i < m.steps.size(); ++i)
            steps.append(stepText(slot, i));
        list.append(QVariantMap{{"name", m.name},
                                {"title", functionName(Keymap::code(Keymap::Macro, slot))},
                                {"delay", m.delay},
                                {"steps", steps},
                                {"size", int(Macros::body(m).size())}});
    }
    return list;
}

int Backend::addMacro() {
    Macros::Macro macro;
    macro.name = tr("Macro %1").arg(m_macros.size() + 1);
    m_macros.append(macro);
    macrosEdited();
    return int(m_macros.size()) - 1;
}

// Later macros move up a slot, so keys given them follow, and keys given
// this one go back to their own function.
void Backend::removeMacro(int slot) {
    if (slot < 0 || slot >= m_macros.size())
        return;
    m_macros.removeAt(slot);
    for (auto it = m_remaps.begin(); it != m_remaps.end();) {
        if (it.value() >> 16 == Keymap::Macro) {
            const int s = it.value() & 0xFFFF;
            if (s == slot) {
                it = m_remaps.erase(it);
                continue;
            }
            if (s > slot)
                it.value() = Keymap::code(Keymap::Macro, s - 1);
        }
        ++it;
    }
    macrosEdited();
}

void Backend::renameMacro(int slot, const QString &name) {
    if (slot < 0 || slot >= m_macros.size() || m_macros[slot].name == name.trimmed())
        return;
    m_macros[slot].name = name.trimmed();
    macrosEdited();
}

void Backend::setMacroDelay(int slot, int ms) {
    ms = std::clamp(ms, Macros::MinDelay, Macros::MaxDelay);
    if (slot < 0 || slot >= m_macros.size() || m_macros[slot].delay == ms)
        return;
    m_macros[slot].delay = ms;
    macrosEdited();
}

void Backend::addComboStep(int slot, int modifiers, int usage) {
    if (slot < 0 || slot >= m_macros.size() || !Keymap::find(Keymap::code(Keymap::Key, usage)))
        return;
    Macros::Step step;
    step.modifiers = modifiers & 0xF;
    step.usage = usage;
    m_macros[slot].steps.append(step);
    macrosEdited();
}

bool Backend::addTextStep(int slot, const QString &text) {
    if (slot < 0 || slot >= m_macros.size() || text.isEmpty() || !Macros::typeable(text))
        return false;
    Macros::Step step;
    step.type = Macros::Step::Text;
    step.text = text;
    m_macros[slot].steps.append(step);
    macrosEdited();
    return true;
}

void Backend::removeStep(int slot, int step) {
    if (slot < 0 || slot >= m_macros.size() || step < 0 || step >= m_macros[slot].steps.size())
        return;
    m_macros[slot].steps.removeAt(step);
    macrosEdited();
}

void Backend::moveStep(int slot, int step, int delta) {
    if (slot < 0 || slot >= m_macros.size())
        return;
    auto &steps = m_macros[slot].steps;
    const int to = step + delta;
    if (step < 0 || step >= steps.size() || to < 0 || to >= steps.size())
        return;
    steps.move(step, to);
    macrosEdited();
}

QString Backend::stepText(int slot, int step) const {
    if (slot < 0 || slot >= m_macros.size() || step < 0 || step >= m_macros[slot].steps.size())
        return {};
    const Macros::Step &s = m_macros[slot].steps[step];
    if (s.type == Macros::Step::Text)
        return tr("Type “%1”").arg(QString(s.text).replace('\n', "⏎"));
    QStringList parts;
    if (s.modifiers & Macros::Ctrl)
        parts << "Ctrl";
    if (s.modifiers & Macros::Shift)
        parts << "Shift";
    if (s.modifiers & Macros::Alt)
        parts << "Alt";
    if (s.modifiers & Macros::Super)
        parts << "Super";
    parts << functionName(Keymap::code(Keymap::Key, s.usage));
    return parts.join(" + ");
}
