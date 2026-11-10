#pragma once

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QList>
#include <QString>

#include "protocol.h"

// The keyboard's control interface, over its hidraw node: 64-byte feature
// reports with no report ID on interface 0. Only the wired connection
// carries it; the 2.4 GHz dongle and Bluetooth don't.
class Keyboard {
    Q_DECLARE_TR_FUNCTIONS(Keyboard)

public:
    enum Status { Ready, NotFound, NoPermission, Failed };

    Keyboard() = default;
    ~Keyboard();
    Keyboard(const Keyboard &) = delete;
    Keyboard &operator=(const Keyboard &) = delete;

    // The /dev/hidrawN of the GMK67's control interface, or empty. sysRoot
    // and devRoot are only changed by tests.
    static QString find(const QString &sysRoot = "/sys/class/hidraw", const QString &devRoot = "/dev");

    bool open(const QString &path = {});
    void close();
    bool isOpen() const { return m_fd >= 0; }

    bool setEffect(const Protocol::Effect &effect);
    bool readEffect(Protocol::Effect *effect);
    // One temporary frame, one color per Protocol::keys() entry. Nothing is
    // saved: the keyboard returns to its stored effect a few seconds after
    // frames stop.
    bool sendLive(const QList<QColor> &colors);
    // Replaces the base layer's remap table (Keymap::table) and clears the
    // Fn layer's, so Fn keeps its stock functions. Saved on the keyboard.
    bool writeKeymap(const QByteArray &table);
    // Replaces every macro with Macros::area. Saved on the keyboard.
    bool writeMacros(const QByteArray &area);

    Status status() const { return m_status; }
    QString errorString() const { return m_error; }

private:
    bool send(const QByteArray &data, bool pause);
    bool read(QByteArray *reply, bool pause);
    bool command(int cmd, int pages = 0);
    bool exchange(const QByteArray &packet);
    bool fail(Status status, const QString &message);

    int m_fd = -1;
    Status m_status = Ready;
    QString m_error;
};
