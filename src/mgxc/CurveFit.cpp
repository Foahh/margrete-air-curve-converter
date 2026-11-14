#include "CurveFit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace mgxc {
    namespace {
        using Tick = std::int64_t;
        using Pair = std::array<double, 2>;
        constexpr double Tolerance = 1e-9;
        constexpr double Infinity = std::numeric_limits<double>::infinity();

        struct Sample {
            Tick t;
            Pair value;
        };

        Pair Values(const CurvePoint &p) { return {static_cast<double>(p.x), static_cast<double>(p.y)}; }

        // Match the RPC fitter's ties-to-even coordinate and candidate rounding.
        Tick RoundEven(double value) {
            if (!std::isfinite(value) || value < std::numeric_limits<int>::min() ||
                value > std::numeric_limits<int>::max()) {
                throw std::out_of_range("Curve coordinate or tick is outside the supported integer range");
            }
            const auto lower = static_cast<Tick>(std::floor(value));
            const double fraction = value - static_cast<double>(lower);
            return lower + (fraction > 0.5 || (fraction == 0.5 && lower % 2 != 0));
        }

        CurvePoint Quantize(const Sample &p) {
            return {p.t, static_cast<int>(RoundEven(p.value[0])), static_cast<int>(RoundEven(p.value[1]))};
        }

        Tick NextGrid(Tick t, int grid) { return t + grid - (t % grid + grid) % grid; }

        Pair Slope(const CurvePoint &a, const CurvePoint &b) {
            const double dt = static_cast<double>(b.t - a.t);
            return {(static_cast<double>(b.x) - a.x) / dt, (static_cast<double>(b.y) - a.y) / dt};
        }

        bool SameRatio(Tick a, Tick at, Tick b, Tick bt) {
            const auto ga = std::gcd(a, at), gb = std::gcd(b, bt);
            return a / ga == b / gb && at / ga == bt / gb;
        }

        std::vector<CurvePoint> DropCollinear(const std::vector<CurvePoint> &points) {
            std::vector<CurvePoint> result{points.front()};
            for (std::size_t i = 1; i + 1 < points.size(); ++i) {
                const auto &a = result.back(), &b = points[i], &c = points[i + 1];
                if (!SameRatio(static_cast<Tick>(b.x) - a.x, b.t - a.t, static_cast<Tick>(c.x) - a.x, c.t - a.t) ||
                    !SameRatio(static_cast<Tick>(b.y) - a.y, b.t - a.t, static_cast<Tick>(c.y) - a.y, c.t - a.t)) {
                    result.push_back(b);
                }
            }
            result.push_back(points.back());
            return result;
        }

        bool Pause(double a, double b, double c) {
            return (a == b && std::abs(c - b) > Tolerance) || (b == c && std::abs(b - a) > Tolerance);
        }

        void FitSamples(std::vector<CurvePoint> &points, std::span<const Sample> samples) {
            std::size_t start = 0;
            while (start + 1 < samples.size()) {
                const auto a = points.back();
                const auto origin = Values(a);
                Pair low{-Infinity, -Infinity}, high{Infinity, Infinity};
                auto end = start + 1;
                for (auto i = start + 1; i < samples.size(); ++i) {
                    const double dt = static_cast<double>(samples[i].t - a.t);
                    const auto slope = Slope(a, Quantize(samples[i]));
                    if (slope[0] >= low[0] - Tolerance / dt && slope[0] <= high[0] + Tolerance / dt &&
                        slope[1] >= low[1] - Tolerance / dt && slope[1] <= high[1] + Tolerance / dt) {
                        end = i;
                    }
                    for (int axis = 0; axis < 2; ++axis) {
                        low[axis] = std::max(low[axis], (samples[i].value[axis] - origin[axis] - 0.5) / dt);
                        high[axis] = std::min(high[axis], (samples[i].value[axis] - origin[axis] + 0.5) / dt);
                    }
                    if (low[0] > high[0] || low[1] > high[1])
                        break;
                }
                points.push_back(Quantize(samples[end]));
                start = end;
            }
        }

        std::optional<Tick> FitTail(const std::vector<Sample> &samples) {
            const auto first = Quantize(samples.front()), last = Quantize(samples.back());
            const auto origin = Values(first), target = Values(last);
            const Pair delta{target[0] - origin[0], target[1] - origin[1]};
            if (delta[0] == 0 && delta[1] == 0)
                return {};
            auto terminal = samples.size() - 1;
            while (terminal > 0) {
                const auto p = Quantize(samples[terminal - 1]);
                if (p.x != last.x || p.y != last.y)
                    break;
                --terminal;
            }
            Pair low{-Infinity, -Infinity}, high{Infinity, Infinity};
            double sumT = 0, sumT2 = 0, sumP = 0, sumTP = 0, bestScore = Infinity, baseline = 0;
            std::optional<Tick> bestTick;
            for (std::size_t i = 0; i < samples.size(); ++i) {
                const auto &sample = samples[i];
                const double t = static_cast<double>(sample.t);
                const double projection =
                        -delta[0] * (sample.value[0] - target[0]) - delta[1] * (sample.value[1] - target[1]);
                sumT += t;
                sumT2 += t * t;
                sumP += projection;
                sumTP += t * projection;
                if (t == 0)
                    continue;
                const double score = (delta[0] * delta[0] + delta[1] * delta[1]) *
                                             (static_cast<double>(i + 1) - 2 * sumT / t + sumT2 / (t * t)) -
                                     2 * (sumP - sumTP / t);
                if (i + 1 == samples.size()) {
                    baseline = score;
                } else if (i >= terminal && delta[0] / t >= low[0] - Tolerance / t &&
                           delta[0] / t <= high[0] + Tolerance / t && delta[1] / t >= low[1] - Tolerance / t &&
                           delta[1] / t <= high[1] + Tolerance / t && score < bestScore) {
                    bestScore = score;
                    bestTick = sample.t;
                }
                for (int axis = 0; axis < 2; ++axis) {
                    low[axis] = std::max(low[axis], (sample.value[axis] - origin[axis] - 0.5) / t);
                    high[axis] = std::min(high[axis], (sample.value[axis] - origin[axis] + 0.5) / t);
                }
            }
            return bestScore < baseline - Tolerance * std::max(1.0, std::abs(baseline)) ? bestTick : std::nullopt;
        }

        void FitEndpoints(std::vector<CurvePoint> &points, const std::vector<Sample> &samples) {
            auto a = points[points.size() - 2], b = points.back();
            std::vector<Sample> tail;
            for (const auto &s: samples) {
                if (s.t >= a.t && s.t <= b.t)
                    tail.push_back({s.t - a.t, s.value});
            }
            if (const auto offset = FitTail(tail))
                points.insert(points.end() - 1, {a.t + *offset, b.x, b.y});
            a = points[0];
            b = points[1];
            std::vector<Sample> head;
            for (auto it = samples.rbegin(); it != samples.rend(); ++it) {
                if (it->t >= a.t && it->t <= b.t)
                    head.push_back({b.t - it->t, it->value});
            }
            if (const auto offset = FitTail(head))
                points.insert(points.begin() + 1, {b.t - *offset, a.x, a.y});
        }

        Pair LocalVariation(const Pair &left, const Pair &right, const Pair *before, const Pair *after) {
            Pair result{};
            for (int axis = 0; axis < 2; ++axis) {
                result[axis] = std::abs(right[axis] - left[axis]);
                if (before)
                    result[axis] = std::abs(left[axis] - (*before)[axis]) + result[axis];
                if (after)
                    result[axis] += std::abs((*after)[axis] - right[axis]);
            }
            return result;
        }

        double AccurateSum(std::span<const double> values) {
            double sum = 0, correction = 0;
            for (const double value: values) {
                const double next = sum + value;
                correction += std::abs(sum) >= std::abs(value) ? (sum - next) + value : (value - next) + sum;
                sum = next;
            }
            return sum + correction;
        }

        // Prefix moments only reject candidates. Every accepted move still uses
        // the direct weighted-error and peak scan, including between-grid probes.
        class ErrorSums {
        public:
            explicit ErrorSums(const std::vector<Sample> &samples) :
                start(samples.front().t), span(static_cast<double>(samples.back().t - start)),
                origin(samples.front().value), roundoff((samples.size() + 16) * 0x1p-48) {
                sums.reserve(samples.size());
                sums.push_back({});
                for (std::size_t i = 0; i < samples.size(); ++i) {
                    const double x = samples[i].value[0] - origin[0], y = samples[i].value[1] - origin[1];
                    extent[0] = std::max(extent[0], std::abs(x));
                    extent[1] = std::max(extent[1], std::abs(y));
                    if (i + 1 == samples.size())
                        break;
                    const double u = (samples[i].t - start) / span;
                    const double weight = static_cast<double>(samples[i + 1].t - samples[i].t);
                    const std::array<double, 8> values{u, u * u, x, x * x, u * x, y, y * y, u * y};
                    auto next = sums.back();
                    for (std::size_t j = 0; j < next.size(); ++j)
                        next[j] += weight * values[j];
                    sums.push_back(next);
                }
            }

            Pair LowerBounds(const CurvePoint &a, const CurvePoint &b, std::size_t lo, std::size_t hi) const {
                const auto &first = sums[lo], &last = sums[hi];
                const double dt = static_cast<double>(b.t - a.t), u = (a.t - start) / span;
                const double su = last[0] - first[0], suu = last[1] - first[1];
                const auto av = Values(a), bv = Values(b);
                Pair bounds{};
                for (int axis = 0; axis < 2; ++axis) {
                    const int offset = 2 + 3 * axis;
                    if (last[offset + 1] == 0 && av[axis] == bv[axis] && av[axis] == origin[axis])
                        continue;
                    const double slope = (bv[axis] - av[axis]) * span / dt;
                    const double intercept = av[axis] - origin[axis] - slope * u;
                    const double sv = last[offset] - first[offset], svv = last[offset + 1] - first[offset + 1],
                                 suv = last[offset + 2] - first[offset + 2];
                    const std::array<double, 6> terms{intercept * intercept * dt,
                                                      2 * intercept * slope * su,
                                                      slope * slope * suu,
                                                      -2 * intercept * sv,
                                                      svv,
                                                      -2 * slope * suv};
                    const double scale =
                            std::abs(terms[0]) +
                            2 * std::abs(intercept * slope) * (std::abs(first[0]) + std::abs(last[0])) +
                            slope * slope * (std::abs(first[1]) + std::abs(last[1])) +
                            2 * std::abs(intercept) * (std::abs(first[offset]) + std::abs(last[offset])) +
                            std::abs(first[offset + 1]) + std::abs(last[offset + 1]) +
                            2 * std::abs(slope) * (std::abs(first[offset + 2]) + std::abs(last[offset + 2]));
                    const double residual =
                            std::max(std::abs(av[axis] - origin[axis]), std::abs(bv[axis] - origin[axis])) +
                            extent[axis];
                    const double rounding = 0x1p-48 * (std::abs(av[axis]) + std::abs(bv[axis] - av[axis]) +
                                                       std::abs(origin[axis]) + extent[axis]);
                    bounds[axis] = AccurateSum(terms) - roundoff * std::max(1.0, scale) -
                                   dt * rounding * (2 * residual + rounding);
                }
                return bounds;
            }

        private:
            Tick start;
            double span;
            Pair origin, extent{};
            double roundoff;
            std::vector<std::array<double, 8>> sums;
        };

        struct AxisError {
            double squared = 0;
            double peak = 0;
            double gridPeak = 0;
        };
        using Error = std::array<AxisError, 2>;

        Pair Evaluate(const CurvePoint &a, const CurvePoint &b, Tick t, const CurveEasing &easeX,
                      const CurveEasing &easeY) {
            if (t == a.t)
                return Values(a);
            if (t == b.t)
                return Values(b);
            const double u = static_cast<double>(t - a.t) / static_cast<double>(b.t - a.t);
            return {a.x + easeX(u) * (static_cast<double>(b.x) - a.x),
                    a.y + easeY(u) * (static_cast<double>(b.y) - a.y)};
        }

        std::array<int, 2> Directions(const CurvePoint &a, const CurvePoint &b) {
            return {(b.x > a.x) - (b.x < a.x), (b.y > a.y) - (b.y < a.y)};
        }

        void Refine(std::vector<CurvePoint> &points, const std::set<Tick> &fixed, int grid, const CurveEasing &easeX,
                    const CurveEasing &easeY) {
            if (points.size() <= 2)
                return;
            const auto first = points.front(), last = points.back();
            const auto checkStep = std::max<Tick>(1, std::min<Tick>(grid / 4, (last.t - first.t) / 4096));
            std::vector<Tick> ticks;
            for (Tick t = first.t; t < last.t; t += checkStep)
                ticks.push_back(t);
            for (Tick t = NextGrid(first.t, grid); t < last.t; t += grid)
                ticks.push_back(t);
            ticks.push_back(last.t);
            std::ranges::sort(ticks);
            ticks.erase(std::unique(ticks.begin(), ticks.end()), ticks.end());
            std::vector<Sample> samples;
            samples.reserve(ticks.size());
            std::unordered_map<Tick, std::size_t> index;
            for (const Tick t: ticks) {
                index.emplace(t, samples.size());
                samples.push_back({t, Evaluate(first, last, t, easeX, easeY)});
            }
            const ErrorSums sums(samples);
            std::unordered_map<Tick, CurvePoint> vertices;
            for (const auto &p: points)
                vertices.emplace(p.t, p);
            using Key = std::pair<Tick, Tick>;
            std::map<Key, Error> cache;
            std::map<Key, Pair> boundCache;
            Error limits{};
            bool limited = false;
            const auto error = [&](const CurvePoint &a, const CurvePoint &b) {
                const Key key{a.t, b.t};
                if (auto it = cache.find(key); it != cache.end())
                    return it->second;
                Error result{};
                const auto slope = Slope(a, b), origin = Values(a);
                for (auto j = index.at(a.t); j <= index.at(b.t); ++j) {
                    const auto &sample = samples[j];
                    for (int axis = 0; axis < 2; ++axis) {
                        const double value =
                                std::abs(origin[axis] + (sample.t - a.t) * slope[axis] - sample.value[axis]);
                        if (limited && (value > limits[axis].peak + Tolerance ||
                                        (sample.t % grid == 0 && value > limits[axis].gridPeak + Tolerance))) {
                            const Error invalid{{{Infinity, Infinity, Infinity}, {Infinity, Infinity, Infinity}}};
                            cache.emplace(key, invalid);
                            return invalid;
                        }
                        if (sample.t < b.t)
                            result[axis].squared += value * value * (samples[j + 1].t - sample.t);
                        result[axis].peak = std::max(result[axis].peak, value);
                        if (sample.t % grid == 0)
                            result[axis].gridPeak = std::max(result[axis].gridPeak, value);
                    }
                }
                cache.emplace(key, result);
                return result;
            };
            const auto lowerBound = [&](const CurvePoint &a, const CurvePoint &b) {
                const Key key{a.t, b.t};
                if (auto it = cache.find(key); it != cache.end()) {
                    return Pair{it->second[0].squared, it->second[1].squared};
                }
                if (auto it = boundCache.find(key); it != boundCache.end())
                    return it->second;
                const auto result = sums.LowerBounds(a, b, index.at(a.t), index.at(b.t));
                boundCache.emplace(key, result);
                return result;
            };
            std::vector<Error> edges;
            std::vector<Pair> slopes;
            Pair squared{}, variation{};
            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                edges.push_back(error(points[i], points[i + 1]));
                slopes.push_back(Slope(points[i], points[i + 1]));
                for (int axis = 0; axis < 2; ++axis) {
                    squared[axis] += edges.back()[axis].squared;
                    limits[axis].peak = std::max(limits[axis].peak, edges.back()[axis].peak);
                    limits[axis].gridPeak = std::max(limits[axis].gridPeak, edges.back()[axis].gridPeak);
                    if (i > 0)
                        variation[axis] += std::abs(slopes[i][axis] - slopes[i - 1][axis]);
                }
            }
            for (int axis = 0; axis < 2; ++axis)
                limits[axis].squared = squared[axis];
            limited = true;
            const auto variationLimit = variation;
            for (int sweep = 0; sweep < 8; ++sweep) {
                bool changed = false;
                for (std::size_t step = 0; step + 2 < points.size(); ++step) {
                    const auto i = sweep % 2 == 0 ? step + 1 : points.size() - 2 - step;
                    const auto a = points[i - 1], cur = points[i], b = points[i + 1];
                    if (fixed.contains(cur.t))
                        continue;
                    std::set<Tick> candidates;
                    for (int offset = -8; offset <= 8; ++offset)
                        candidates.insert(cur.t + static_cast<Tick>(offset) * grid);
                    for (int offset = 1; offset < 16; ++offset) {
                        candidates.insert(RoundEven((a.t + (b.t - a.t) * offset / 16.0) / grid) * grid);
                    }
                    const auto *before = i >= 2 ? &slopes[i - 2] : nullptr;
                    const auto *after = i + 1 < slopes.size() ? &slopes[i + 1] : nullptr;
                    const auto oldVariation = LocalVariation(slopes[i - 1], slopes[i], before, after);
                    Pair baseSquared{};
                    for (int axis = 0; axis < 2; ++axis)
                        baseSquared[axis] = squared[axis] - edges[i - 1][axis].squared - edges[i][axis].squared;
                    auto best = cur;
                    auto bestSquared = squared, bestVariation = variation;
                    std::array<Error, 2> bestEdges{edges[i - 1], edges[i]};
                    std::array<Pair, 2> bestSlopes{slopes[i - 1], slopes[i]};
                    for (const Tick t: candidates) {
                        if (t == cur.t || t <= a.t || t >= b.t)
                            continue;
                        auto it = vertices.find(t);
                        if (it == vertices.end())
                            it = vertices.emplace(t, Quantize(samples[index.at(t)])).first;
                        const auto candidate = it->second;
                        if (Directions(a, candidate) != Directions(a, cur) ||
                            Directions(candidate, b) != Directions(cur, b))
                            continue;
                        const auto leftSlope = Slope(a, candidate), rightSlope = Slope(candidate, b);
                        const auto local = LocalVariation(leftSlope, rightSlope, before, after);
                        const Pair newVariation{variation[0] - oldVariation[0] + local[0],
                                                variation[1] - oldVariation[1] + local[1]};
                        if (newVariation[0] > variationLimit[0] + Tolerance ||
                            newVariation[1] > variationLimit[1] + Tolerance)
                            continue;
                        const auto lowerLeft = lowerBound(a, candidate), lowerRight = lowerBound(candidate, b);
                        const Pair lower{baseSquared[0] + lowerLeft[0] + lowerRight[0],
                                         baseSquared[1] + lowerLeft[1] + lowerRight[1]};
                        if (lower[0] + lower[1] >= bestSquared[0] + bestSquared[1] - Tolerance ||
                            lower[0] > limits[0].squared + Tolerance || lower[1] > limits[1].squared + Tolerance)
                            continue;
                        const auto left = error(a, candidate);
                        if (!std::isfinite(left[0].squared))
                            continue;
                        const auto right = error(candidate, b);
                        if (!std::isfinite(right[0].squared))
                            continue;
                        const Pair nextSquared{baseSquared[0] + left[0].squared + right[0].squared,
                                               baseSquared[1] + left[1].squared + right[1].squared};
                        if (nextSquared[0] + nextSquared[1] >= bestSquared[0] + bestSquared[1] - Tolerance)
                            continue;
                        bool valid = true;
                        for (int axis = 0; axis < 2; ++axis) {
                            valid &= nextSquared[axis] <= limits[axis].squared + Tolerance &&
                                     std::max(left[axis].peak, right[axis].peak) <= limits[axis].peak + Tolerance &&
                                     std::max(left[axis].gridPeak, right[axis].gridPeak) <=
                                             limits[axis].gridPeak + Tolerance;
                        }
                        if (!valid)
                            continue;
                        best = candidate;
                        bestSquared = nextSquared;
                        bestVariation = newVariation;
                        bestEdges = {left, right};
                        bestSlopes = {leftSlope, rightSlope};
                    }
                    if (best != cur) {
                        points[i] = best;
                        squared = bestSquared;
                        variation = bestVariation;
                        edges[i - 1] = bestEdges[0];
                        edges[i] = bestEdges[1];
                        slopes[i - 1] = bestSlopes[0];
                        slopes[i] = bestSlopes[1];
                        changed = true;
                    }
                }
                if (!changed)
                    break;
            }
        }
    } // namespace

    std::vector<CurvePoint> FitCurve(const CurvePoint &first, const CurvePoint &last, int grid,
                                     const CurveEasing &easeX, const CurveEasing &easeY) {
        if (grid <= 0)
            throw std::invalid_argument("Curve grid must be positive");
        if (last.t <= first.t)
            throw std::invalid_argument("Curve end must be later than its start");
        std::vector<Sample> samples{{first.t, Values(first)}};
        for (Tick t = NextGrid(first.t, grid); t < last.t; t += grid)
            samples.push_back({t, Evaluate(first, last, t, easeX, easeY)});
        samples.push_back({last.t, Values(last)});
        bool linear = true;
        for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
            const double u = static_cast<double>(samples[i].t - first.t) / static_cast<double>(last.t - first.t);
            if (std::abs(samples[i].value[0] - (first.x + u * (static_cast<double>(last.x) - first.x))) > Tolerance ||
                std::abs(samples[i].value[1] - (first.y + u * (static_cast<double>(last.y) - first.y))) > Tolerance) {
                linear = false;
                break;
            }
        }
        if (linear)
            return {first, last};
        const double dx = std::abs(static_cast<double>(last.x) - first.x),
                     dy = std::abs(static_cast<double>(last.y) - first.y);
        const bool keepLanes = dx > 0 && (dy == 0 || dx <= dy);
        double lastX = first.x;
        std::vector<std::size_t> anchors{0};
        std::set<Tick> fixed{first.t, last.t};
        for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
            bool turn = false, pause = false;
            for (int axis = 0; axis < 2; ++axis) {
                const double a = samples[i - 1].value[axis], b = samples[i].value[axis], c = samples[i + 1].value[axis];
                turn |= (a < b && b > c) || (a > b && b < c);
                pause |= Pause(a, b, c);
            }
            const double x = samples[i].value[0];
            const bool crossing = keepLanes && (x >= lastX + 1 || x <= lastX - 1);
            if (turn || pause || crossing) {
                anchors.push_back(i);
                lastX = static_cast<double>(RoundEven(x));
            }
            if (turn || pause)
                fixed.insert(samples[i].t);
        }
        anchors.push_back(samples.size() - 1);
        std::vector<CurvePoint> points{first};
        for (std::size_t i = 0; i + 1 < anchors.size(); ++i) {
            FitSamples(points, std::span(samples).subspan(anchors[i], anchors[i + 1] - anchors[i] + 1));
        }
        FitEndpoints(points, samples);
        points = DropCollinear(points);
        Refine(points, fixed, grid, easeX, easeY);
        return DropCollinear(points);
    }
} // namespace mgxc
