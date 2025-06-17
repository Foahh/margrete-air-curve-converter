#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>

#include "Dialog.h"
#include "aff/Parser.h"
#include "mgxc/Interpolator.h"

static Config g_cctx;

TEST_CASE("Parse & Interpolate") {
    auto parser = aff::Parser(g_cctx);
    parser.ParseFile("../../../example/2.aff");

    std::cout << "Parsed " << g_cctx.chains.size() << " chains:" << std::endl;
    auto i = 0;
    for (const auto &chain: g_cctx.chains) {
        std::cout << i++ << ":" << std::endl;
        for (const auto &note: chain) {
            std::cout << "  t=" << note.t << ", x=" << note.x << ", y=" << note.y
                      << ", eX=" << static_cast<int>(note.eX) << ", eY=" << static_cast<int>(note.eY) << std::endl;
        }
    }

    auto intp = Interpolator(g_cctx);
    intp.Convert();

    std::cout << std::endl << "Interpolated " << intp.m_notes.size() << " chains:" << std::endl;
    i = 0;
    for (const auto &chain: intp.m_notes) {
        std::cout << i++ << ":" << std::endl;
        for (const auto &note: chain) {
            std::cout << "  a=" << note.longAttr << ", t=" << note.tick << ", x=" << note.x << ", h=" << note.height
                      << std::endl;
        }
    }
}

TEST_CASE("Show Once") {
    static auto g_dlg = std::make_unique<Dialog>(g_cctx, nullptr, std::stop_token{});
    REQUIRE(g_dlg->ShowDialog() == S_OK);
}

TEST_CASE("Show Multiple Times") {
    static auto g_dlg = std::make_unique<Dialog>(g_cctx, nullptr, std::stop_token{});

    REQUIRE(g_dlg->ShowDialog() == S_OK);
    REQUIRE(g_dlg->ShowDialog() == S_OK);
    REQUIRE(g_dlg->ShowDialog() == S_OK);
    REQUIRE(g_dlg->ShowDialog() == S_OK);
    REQUIRE(g_dlg->ShowDialog() == S_OK);
}
