#include "streamer.h"

#include <QElapsedTimer>

void Streamer::play(std::unique_ptr<Animation> animation, int brightness, int fps) {
    stop();
    m_animation = std::move(animation);
    m_brightness = brightness;
    m_fps = std::max(fps, 1);
    if (m_animation)
        start();
}

void Streamer::stop() {
    if (isRunning()) {
        requestInterruption();
        wait();
    }
    m_animation.reset();
}

void Streamer::run() {
    const qint64 period = 1000000000LL / m_fps;
    Keyboard keyboard;
    bool reported = false, wasReachable = false;
    QElapsedTimer clock;
    clock.start();
    qint64 last = clock.nsecsElapsed(), lastPreview = 0;

    while (!isInterruptionRequested()) {
        const qint64 now = clock.nsecsElapsed();
        const QList<QColor> colors = dimmed(m_animation->frame(now / 1e9, (now - last) / 1e9), m_brightness);
        last = now;

        if (keyboard.sendLive(colors)) {
            if (!wasReachable)
                emit reachable(true, Keyboard::Ready, {});
            wasReachable = true;
            reported = false;
        } else {
            // Report once per outage, then retry about once a second while
            // the preview keeps playing.
            if (!reported)
                emit reachable(false, keyboard.status(), keyboard.errorString());
            reported = true;
            wasReachable = false;
        }
        if (now - lastPreview >= 33000000LL) {
            emit frame(colors);
            lastPreview = now;
        }

        const qint64 wait = wasReachable ? period - (clock.nsecsElapsed() - now) : 1000000000LL;
        // Sleep in short slices so stop() never waits long.
        for (qint64 slept = 0; slept < wait && !isInterruptionRequested(); slept += 5000000LL)
            QThread::usleep(std::min<qint64>(5000000LL, wait - slept) / 1000);
    }
}
