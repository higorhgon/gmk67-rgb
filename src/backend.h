#pragma once

#include <QColor>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

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

public:
    // Mirrors Keyboard::Status for QML.
    enum Status { Ready, NotFound, NoPermission, Failed };
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
};
