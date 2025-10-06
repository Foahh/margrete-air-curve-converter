#pragma once
#include <array>
#include <stdexcept>
#include <type_traits>

namespace utils {
    /**
     * @brief Returns the sign of a value as -1, 0, or 1.
     * @param v The value to check.
     * @return -1 if v < 0, 1 if v > 0, 0 if v == 0.
     */
    static int step(const double v) { return static_cast<int>(v > 0) - static_cast<int>(v < 0); }
    /**
     * @brief Rounds a double to the nearest integer.
     * @param v The value to round.
     * @return The rounded integer.
     */
    static int iround(const double v) { return static_cast<int>(std::round(v)); }

    /**
     * @brief Checks if an index is within the bounds of a sized range.
     * @tparam R A sized range type.
     * @tparam I An integral index type.
     * @param r The range to check.
     * @param i The index to check.
     * @return True if i is a valid index in r, false otherwise.
     */
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

    /**
     * @brief Concept for enum types.
     */
    template<typename T>
    concept Enum = std::is_enum_v<T>;

    /**
     * @brief Converts an enum value to its index in a mapping array.
     * @tparam E Enum type.
     * @tparam N Size of the mapping array.
     * @param value The enum value.
     * @param map The mapping array.
     * @return The index of the value in the map.
     * @throws std::out_of_range if value is not found.
     */
    template<Enum E, std::size_t N>
    constexpr int enum_to_idx(E value, const std::array<E, N> &map) {
        for (std::size_t i = 0; i < N; ++i)
            if (map[i] == value) {
                return static_cast<int>(i);
            }

        throw std::out_of_range("enum_to_idx: value not found in map");
    }

    /**
     * @brief Converts an index to its corresponding enum value in a mapping array.
     * @tparam E Enum type.
     * @tparam N Size of the mapping array.
     * @param idx The index.
     * @param map The mapping array.
     * @return The enum value at the given index.
     * @throws std::out_of_range if idx is out of range.
     */
    template<Enum E, std::size_t N>
    constexpr E idx_to_enum(int idx, const std::array<E, N> &map) {
        if (idx >= N || idx < 0) {
            throw std::out_of_range("idx_to_enum: index out of range");
        }

        return map[idx];
    }
} // namespace utils
