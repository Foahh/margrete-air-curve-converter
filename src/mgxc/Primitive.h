#pragma once

#include <algorithm>
#include <atomic>
#include <format>
#include <sstream>
#include <string>
#include <vector>

#include <MargretePlugin.h>

#include "Easing.h"
#include "Utils.h"

namespace mgxc {
    /**
     * @brief Number of ticks in a bar.
     */
    constexpr MpInteger BAR_TICKS = 1920;
    /**
     * @brief Number of ticks in a beat.
     */
    constexpr MpInteger BEAT_TICKS = BAR_TICKS / 4;

    /**
     * @class Joint
     * @brief Represents a note/control point in a chain.
     *
     * Stores timing, position, and easing information for a single point.
     */
    class Joint {
    public:
        MpInteger t{0}; /**< Tick position. */
        MpInteger x{0}; /**< X position. */
        MpInteger y{80}; /**< Y position. */
        EasingMode eX{EasingMode::Linear}; /**< Easing mode for X. */
        EasingMode eY{EasingMode::Linear}; /**< Easing mode for Y. */

        /**
         * @brief Default constructor.
         */
        explicit Joint() = default;

        /**
         * @brief Constructs a Joint with all fields specified.
         * @param t Tick position.
         * @param x X position.
         * @param y Y position.
         * @param eX Easing mode for X.
         * @param eY Easing mode for Y.
         */
        explicit Joint(const MpInteger t, const MpInteger x, const MpInteger y, const EasingMode eX,
                       const EasingMode eY) : t(t), x(x), y(y), eX(eX), eY(eY) {}

        /**
         * @brief Equality operator for Joint.
         */
        friend bool operator==(const Joint &a, const Joint &b) {
            return std::tie(a.t, a.x, a.y) == std::tie(b.t, b.x, b.y);
        }

        /**
         * @brief Three-way comparison operator for Joint.
         */
        friend auto operator<=>(const Joint &a, const Joint &b) {
            return std::tie(a.t, a.x, a.y) <=> std::tie(b.t, b.x, b.y);
        }

        /**
         * @brief Returns a Joint snapped to the given tick value.
         * @param snap Snap value.
         * @return Snapped Joint.
         */
        Joint Snap(const MpInteger snap) const {
            const auto sT = utils::iround(static_cast<double>(t) / snap);
            return Joint{sT, x, y, eX, eY};
        }

        /**
         * @brief Gets the unique ID of the Joint.
         * @return The unique ID.
         */
        std::size_t GetID() const noexcept { return id; }

    private:
        inline static std::atomic_size_t nextId{0}; /**< Static counter for unique IDs. */
        std::size_t id{nextId++}; /**< Unique ID for this Joint. */
    };

    class Chain {
    public:
        MpInteger type{MP_NOTETYPE_AIRSLIDE};
        MpInteger width{4};
        MpInteger til{0};
        Easing es{EasingKind::Sine, 0};
        std::vector<Joint> joints{};

        template<class... Args>
        decltype(auto) emplace_back(Args &&...args) noexcept(
                noexcept(std::declval<std::vector<Joint> &>().emplace_back(std::forward<Args>(args)...))) {
            return joints.emplace_back(std::forward<Args>(args)...);
        }

        decltype(auto) push_back(const Joint &n) noexcept(noexcept(std::declval<std::vector<Joint> &>().push_back(n))) {
            return joints.push_back(n);
        }

        decltype(auto) begin() noexcept { return joints.begin(); }
        decltype(auto) begin() const noexcept { return joints.begin(); }
        decltype(auto) end() noexcept { return joints.end(); }
        decltype(auto) end() const noexcept { return joints.end(); }

        decltype(auto) front() noexcept(noexcept(joints.front())) { return joints.front(); }
        decltype(auto) front() const noexcept(noexcept(joints.front())) { return joints.front(); }
        decltype(auto) back() noexcept(noexcept(joints.back())) { return joints.back(); }
        decltype(auto) back() const noexcept(noexcept(joints.back())) { return joints.back(); }

        decltype(auto) operator[](const std::size_t i) noexcept { return joints[i]; }
        decltype(auto) operator[](const std::size_t i) const noexcept { return joints[i]; }

        constexpr bool empty() const noexcept { return joints.empty(); }
        constexpr std::size_t size() const noexcept { return joints.size(); }

        void sort() {
            std::ranges::stable_sort(joints, [](const Joint &a, const Joint &b) { return a.t < b.t; });
        }
    };

} // namespace mgxc

namespace mgxc::data {
    inline std::string Serialize(const Joint &n) {
        return std::format("({},{},{},{},{})\n", n.t, n.x, n.y, static_cast<char>(n.eX), static_cast<char>(n.eY));
    }

    inline std::string Serialize(const Chain &chain) {
        if (chain.empty()) {
            return {};
        }

        std::ostringstream oss;

        oss << std::format("[{},{},{},{},{}]\n", chain.type, chain.width, chain.til, static_cast<char>(chain.es.m_kind),
                           chain.es.m_param);

        for (const auto &n: chain) {
            oss << Serialize(n);
        }
        return oss.str();
    }

    inline std::string Serialize(const std::vector<Chain> &chains) {
        if (chains.empty()) {
            return {};
        }

        std::ostringstream oss;
        for (const auto &c: chains) {
            oss << Serialize(c);
            if (&c != &chains.back()) {
                oss << "\n";
            }
        }
        return oss.str();
    }

    inline std::vector<Chain> Parse(const std::string &text) {
        std::vector<Chain> chains;
        std::vector<std::string> buf;
        std::istringstream in{text};

        const auto flush = [&] {
            if (buf.empty()) {
                return;
            }

            Chain chain;

            {
                std::istringstream hd(buf.front());
                char bL, c1, c2, c3, c4, bR;
                char eK;
                if (!(hd >> bL >> chain.type >> c1 >> chain.width >> c2 >> chain.til >> c3 >> eK >> c4 >>
                      chain.es.m_param >> bR)) {
                    throw std::invalid_argument("Invalid header format: " + buf.front());
                }
                if (bL != '[' || c1 != ',' || c2 != ',' || c3 != ',' || c4 != ',' || bR != ']') {
                    throw std::invalid_argument("Invalid header format: " + buf.front());
                }
                chain.es.m_kind = static_cast<EasingKind>(eK);
            }

            for (std::size_t i = 1; i < buf.size(); ++i) {
                if (buf[i].empty()) {
                    continue;
                }

                int t, x, y;
                char eX, eY;
                {
                    std::istringstream bd(buf[i]);
                    char pL, c1, c2, c3, c4, pR;
                    if (!(bd >> pL >> t >> c1 >> x >> c2 >> y >> c3 >> eX >> c4 >> eY >> pR)) {
                        throw std::invalid_argument("Invalid body format: " + buf[i]);
                    }
                    if (pL != '(' || c1 != ',' || c2 != ',' || c3 != ',' || c4 != ',' || pR != ')') {
                        throw std::invalid_argument("Invalid body format: " + buf[i]);
                    }
                }

                chain.emplace_back(t, x, y, static_cast<EasingMode>(eX), static_cast<EasingMode>(eY));
            }

            if (!chain.empty()) {
                chain.sort();
                chains.push_back(std::move(chain));
            }

            buf.clear();
        };

        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) {
                flush();
                continue;
            }

            if (!line.empty() && line.front() == '[') {
                flush();
                buf.push_back(std::move(line));
            } else if (!buf.empty()) {
                buf.push_back(std::move(line));
            }
        }
        flush();

        return chains;
    }

} // namespace mgxc::data
