#pragma once

#include <string_view>

/**
 * @enum EasingMode
 * @brief Enumerates the available easing modes for interpolation.
 */
enum class EasingMode {
    Linear = '-',
    In = 'i',
    Out = 'o',
};

/**
 * @enum EasingKind
 * @brief Enumerates the available kinds of easing functions.
 */
enum class EasingKind {
    Sine = 's',
    Power = 'p',
    Circular = 'c',
};

/**
 * @brief Returns the string representation of an EasingKind.
 * @param kind The EasingKind value.
 * @return String view of the kind name.
 */
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

/**
 * @brief Returns a character representing the EasingMode.
 * @param mode The EasingMode value.
 * @return Character for the mode.
 */
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

/**
 * @class Easing
 * @brief Provides methods for solving and inverting various easing functions.
 */
class Easing {
    using enum EasingKind;
    using enum EasingMode;

public:
    /**
     * @brief Solves the easing function for a given input and mode.
     * @param u Input value in [0, 1].
     * @param mode Easing mode (Linear, In, Out).
     * @return The eased value.
     */
    double Solve(double u, EasingMode mode) const;
    /**
     * @brief Inversely solves the easing function for a given output and mode.
     * @param v Output value in [0, 1].
     * @param mode Easing mode (Linear, In, Out).
     * @return The input value that produces the given output.
     */
    double InverseSolve(double v, EasingMode mode) const;

    EasingKind m_kind{Sine}; /**< The kind of easing function. */
    double m_param{0}; /**< The parameter for the easing function, if any. */

private:
    double SolveIn(double t) const;
    double SolveOut(double t) const;
    double SolveInverseIn(double y) const;
    double SolveInverseOut(double y) const;

    double CircularOut(double t) const;
    double InverseCircularOut(double y) const;
};
