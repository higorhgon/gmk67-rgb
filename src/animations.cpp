#include "animations.h"

#include "protocol.h"

#include <QHash>
#include <algorithm>
#include <cmath>

namespace {

QColor lerp(const QColor &a, const QColor &b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(int(std::lround(a.red() + (b.red() - a.red()) * t)),
                  int(std::lround(a.green() + (b.green() - a.green()) * t)),
                  int(std::lround(a.blue() + (b.blue() - a.blue()) * t)));
}

QColor hue(double h) {
    h -= std::floor(h);
    return QColor::fromHsvF(float(h), 1, 1).toRgb();
}

// Classic fire: fuel along hidden rows under the space bar, heat that rises
// and cools, and a cooling map that scrolls up so the tongues flicker.
class Flames : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double, double dt) override {
        if (m_heat.isEmpty()) {
            m_heat = QList<double>(W * H, 0.0);
            for (int i = 0; i < W * H; ++i)
                m_cooling.append(uniform(0, 0.06));
        }
        m_steps += dt * 22 * rate();
        const int steps = std::min(int(m_steps), 6);
        m_steps -= steps;
        for (int i = 0; i < steps; ++i)
            step();

        QList<QColor> out;
        const int visible = H - Fuel;
        for (const auto &key : Protocol::keys()) {
            const int gx = std::min(int(key.x / Protocol::LayoutWidth * W), W - 1);
            // The top row maps near the top of the fire, the space bar row
            // just above the fuel.
            const int gy = int(std::lround(2 + key.y / (Protocol::LayoutHeight - 1) * (visible - 3)));
            out.append(palette(m_heat[gy * W + gx]));
        }
        return out;
    }

private:
    static constexpr int W = 34, H = 14, Fuel = 3;

    double &heat(int x, int y) { return m_heat[y * W + x]; }

    void step() {
        for (int y = H - Fuel; y < H; ++y)
            for (int x = 0; x < W; ++x)
                heat(x, y) = m_random.generateDouble() < 0.35 ? uniform(0.75, 1.0) : heat(x, y) * 0.8;
        m_shift = (m_shift + 1) % H;
        for (int y = H - Fuel - 1; y >= 0; --y) {
            const int coolRow = (y + m_shift) % H;
            for (int x = 0; x < W; ++x) {
                const double v = (heat(std::max(x - 1, 0), y + 1) + heat(x, y + 1)
                                  + heat(std::min(x + 1, W - 1), y + 1) + heat(x, std::min(y + 2, H - 1))) / 4;
                heat(x, y) = std::max(v - 0.045 - m_cooling[coolRow * W + x], 0.0);
            }
        }
    }

    static QColor palette(double v) {
        static const QList<QPair<double, QColor>> stops = {
            {0.0, QColor(0, 0, 0)},     {0.18, QColor(60, 0, 0)},    {0.38, QColor(200, 20, 0)},
            {0.6, QColor(255, 90, 0)},  {0.8, QColor(255, 170, 10)}, {1.0, QColor(255, 240, 140)}};
        v = std::clamp(v, 0.0, 1.0);
        for (int i = 1; i < stops.size(); ++i)
            if (v <= stops[i].first)
                return lerp(stops[i - 1].second, stops[i].second,
                            (v - stops[i - 1].first) / (stops[i].first - stops[i - 1].first));
        return stops.last().second;
    }

    QList<double> m_heat, m_cooling;
    double m_steps = 0;
    int m_shift = 0;
};

class RainbowWave : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double t, double) override {
        QList<QColor> out;
        for (const auto &key : Protocol::keys())
            out.append(hue(key.x / Protocol::LayoutWidth - t * 0.25 * rate() + key.y * 0.03));
        return out;
    }
};

class Plasma : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double t, double) override {
        t *= rate();
        QList<QColor> out;
        for (const auto &key : Protocol::keys()) {
            const double v = std::sin(key.x * 0.55 + t) + std::sin(key.y * 0.9 - t * 1.3)
                           + std::sin((key.x + key.y) * 0.35 + t * 0.7)
                           + std::sin(std::hypot(key.x - 8, key.y - 2) * 0.7 - t * 1.1);
            out.append(hue(v / 8 + t * 0.05));
        }
        return out;
    }
};

// Drops falling down each column, leaving a fading trail.
class MatrixRain : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double, double dt) override {
        for (int col = 0; col <= int(Protocol::LayoutWidth); ++col) {
            if (m_drops.contains(col)) {
                m_drops[col] += dt * 3.0 * rate();
                if (m_drops[col] > Protocol::LayoutHeight + 3)
                    m_drops.remove(col);
            } else if (m_random.generateDouble() < dt * 0.6 * rate()) {
                m_drops.insert(col, -0.5);
            }
        }
        const QColor head = lerp(m_color, Qt::white, 0.6);
        QList<QColor> out;
        for (const auto &key : Protocol::keys()) {
            const auto drop = m_drops.constFind(int(key.x));
            if (drop == m_drops.constEnd() || key.y > *drop + 0.5)
                out.append(Qt::black);
            else if (*drop - key.y < 0.6)
                out.append(head);
            else
                out.append(lerp(m_color, Qt::black, (*drop - key.y) / 3.5));
        }
        return out;
    }

private:
    QHash<int, double> m_drops;  // column → head position in rows
};

// Keys fade in and out at random, like stars.
class Twinkle : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double t, double) override {
        if (m_phase.isEmpty())
            for (int i = 0; i < Protocol::keys().size(); ++i) {
                m_phase.append(uniform(0, 2 * M_PI));
                m_frequency.append(uniform(0.4, 1.2));
            }
        t *= rate();
        QList<QColor> out;
        for (int i = 0; i < m_phase.size(); ++i)
            out.append(lerp(Qt::black, m_color, std::pow(std::max(std::sin(t * m_frequency[i] + m_phase[i]), 0.0), 3)));
        return out;
    }

private:
    QList<double> m_phase, m_frequency;
};

}  // namespace

double Animation::rate() const { return 0.25 * std::pow(2.0, m_speed / 5.0); }

std::unique_ptr<Animation> Animation::create(int value, int speed, const QColor &color, quint32 seed) {
    switch (value - Protocol::SoftwareBase) {
    case 0: return std::unique_ptr<Animation>(new Flames(speed, color, seed));
    case 1: return std::unique_ptr<Animation>(new RainbowWave(speed, color, seed));
    case 2: return std::unique_ptr<Animation>(new Plasma(speed, color, seed));
    case 3: return std::unique_ptr<Animation>(new MatrixRain(speed, color, seed));
    case 4: return std::unique_ptr<Animation>(new Twinkle(speed, color, seed));
    }
    return nullptr;
}

QList<QColor> dimmed(const QList<QColor> &colors, int brightness) {
    const double k = (std::clamp(brightness, 0, Protocol::MaxLevel) + 1) / double(Protocol::MaxLevel + 1);
    QList<QColor> out;
    out.reserve(colors.size());
    for (const QColor &c : colors)
        out.append(QColor(int(std::lround(c.red() * k)), int(std::lround(c.green() * k)), int(std::lround(c.blue() * k))));
    return out;
}
