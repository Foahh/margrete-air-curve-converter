#pragma once

#include <vector>

#include "Primitive.h"

/**
 * @class Config
 * @brief Stores configuration and options for the plugin's operation.
 *
 * Contains global context (chains), parsing options, and commit options.
 */
class Config {
public:
    /** Global context: list of note/control chains. */
    std::vector<mgxc::Chain> chains;

    /** Snap tick value for quantization. */
    MpInteger snap = 5;

    /** If true, append to existing data when parsing. */
    bool append{false};
    /** Default width for arc parsing. */
    MpInteger width = 4;
    /** Default TIL value for arc parsing. */
    MpInteger til = 0;

    /** Tick offset for commit operations. */
    MpInteger tOffset = 0;
    /** X offset for commit operations. */
    MpInteger xOffset = 0;
    /** Y offset for commit operations. */
    MpInteger yOffset = 0;
    /** If true, clamp (x, y) values during commit. */
    bool clamp = true;
};
