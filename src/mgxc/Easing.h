#pragma once

enum class EasingMode {
    Linear = '-',
    In = 'i',
    Out = 'o',
};

enum class EasingKind {
    Sine = 's',
    Power = 'p',
    Circular = 'c',
};

constexpr std::string_view GetKindStr(const EasingKind kind) {
    switch (kind) {
        using enum EasingKind;
        case Sine:
            return "Sine";
        case Power:
            return "Power";
        case Circular:
            return "Circular";
        default:
            return "??";
    }
}

constexpr char GetModeChar(const EasingMode mode) {
    switch (mode) {
        using enum EasingMode;
        case Linear:
            return 'S';
        case In:
            return 'I';
        case Out:
            return 'O';
        default:
            return '?';
    }
}

class Easing {
    using enum EasingKind;
    using enum EasingMode;

public:
    double Solve(double u, EasingMode mode) const;
    double InverseSolve(double v, EasingMode mode) const;

    EasingKind m_kind{Sine};
    double m_param{0};

private:
    double SolveIn(double t) const;
    double SolveOut(double t) const;
    double SolveInverseIn(double y) const;
    double SolveInverseOut(double y) const;

    double CircularOut(double t) const;
    double InverseCircularOut(double y) const;
};
