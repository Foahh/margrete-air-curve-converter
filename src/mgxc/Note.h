#pragma once

#include <MargretePlugin.h>

#include "EasingSolver.h"

namespace mgxc {
    constexpr MpInteger BAR_TICKS = 1920;
    constexpr MpInteger BEAT_TICKS = BAR_TICKS / 4;

    struct Note {
        MpInteger type{MP_NOTETYPE_AIRSLIDE};
        MpInteger t{0};
        MpInteger x{0};
        EasingMode eX{EasingMode::Linear};
        MpInteger y{80};
        EasingMode eY{EasingMode::Linear};

        MpInteger width{4};
        MpInteger til{0};

        friend constexpr auto operator<=>(const Note &, const Note &) = default;
    };
} // namespace mgxc
