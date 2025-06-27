#pragma once

#include <MargretePlugin.h>
#include <atomic>

#include "EasingSolver.h"

namespace mgxc {
    constexpr MpInteger BAR_TICKS = 1920;
    constexpr MpInteger BEAT_TICKS = BAR_TICKS / 4;

    class Note {
    public:
        MpInteger type{MP_NOTETYPE_AIRSLIDE};
        MpInteger t{0};
        MpInteger x{0};
        EasingMode eX{EasingMode::Linear};
        MpInteger y{80};
        EasingMode eY{EasingMode::Linear};
        MpInteger width{4};
        MpInteger til{0};

        explicit Note() = default;

        explicit Note(const MpInteger type, const MpInteger t, const MpInteger x, const EasingMode eX,
                      const MpInteger y, const EasingMode eY, const MpInteger width, const MpInteger til) :
            type(type), t(t), x(x), eX(eX), y(y), eY(eY), width(width), til(til) {}

        friend bool operator==(const Note &a, const Note &b) {
            return std::tie(a.type, a.t, a.x, a.eX, a.y, a.eY, a.width, a.til) ==
                   std::tie(b.type, b.t, b.x, b.eX, b.y, b.eY, b.width, b.til);
        }

        friend auto operator<=>(const Note &a, const Note &b) {
            return std::tie(a.type, a.t, a.x, a.eX, a.y, a.eY, a.width, a.til) <=>
                   std::tie(b.type, b.t, b.x, b.eX, b.y, b.eY, b.width, b.til);
        }

        std::size_t GetID() const noexcept { return id; }

    private:
        inline static std::atomic_size_t nextId{0};
        std::size_t id{nextId++};
    };
} // namespace mgxc
