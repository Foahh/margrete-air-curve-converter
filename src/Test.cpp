#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "Dialog.h"
#include "aff/Parser.h"
#include "mgxc/Interpolator.h"

static Config g_cctx;
static IMargretePluginContext *g_ctx = nullptr;

TEST_CASE("Parse & Interpolate") {
    auto parser = aff::Parser(g_cctx);
    parser.ParseFile("../../../aff/2.aff");

    auto intp = Interpolator(g_cctx);
    intp.Convert();
}

TEST_CASE("Show Once") {
    auto dlg = Dialog(g_cctx, g_ctx, std::stop_token{});
    REQUIRE(dlg.ShowDialog() == S_OK);
}

TEST_CASE("Stop Dialog") {
    std::atomic_bool flag{false};

    auto worker = std::jthread([&flag](const std::stop_token &st) {
        auto dlg = Dialog(g_cctx, g_ctx, st);
        REQUIRE(dlg.ShowDialog() == S_OK);
        flag = !dlg.IsRunning();
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    worker.request_stop();
    worker.join();

    REQUIRE(flag);
}
