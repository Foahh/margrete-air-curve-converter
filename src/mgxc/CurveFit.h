#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace mgxc {
    struct CurvePoint {
        std::int64_t t;
        int x;
        int y;

        bool operator==(const CurvePoint &) const = default;
    };

    using CurveEasing = std::function<double(double)>;

    // Fit quantized chords, then refine their grid-aligned joints without
    // increasing either axis's error or total slope variation. Anchors are exact.
    std::vector<CurvePoint> FitCurve(const CurvePoint &first, const CurvePoint &last, int grid,
                                     const CurveEasing &easeX, const CurveEasing &easeY);
} // namespace mgxc
