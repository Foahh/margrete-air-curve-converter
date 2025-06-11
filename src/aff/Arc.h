#pragma once

#include "EasingSolver.h"

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

        int Duration() const;
        bool CanLinkWith(const Arc &other) const;
    };

    inline int Arc::Duration() const { return toT - t; }

    inline bool Arc::CanLinkWith(const Arc &other) const {
        return type == other.type && trace == other.trace && toX == other.x && toY == other.y && toT == other.t;
    }
} // namespace aff
