#pragma once

#include <vector>

#include "EasingSolver.h"
#include "Note.h"

class Config {
public:
    EasingSolver solver;
    std::vector<std::vector<mgxc::Note>> chains;

    // all options
    MpInteger snap = 5;

    // parse options
    bool append{false};
    MpInteger width = 4;
    MpInteger til = 0;

    // convert options
    MpInteger tOffset = 0;
    MpInteger xOffset = 0;
    MpInteger yOffset = 0;
    bool clamp = true;
};
