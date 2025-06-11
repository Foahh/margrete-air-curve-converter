#pragma once


enum class EasingMode {
    Linear = 0,
    In = 1,
    Out = 2,
};

enum class EasingKind {
    Sine = 0,
    Power = 1,
    Circular = 2,
};

class EasingSolver {
    using enum EasingKind;
    using enum EasingMode;

public:
    double Solve(double u, EasingMode mode) const;
    double InverseSolve(double v, EasingMode mode) const;

    EasingKind m_kind{Sine};
    int m_exponent{2};
    double m_epsilon{0.2};

private:
    double SolveIn(double t) const;
    double SolveOut(double t) const;
    double SolveInverseIn(double y) const;
    double SolveInverseOut(double y) const;

    double CircularOut(double t) const;
    double InverseCircularOut(double y) const;
};
