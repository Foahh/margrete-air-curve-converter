#pragma once

#include "Easing.h"

namespace aff {
    /**
     * @struct Arc
     * @brief Represents a single arc in an .aff file.
     *
     * Contains timing, position, type, and linking logic for arc chains.
     */
    struct Arc {
        /** Start tick of the arc. */
        int t{0};
        /** End tick of the arc. */
        int toT{0};

        /** Start X position. */
        int x{0};
        /** End X position. */
        int toX{0};
        /** Easing mode for X. */
        EasingMode eX{EasingMode::Linear};

        /** Start Y position. */
        int y{0};
        /** End Y position. */
        int toY{0};
        /** Easing mode for Y. */
        EasingMode eY{EasingMode::Linear};

        /** Arc type identifier. */
        int type{0};
        /** Whether the arc is a trace. */
        bool trace{false};

        /**
         * @brief Returns the duration (tick length) of the arc.
         * @return Duration in ticks.
         */
        int Duration() const { return toT - t; }

        /**
         * @brief Checks if this arc can be linked with another arc.
         * @param other The other arc to check.
         * @return True if the arcs can be linked.
         */
        bool CanLinkWith(const Arc &other) const {
            return type == other.type && trace == other.trace && toX == other.x && toY == other.y && toT == other.t;
        }
    };
} // namespace aff
