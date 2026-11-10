#pragma once

#include <QColor>
#include <QList>
#include <QMutex>
#include <QThread>
#include <memory>

#include "animations.h"
#include "keyboard.h"

// Plays an animation on the keyboard from a thread of its own, at a steady
// rate, and keeps trying while the keyboard is unplugged. The keyboard
// seems to drop some live frames, and 60 a second looked smoother than 10
// to 30.
class Streamer : public QThread {
    Q_OBJECT

public:
    explicit Streamer(QObject *parent = nullptr) : QThread(parent) {}
    ~Streamer() override { stop(); }

    // Takes over the animation; stops whatever was playing first.
    void play(std::unique_ptr<Animation> animation, int brightness, int fps = 60);
    void stop();

signals:
    // The keyboard became reachable or not; message says why not.
    void reachable(bool ok, int status, const QString &message);

protected:
    void run() override;

private:
    std::unique_ptr<Animation> m_animation;
    int m_brightness = Protocol::MaxLevel;
    int m_fps = 60;
};
