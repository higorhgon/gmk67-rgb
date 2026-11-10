#pragma once

#include <QColor>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "macros.h"
#include "protocol.h"
#include "streamer.h"

// The window's state: which effect is picked and how it's set, whether the
// keyboard is there, and what its keys show. Settings are kept per effect,
// so switching back finds them as they were.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList modes READ modes NOTIFY modesChanged)
    Q_PROPERTY(QVariantList keys READ keys CONSTANT)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap modeInfo READ modeInfo NOTIFY settingsChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY settingsChanged)
    Q_PROPERTY(bool random READ random WRITE setRandom NOTIFY settingsChanged)
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY settingsChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY settingsChanged)
    Q_PROPERTY(int direction READ direction WRITE setDirection NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList directions READ directions NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList recentColors READ recentColors NOTIFY recentColorsChanged)
    Q_PROPERTY(int maxLevel READ maxLevel CONSTANT)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString udevCommand READ udevCommand CONSTANT)
    Q_PROPERTY(int activeMode READ activeMode NOTIFY activeChanged)
    Q_PROPERTY(bool animating READ animating NOTIFY activeChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(QVariantList keyFunctions READ keyFunctions NOTIFY keyFunctionsChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    Q_PROPERTY(int macroSpace READ macroSpace CONSTANT)
    Q_PROPERTY(int macroUsed READ macroUsed NOTIFY macrosChanged)
    // Per Protocol::keys() entry: the function code it's given, 0 for its own.
    Q_PROPERTY(QVariantList remaps READ remaps NOTIFY keymapChanged)
    Q_PROPERTY(bool keymapDirty READ keymapDirty NOTIFY keymapChanged)

public:
    // Mirrors Keyboard::Status for QML.
    enum Status { Ready, NotFound, NoPermission, Failed, Wireless };
    Q_ENUM(Status)

    // configDirectory holds profile.json; tests pass a temporary one, and
    // probe = false keeps them off the real keyboard.
    explicit Backend(const QString &configDirectory = {}, bool probe = true, QObject *parent = nullptr);
    ~Backend() override;

    QVariantList modes() const;
    QVariantList keys() const;
    int mode() const { return m_mode; }
    void setMode(int value);
    QVariantMap modeInfo() const;
    QColor color() const { return current().color; }
    void setColor(const QColor &color);
    bool random() const { return current().random; }
    void setRandom(bool random);
    int brightness() const { return current().brightness; }
    void setBrightness(int value);
    int speed() const { return current().speed; }
    void setSpeed(int value);
    int direction() const { return current().direction; }
    void setDirection(int value);
    QVariantList directions() const;
    QVariantList recentColors() const;
    int maxLevel() const { return Protocol::MaxLevel; }
    int status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }
    QString udevCommand() const;
    int activeMode() const { return m_activeMode; }
    bool animating() const { return m_streamer.isRunning(); }
    bool busy() const { return m_busy; }
    QVariantList preview() const;
    QVariantList keyFunctions() const;
    QVariantList remaps() const;
    bool keymapDirty() const { return m_remaps != m_savedRemaps || m_macros != m_savedMacros; }
    QVariantList macros() const;
    int macroSpace() const { return Macros::Space; }
    int macroUsed() const { return Macros::used(m_macros); }

    // Macro editing, by slot. Like remaps, kept here until saveKeymap().
    Q_INVOKABLE int addMacro();
    Q_INVOKABLE void removeMacro(int slot);
    Q_INVOKABLE void renameMacro(int slot, const QString &name);
    Q_INVOKABLE void setMacroDelay(int slot, int ms);
    Q_INVOKABLE void addComboStep(int slot, int modifiers, int usage);
    Q_INVOKABLE bool addTextStep(int slot, const QString &text);
    Q_INVOKABLE void removeStep(int slot, int step);
    Q_INVOKABLE void moveStep(int slot, int step, int delta);
    Q_INVOKABLE bool typeable(const QString &text) const { return Macros::typeable(text); }
    // What a step does, as "Ctrl + Shift + T" or "Type “hello”".
    Q_INVOKABLE QString stepText(int slot, int step) const;

    // Remapping, by Protocol::keys() index. Changes stay here until
    // saveKeymap() writes them to the keyboard.
    Q_INVOKABLE bool remappable(int key) const;
    Q_INVOKABLE void remapKey(int key, int code);
    Q_INVOKABLE void resetKey(int key);
    Q_INVOKABLE void resetAllKeys();
    Q_INVOKABLE void discardKeymap();
    Q_INVOKABLE void saveKeymap();
    // The short label and the full name of a function code.
    Q_INVOKABLE QString functionLabel(int code) const;
    Q_INVOKABLE QString functionName(int code) const;

    // Sends the picked effect: saved on the keyboard for its own effects,
    // played from here for the animated ones.
    Q_INVOKABLE void apply();
    Q_INVOKABLE void stopAnimation();
    Q_INVOKABLE void copyUdevCommand();
    // Looks for the keyboard again and reads the effect it has.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void retranslate();

    // The settings of one effect, as kept in the profile.
    Protocol::Effect settings(int value) const;

signals:
    void modesChanged();
    void settingsChanged();
    void recentColorsChanged();
    void statusChanged();
    void activeChanged();
    void busyChanged();
    void previewChanged();
    void keymapChanged();
    void keyFunctionsChanged();
    void macrosChanged();
    // A short outcome for a toast.
    void notice(const QString &message);

private:
    Protocol::Effect &current();
    const Protocol::Effect &current() const;
    void changed();
    void setStatus(int status, const QString &message);
    void rememberColor(const QColor &color);
    void updateStaticPreview();
    void load();
    void save() const;
    void poll();

    QString m_configDirectory;
    bool m_probe;
    QHash<int, Protocol::Effect> m_effects;
    QList<QColor> m_recent;
    int m_mode = 0x01;
    int m_activeMode = -1;
    int m_status = NotFound;
    QString m_statusMessage;
    bool m_busy = false;
    bool m_readOnce = false;
    QList<QColor> m_preview;
    Streamer m_streamer;
    QTimer m_poll;
    QTimer m_saveTimer;
    // Plays the picked animation on screen only, until it plays for real.
    QTimer m_previewTimer;
    std::unique_ptr<Animation> m_previewAnimation;
    qint64 m_previewStart = 0, m_previewLast = 0;
    QFutureWatcher<QPair<bool, QPair<int, QString>>> m_applyWatcher;
    QFutureWatcher<QPair<bool, QPair<int, QString>>> m_keymapWatcher;
    // Light index → function code: as edited, and as last written. The
    // keyboard can't be asked for its table, so the profile remembers it.
    QHash<int, int> m_remaps, m_savedRemaps;
    QList<Macros::Macro> m_macros, m_savedMacros;
    void macrosEdited();
    bool m_resumeAnimation = false;
};
