#pragma once

#include <QColor>
#include <QList>
#include <QRandomGenerator>
#include <memory>

// Effects the keyboard can't do itself, rendered here one frame at a time
// and streamed as live frames. Each frame has one color per Protocol::keys()
// entry.
class Animation {
public:
    virtual ~Animation() = default;

    // value is a Protocol mode id at or above Protocol::SoftwareBase; null
    // for anything else. The seed makes tests repeatable.
    static std::unique_ptr<Animation> create(int value, int speed, const QColor &color,
                                             quint32 seed = QRandomGenerator::global()->generate());

    // t is the time since the start and dt since the last frame, in seconds.
    virtual QList<QColor> frame(double t, double dt) = 0;

protected:
    Animation(int speed, const QColor &color, quint32 seed) : m_speed(speed), m_color(color), m_random(seed) {}

    // Speed 0..15 as a time multiplier around 1.
    double rate() const;
    double uniform(double low, double high) { return low + (high - low) * m_random.generateDouble(); }

    int m_speed;
    QColor m_color;
    QRandomGenerator m_random;
};

// Brightness 0..15 applied to a whole frame.
QList<QColor> dimmed(const QList<QColor> &colors, int brightness);
