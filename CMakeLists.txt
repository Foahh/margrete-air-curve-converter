cmake_minimum_required(VERSION 3.30)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DUNICODE -D_UNICODE)

file(STRINGS "config/VERSION" PROJECT_VERSION)
file(STRINGS "config/PROJECT" PROJECT_NAME)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

include_directories("src")
include_directories("src/aff")
include_directories("src/mgxc")

function(setup_metadata)
    set(REPO_URL "https://github.com/Foahh/margrete-air-curve-converter")

    set(JA_TITLE "Air カーブ変換ツール" PARENT_SCOPE)
    set(JA_DESC "曲線を AirCrush / AirSlide に変換する\\n${REPO_URL}" PARENT_SCOPE)

    set(EN_TITLE "Air Curve Converter" PARENT_SCOPE)
    set(EN_DESC "Convert curve to AirCrush / AirSlide\\n${REPO_URL}" PARENT_SCOPE)

    set(ZH_CN_TITLE "Air 曲线转换器" PARENT_SCOPE)
    set(ZH_CN_DESC "将曲线转换为 AirCrush / AirSlide\\n${REPO_URL}" PARENT_SCOPE)

    set(DEVELOPER "Foahh" PARENT_SCOPE)
endfunction()

function(generate_configurations)
    configure_file(config/meta.ini.in ${CMAKE_CURRENT_BINARY_DIR}/main.ini @ONLY)
    configure_file(config/meta.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/meta.h @ONLY)
    configure_file(config/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/include/version.rc @ONLY)

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
    message(STATUS "CONFIGURATION_PATH: ${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

function(setup_margrete_sdk)
    include(FetchContent)

    FetchContent_Declare(
            MargretePluginSDK
            GIT_REPOSITORY https://github.com/MargreteSDK/MargretePluginSDK.git
            GIT_TAG b8d0c87125e090880a75f6f0ff57a773510299a8
    )

    FetchContent_MakeAvailable(MargretePluginSDK)
    FetchContent_GetProperties(MargretePluginSDK SOURCE_DIR MargretePluginSDK_SOURCE_DIR)

    add_library(MargretePluginSDK INTERFACE)
    target_include_directories(MargretePluginSDK INTERFACE ${MargretePluginSDK_SOURCE_DIR}/include)

    set(MARGRETE_SDK MargretePluginSDK PARENT_SCOPE)
endfunction()

function(setup_common_interface)
    find_package(imgui CONFIG REQUIRED)
    find_path(WTL_INCLUDE_DIRS "atlapp.h")

    add_library(common INTERFACE)
    target_include_directories(common INTERFACE ${WTL_INCLUDE_DIRS})
    target_link_libraries(common INTERFACE comctl32 d3d11 imgui::imgui)
    target_link_libraries(common INTERFACE ${MARGRETE_SDK})

    if (MSVC)
        target_compile_options(common INTERFACE /EHsc /utf-8)
    endif ()
endfunction()

function(build_main_library)
    add_library(main SHARED
            src/DLLMain.cpp
            src/aff/Parser.cpp
            src/Dialog.cpp
            src/mgxc/Interpolator.cpp
            src/mgxc/EasingSolver.cpp
            src/mgxc/Margrete.cpp
            ${CMAKE_CURRENT_BINARY_DIR}/include/version.rc
    )
    target_link_libraries(main PRIVATE common)
endfunction()

function(build_tests)
    find_package(Catch2 3 REQUIRED)
    add_executable(tests
            src/Test.cpp
            src/aff/Parser.cpp
            src/Dialog.cpp
            src/mgxc/Interpolator.cpp
            src/mgxc/EasingSolver.cpp
            src/mgxc/Margrete.cpp
    )

    find_package(Catch2 CONFIG REQUIRED)
    target_link_libraries(tests PRIVATE common Catch2::Catch2 Catch2::Catch2WithMain)
endfunction()

setup_margrete_sdk()
setup_metadata()
generate_configurations()
setup_common_interface()
build_main_library()
build_tests()