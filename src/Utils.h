#pragma once
#include <array>
#include <stdexcept>
#include <type_traits>

namespace utils {
    static int step(const double v) { return static_cast<int>(v > 0) - static_cast<int>(v < 0); }
    static int iround(const double v) { return static_cast<int>(std::round(v)); }

    template<std::ranges::sized_range R, std::integral I>
    constexpr bool in_bounds(const R &r, I i) noexcept {
        if (r.empty()) {
            return false;
        }

        if constexpr (std::signed_integral<I>) {
            if (i < 0) {
                return false;
            }
        }

        return std::cmp_less(i, r.size());
    }

    template<typename T>
    concept Enum = std::is_enum_v<T>;

    template<Enum E, std::size_t N>
    constexpr int enum_to_idx(E value, const std::array<E, N> &map) {
        for (std::size_t i = 0; i < N; ++i)
            if (map[i] == value) {
                return static_cast<int>(i);
            }

        throw std::out_of_range("enum_to_idx: value not found in map");
    }

    template<Enum E, std::size_t N>
    constexpr E idx_to_enum(int idx, const std::array<E, N> &map) {
        if (idx >= N || idx < 0) {
            throw std::out_of_range("idx_to_enum: index out of range");
        }

        return map[idx];
    }
} // namespace utils
