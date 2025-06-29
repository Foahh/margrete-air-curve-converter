#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <stdexcept>

#include "Easing.h"

double Easing::Solve(const double u, const EasingMode mode) const {
    if (u < 0.0 || u > 1.0) {
        throw std::out_of_range(std::format("Value must be in the range [0.0, 1.0], got {}", u));
    }

    if (mode == Linear) {
        return u;
    }

    return mode == In ? SolveIn(u) : SolveOut(u);
}

double Easing::InverseSolve(const double v, const EasingMode mode) const {
    if (v < 0.0 || v > 1.0) {
        throw std::out_of_range(std::format("Value must be in the range [0.0, 1.0], got {}", v));
    }

    if (mode == Linear) {
        return v;
    }

    return mode == In ? SolveInverseIn(v) : SolveInverseOut(v);
}

double Easing::SolveIn(const double t) const {
    switch (m_kind) {
        case Sine:
            return std::sin(t * std::numbers::pi_v<double> / 2.0);
        case Power:
            return 1.0 - std::pow(1.0 - t, m_param);
        case Circular:
            return 1.0 - CircularOut(1.0 - t);
        default:
            return t;
    }
}

double Easing::SolveOut(const double t) const {
    switch (m_kind) {
        case Sine:
            return 1.0 - std::sin((1.0 - t) * std::numbers::pi_v<double> / 2.0);
        case Power:
            return std::pow(t, m_param);
        case Circular:
            return CircularOut(t);
        default:
            return t;
    }
}

double Easing::SolveInverseIn(const double y) const {
    switch (m_kind) {
        case Sine:
            return 2.0 / std::numbers::pi_v<double> * std::asin(y);
        case Power:
            return 1.0 - std::pow(1.0 - y, 1.0 / m_param);
        case Circular:
            return 1.0 - InverseCircularOut(1.0 - y);
        default:
            return y;
    }
}

double Easing::SolveInverseOut(const double y) const {
    switch (m_kind) {
        case Sine:
            return 1.0 - 2.0 / std::numbers::pi_v<double> * std::asin(1.0 - y);
        case Power:
            return std::pow(y, 1.0 / m_param);
        case Circular:
            return InverseCircularOut(y);
        default:
            return y;
    }
}

double Easing::CircularOut(const double t) const {
    return m_param * t + (1.0 - m_param) * (1.0 - std::sqrt(1.0 - t * t));
}

double Easing::InverseCircularOut(const double y) const {
    if (m_param == 1.0)
        return y;
    if (m_param == 0.0)
        return std::sqrt(1.0 - (1.0 - y) * (1.0 - y));

    const double bias = m_param / (1.0 - m_param);
    const double offset = (1.0 - m_param - y) / (1.0 - m_param);

    const double qA = 1.0 + bias * bias;
    const double qB = 2.0 * offset * bias;
    const double qC = offset * offset - 1.0;

    const double disc = std::max(0.0, qB * qB - 4.0 * qA * qC);
    return std::clamp((-qB + std::sqrt(disc)) / (2.0 * qA), 0.0, 1.0);
}
