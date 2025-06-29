#pragma once

#include "Easing.h"

namespace aff {
    struct Arc {
        int t{0};
        int toT{0};

        int x{0};
        int toX{0};
        EasingMode eX{EasingMode::Linear};

        int y{0};
        int toY{0};
        EasingMode eY{EasingMode::Linear};

        int type{0};
        bool trace{false};

        int Duration() const { return toT - t; }

        bool CanLinkWith(const Arc &other) const {
            return type == other.type && trace == other.trace && toX == other.x && toY == other.y && toT == other.t;
        }
    };
} // namespace aff
