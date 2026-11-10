#include "animations.h"

#include "protocol.h"

#include <array>
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

// Smooth noise in -1..1 over a plane: random values on a grid, blended
// with a smoothstep, so it never jumps however it is sampled.
class Noise {
public:
    explicit Noise(QRandomGenerator &random) {
        for (double &v : m_values)
            v = random.generateDouble() * 2 - 1;
    }

    double at(double x, double y) const {
        const double fx = std::floor(x), fy = std::floor(y);
        const int ix = int(fx), iy = int(fy);
        const double u = smooth(x - fx), v = smooth(y - fy);
        const double top = value(ix, iy) + (value(ix + 1, iy) - value(ix, iy)) * u;
        const double bottom = value(ix, iy + 1) + (value(ix + 1, iy + 1) - value(ix, iy + 1)) * u;
        return top + (bottom - top) * v;
    }

private:
    static double smooth(double t) { return t * t * (3 - 2 * t); }
    double value(int x, int y) const { return m_values[size_t((x * 73856093) ^ (y * 19349663)) % m_values.size()]; }

    std::array<double, 256> m_values;
};

// Tongues of fire: each spot along the keyboard burns to a height that rises
// and falls smoothly, hottest at the bottom row and fading to dark red at the
// tip. A real fire simulation turned to noise on only five rows.
class Flames : public Animation {
public:
    Flames(int speed, const QColor &color, quint32 seed) : Animation(speed, color, seed), m_noise(m_random) {}

    QList<QColor> frame(double t, double) override {
        t *= rate() * 1.8;
        QList<QColor> out;
        for (const auto &key : Protocol::keys()) {
            // Rows above the bottom one, to the middle of the key.
            const double row = Protocol::LayoutHeight - 1 - key.y + 0.5;
            const double height = 3.6 + 1.4 * m_noise.at(key.x * 0.45, t * 0.9)
                                + 0.6 * m_noise.at(key.x * 1.1 + 40, t * 1.5);
            double heat = 1 - row / std::max(height, 0.5);
            // A slow shimmer; a fast one made the dim tips blink.
            heat += 0.06 * m_noise.at(key.x * 1.7 + 90, key.y * 1.3 + t * 1.2);
            out.append(palette(std::clamp(heat * 1.15, 0.0, 1.0)));
        }
        return out;
    }

private:
    static QColor palette(double v) {
        static const QList<QPair<double, QColor>> stops = {
            {0.0, QColor(0, 0, 0)},     {0.15, QColor(70, 0, 0)},    {0.38, QColor(200, 20, 0)},
            {0.62, QColor(255, 80, 0)}, {0.85, QColor(255, 160, 10)}, {1.0, QColor(255, 220, 90)}};
        for (int i = 1; i < stops.size(); ++i)
            if (v <= stops[i].first)
                return lerp(stops[i - 1].second, stops[i].second,
                            (v - stops[i - 1].first) / (stops[i].first - stops[i - 1].first));
        return stops.last().second;
    }

    Noise m_noise;
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

// Drops falling down the keyboard, each at its own spot across it. A drop
// lights whichever key lies under it on each row, since the rows are
// staggered, and every key fades out on its own so the trail is smooth.
class MatrixRain : public Animation {
public:
    using Animation::Animation;

    QList<QColor> frame(double, double dt) override {
        const auto &keys = Protocol::keys();
        if (m_level.isEmpty())
            m_level = QList<double>(keys.size(), 0.0);

        if (m_random.generateDouble() < dt * 7.0 * rate())
            m_drops.append({uniform(0, Protocol::LayoutWidth), -0.5, uniform(0.8, 1.3)});
        for (Drop &drop : m_drops)
            drop.y += dt * 3.0 * rate() * drop.speed;
        m_drops.removeIf([](const Drop &d) { return d.y > Protocol::LayoutHeight + 0.5; });

        const double fade = std::exp(-dt * 2.2 * rate() / 0.5);
        QList<QColor> out;
        for (int i = 0; i < keys.size(); ++i) {
            const auto &key = keys[i];
            double head = 0;
            for (const Drop &drop : m_drops)
                if (std::abs(drop.x - key.x) <= key.width / 2 && std::abs(drop.y - key.y) < 1)
                    head = std::max(head, 1 - std::abs(drop.y - key.y));
            m_level[i] = std::max(m_level[i] * fade, head);
            out.append(lerp(Qt::black, m_color, m_level[i]));
        }
        return out;
    }

private:
    struct Drop {
        double x, y, speed;
    };
    QList<Drop> m_drops;
    QList<double> m_level;
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
