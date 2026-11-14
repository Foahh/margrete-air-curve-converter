#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <numbers>

#include "mgxc/CurveFit.h"
#include "mgxc/Easing.h"

using mgxc::CurvePoint;
using mgxc::FitCurve;

namespace {
    const auto Linear = [](double t) { return t; };
    const auto Sine = [](double t) { return (1 - std::cos(std::numbers::pi * t)) / 2; };
    const auto Quad = [](double t) { return t * t; };

    double At(const std::vector<CurvePoint> &points, std::int64_t t, bool height) {
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
            const auto &a = points[i], &b = points[i + 1];
            if (t >= a.t && t <= b.t) {
                const double av = height ? a.y : a.x, bv = height ? b.y : b.x;
                return av + (bv - av) * (t - a.t) / static_cast<double>(b.t - a.t);
            }
        }
        throw std::logic_error("Tick outside path");
    }
} // namespace

TEST_CASE("Fitting matches RPC reference height and mixed curves", "[curve]") {
    const auto height = FitCurve({0, 0, 20}, {960, 0, 120}, 5, Linear, Sine);
    const std::vector<CurvePoint> expectedHeight{{0, 0, 20},    {35, 0, 20},  {130, 0, 24},  {220, 0, 32},
                                                 {335, 0, 47},  {630, 0, 94}, {750, 0, 109}, {850, 0, 117},
                                                 {925, 0, 120}, {960, 0, 120}};
    REQUIRE(height == expectedHeight);
    const auto mixed = FitCurve({0, 0, 20}, {960, 12, 120}, 5, Sine, Quad);
    const std::vector<CurvePoint> expectedMixed{{0, 0, 20},    {50, 0, 20},   {175, 1, 23},   {190, 1, 24},
                                                {255, 2, 27},  {320, 3, 31},  {375, 4, 35},   {430, 5, 40},
                                                {480, 6, 45},  {535, 7, 51},  {585, 8, 57},   {645, 9, 65},
                                                {705, 10, 74}, {775, 11, 85}, {875, 12, 103}, {960, 12, 120}};
    REQUIRE(mixed == expectedMixed);
}

TEST_CASE("Every converter easing stays within half a unit on both axes", "[curve]") {
    const auto kind = GENERATE(EasingKind::Sine, EasingKind::Power, EasingKind::Circular);
    const auto modeX = GENERATE(EasingMode::Linear, EasingMode::In, EasingMode::Out);
    const auto modeY = GENERATE(EasingMode::Linear, EasingMode::In, EasingMode::Out);
    const int grid = GENERATE(1, 5, 20);
    const int direction = GENERATE(-1, 1);
    const int dx = GENERATE(0, 2, 12);
    const Easing easing{kind, kind == EasingKind::Power ? 3.0 : 0.3};
    const CurvePoint a{3, 4, 120}, b{963, 4 + direction * dx, 120 + direction * 100};
    const auto ex = [&](double t) { return easing.Solve(t, modeX); };
    const auto ey = [&](double t) { return easing.Solve(t, modeY); };
    const auto points = FitCurve(a, b, grid, ex, ey);
    REQUIRE(points.front() == a);
    REQUIRE(points.back() == b);
    for (std::size_t i = 1; i < points.size(); ++i) {
        REQUIRE(points[i].t > points[i - 1].t);
        REQUIRE((points[i].x - points[i - 1].x) * direction >= 0);
        REQUIRE((points[i].y - points[i - 1].y) * direction >= 0);
        if (i + 1 < points.size())
            REQUIRE(points[i].t % grid == 0);
    }
    for (int t = ((3 / grid) + 1) * grid; t < 963; t += grid) {
        const double u = (t - 3) / 960.0;
        REQUIRE(std::abs(At(points, t, false) - (a.x + direction * dx * ex(u))) <= 0.5 + 1e-9);
        REQUIRE(std::abs(At(points, t, true) - (a.y + direction * 100 * ey(u))) <= 0.5 + 1e-9);
    }
}

TEST_CASE("Height fit avoids quantization slope spikes", "[curve]") {
    const int direction = GENERATE(-1, 1);
    const int grid = GENERATE(1, 5, 20);
    const auto points = FitCurve({0, 4, 120}, {960, 4, 120 + direction * 100}, grid, Linear, Sine);
    REQUIRE(points.size() <= 15);
    double variation = 0, previous = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const double slope = static_cast<double>(points[i].y - points[i - 1].y) / (points[i].t - points[i - 1].t);
        if (i > 1)
            variation += std::abs(slope - previous);
        previous = slope;
    }
    REQUIRE(variation <= 1.3 * std::numbers::pi * 100 / 960);
}

TEST_CASE("Endpoint approach improves lane reconstruction", "[curve]") {
    const auto cubic = [](double t) { return 1 - std::pow(1 - t, 3); };
    const auto points = FitCurve({0, 0, 20}, {960, 12, 20}, 5, cubic, Linear);
    const auto penultimate = points[points.size() - 2];
    REQUIRE(penultimate.x == 12);
    auto without = points;
    without.erase(without.end() - 2);
    double before = 0, after = 0;
    for (int t = 0; t <= 960; ++t) {
        const double ideal = 12 * cubic(t / 960.0);
        before += std::pow(At(without, t, false) - ideal, 2);
        after += std::pow(At(points, t, false) - ideal, 2);
    }
    REQUIRE(after < before);
}

TEST_CASE("Refinement preserves pauses and overshoots", "[curve]") {
    const auto pause = [](double t) { return t < 0.25 ? 2 * t : t <= 0.75 ? 0.5 : 2 * t - 1; };
    const auto points = FitCurve({0, 0, 20}, {960, 12, 120}, 5, Linear, pause);
    REQUIRE(At(points, 240, true) == 70);
    REQUIRE(At(points, 720, true) == 70);
    for (const auto &p: points)
        if (p.t >= 240 && p.t <= 720)
            REQUIRE(p.y == 70);
    const auto peak = [](double t) { return t <= 0.25 ? 6 * t : 1.5 - (t - 0.25) * 2 / 3; };
    const auto overshoot = FitCurve({3, 0, 20}, {963, 12, 120}, 3, Linear, peak);
    REQUIRE(At(overshoot, 243, true) == 170);
}

TEST_CASE("Coarse long legs keep probe count bounded", "[curve]") {
    int calls = 0;
    const auto ease = [&](double t) {
        ++calls;
        return Sine(t);
    };
    const auto points = FitCurve({0, 4, 20}, {100000000, 4, 120}, 1000000, Linear, ease);
    REQUIRE(points.size() > 2);
    REQUIRE(calls < 10000);
    for (const auto &p: points)
        REQUIRE(p.t % 1000000 == 0);
}

TEST_CASE("Linear and short legs retain their endpoints", "[curve]") {
    REQUIRE(FitCurve({-7, 0, 20}, {3, 12, 120}, 20, Linear, Linear) ==
            std::vector<CurvePoint>{{-7, 0, 20}, {3, 12, 120}});
    REQUIRE_THROWS_AS(FitCurve({0, 0, 20}, {960, 0, 120}, 0, Linear, Sine), std::invalid_argument);
    REQUIRE_THROWS_AS(FitCurve({0, 0, 20}, {0, 0, 120}, 5, Linear, Sine), std::invalid_argument);
}

TEST_CASE("Refinement bounds peaks between grid samples", "[curve]") {
    const auto ripple = [](double t) { return t * t + 0.01 * std::pow(std::sin(std::numbers::pi * 960 * t / 5), 2); };
    const std::vector<CurvePoint> baseline{{0, 4, 20},   {35, 4, 20},  {150, 4, 22},  {325, 4, 31}, {490, 4, 46},
                                           {640, 4, 64}, {810, 4, 91}, {955, 4, 119}, {960, 4, 120}};
    const auto points = FitCurve({0, 4, 20}, {960, 4, 120}, 5, Linear, ripple);
    const auto quality = [&](const std::vector<CurvePoint> &p) {
        double squared = 0, peak = 0, variation = 0, previous = 0;
        for (int t = 0; t < 960; ++t) {
            const double error = std::abs(At(p, t, true) - (20 + 100 * ripple(t / 960.0)));
            squared += error * error;
            peak = std::max(peak, error);
        }
        for (std::size_t i = 1; i < p.size(); ++i) {
            const double slope = static_cast<double>(p[i].y - p[i - 1].y) / (p[i].t - p[i - 1].t);
            if (i > 1)
                variation += std::abs(slope - previous);
            previous = slope;
        }
        return std::array<double, 3>{squared, peak, variation};
    };
    const auto before = quality(baseline), after = quality(points);
    REQUIRE(before[1] > 1);
    REQUIRE(after[0] < before[0]);
    REQUIRE(after[1] <= before[1] + 1e-9);
    REQUIRE(after[2] <= before[2] + 1e-9);
}
