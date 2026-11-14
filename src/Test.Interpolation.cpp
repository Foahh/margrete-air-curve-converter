#include <catch2/catch_test_macros.hpp>
#include "mgxc/Interpolator.h"

TEST_CASE("Conversion fits height and applies snapped timing and offsets once", "[interpolation]") {
    Config config;
    config.tOffset = 100;
    config.xOffset = 2;
    config.yOffset = 10;
    config.chains.emplace_back();
    auto &chain = config.chains.back();
    chain.es = {EasingKind::Power, 2};
    chain.til = 3;
    chain.width = 2;
    chain.emplace_back(3, 4, 20, EasingMode::Linear, EasingMode::Out);
    chain.emplace_back(963, 4, 120, EasingMode::Linear, EasingMode::Linear);
    chain.emplace_back(1447, 8, 80, EasingMode::Linear, EasingMode::Linear);
    Interpolator interpolator(config);
    interpolator.Convert();
    const auto &notes = interpolator.GetNoteChains().at(0);
    REQUIRE(notes.front().tick == 105);
    REQUIRE(notes.front().x == 6);
    REQUIRE(notes.front().height == 30);
    REQUIRE(notes.back().tick == 1545);
    REQUIRE(notes.back().x == 10);
    REQUIRE(notes.back().height == 90);
    REQUIRE(notes.front().longAttr == MP_NOTELONGATTR_BEGIN);
    REQUIRE(notes.back().longAttr == MP_NOTELONGATTR_END);
    REQUIRE(notes.size() < 25);
    int seamCount = 0;
    for (std::size_t i = 0; i < notes.size(); ++i) {
        REQUIRE(notes[i].type == MP_NOTETYPE_AIRSLIDE);
        REQUIRE(notes[i].timelineId == 3);
        REQUIRE(notes[i].width == 2);
        if (notes[i].tick == 1065)
            ++seamCount;
        if (i > 0)
            REQUIRE(notes[i].tick > notes[i - 1].tick);
        if (i > 0 && i + 1 < notes.size())
            REQUIRE(notes[i].longAttr == MP_NOTELONGATTR_CONTROL);
    }
    REQUIRE(seamCount == 1);
    const auto size = notes.size();
    interpolator.Convert();
    REQUIRE(interpolator.GetNoteChains().size() == 1);
    REQUIRE(interpolator.GetNoteChains().front().size() == size);
}

TEST_CASE("Conversion retains AirCrush metadata and clamps after offsets", "[interpolation]") {
    Config config;
    config.xOffset = 20;
    config.yOffset = 300;
    config.chains.emplace_back();
    auto &chain = config.chains.back();
    chain.type = MP_NOTETYPE_AIRCRUSH;
    chain.emplace_back(0, 4, 20, EasingMode::Linear, EasingMode::In);
    chain.emplace_back(960, 4, 120, EasingMode::Linear, EasingMode::Linear);
    Interpolator interpolator(config);
    interpolator.Convert();
    for (const auto &note: interpolator.GetNoteChains().at(0)) {
        REQUIRE(note.x == 15);
        REQUIRE(note.width == 1);
        REQUIRE(note.height >= 320);
        REQUIRE(note.height <= 360);
        REQUIRE(note.type == MP_NOTETYPE_AIRCRUSH);
        REQUIRE(note.optionValue == MP_OPTIONVALUE_AIRCRUSH_TRACELIKE);
    }
    config.clamp = false;
    interpolator.Convert();
    REQUIRE(interpolator.GetNoteChains().front().back().height == 420);
    REQUIRE(interpolator.GetNoteChains().front().back().x == 24);
}

TEST_CASE("Invalid snap and collapsed legs are rejected", "[interpolation]") {
    Config config;
    config.chains.emplace_back();
    config.chains.back().emplace_back(0, 0, 20, EasingMode::Linear, EasingMode::In);
    config.chains.back().emplace_back(1, 0, 120, EasingMode::Linear, EasingMode::Linear);
    Interpolator interpolator(config);
    REQUIRE_THROWS_AS(interpolator.Convert(), std::invalid_argument);
    config.snap = 0;
    REQUIRE_THROWS_AS(interpolator.Convert(), std::invalid_argument);
}
