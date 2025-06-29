#pragma once

#include <vector>

#include "Primitive.h"

class Config {
public:
    // global context
    std::vector<mgxc::Chain> chains;

    // common options
    MpInteger snap = 5;

    // arc parse options
    bool append{false};
    MpInteger width = 4;
    MpInteger til = 0;

    // commit options
    MpInteger tOffset = 0;
    MpInteger xOffset = 0;
    MpInteger yOffset = 0;
    bool clamp = true;
};
