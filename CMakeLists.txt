cmake_minimum_required(VERSION 3.22)

# Enable CXX and OBJCXX on Apple
if(APPLE)
    project(EdgeDrop VERSION 3.0.0 LANGUAGES CXX OBJCXX)
else()
    project(EdgeDrop VERSION 3.0.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(EDGEDROP_BUILD_DESKTOP "Build Qt 6 desktop application" ON)
option(EDGEDROP_BUILD_TESTS "Build test suite" ON)

# Common Homebrew prefix fallback on macOS Apple Silicon / Intel
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew/opt/qt"
        "/opt/homebrew/opt/opencv"
        "/opt/homebrew/opt/nlohmann-json"
        "/opt/homebrew/opt/qpdf"
        "/usr/local/opt/qt"
        "/usr/local/opt/opencv"
        "/usr/local/opt/nlohmann-json"
        "/usr/local/opt/qpdf"
    )
endif()

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

macro(add_cxx_flag_if_supported flag)
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var_name "FLAG_${flag}")
    check_cxx_compiler_flag("${flag}" ${var_name})
    if(${var_name})
        add_compile_options(${flag})
    endif()
endmacro()

macro(add_link_flag_if_supported flag)
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var_name "LINK_FLAG_${flag}")
    check_linker_flag(CXX "${flag}" ${var_name})
    if(${var_name})
        add_link_options(${flag})
    endif()
endmacro()

# ══════════════════════════════════════════════════════════════════════════════
# Compiler, Linker & Binary Hardening
# ══════════════════════════════════════════════════════════════════════════════

if(MSVC)
    add_compile_options(/W4 /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_compile_options(/GS /guard:cf /sdl)
        add_link_options(/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
        add_link_flag_if_supported(/CETCOMPAT)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    add_cxx_flag_if_supported("-Wformat=2")
    add_cxx_flag_if_supported("-Werror=format-security")
    add_cxx_flag_if_supported("-fvisibility=hidden")
    add_cxx_flag_if_supported("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
        add_cxx_flag_if_supported("-fstack-protector-strong")
        add_compile_definitions($<$<CONFIG:Release>:_FORTIFY_SOURCE=2>)
        add_cxx_flag_if_supported("-fno-delete-null-pointer-checks")
        add_cxx_flag_if_supported("-ftrivial-auto-var-init=zero")
        add_cxx_flag_if_supported("-fPIE")
        if(APPLE)
            add_link_flag_if_supported("-Wl,-dead_strip")
        elseif(UNIX)
            add_link_flag_if_supported("-Wl,-z,relro")
            add_link_flag_if_supported("-Wl,-z,now")
            add_link_flag_if_supported("-Wl,-z,noexecstack")
            add_link_flag_if_supported("-pie")
        endif()
    endif()
endif()

# Find native compression dependencies (ZERO Qt required for core)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json REQUIRED)
endif()

find_library(QPDF_LIB qpdf
    HINTS "/opt/homebrew/opt/qpdf/lib" "/usr/local/opt/qpdf/lib"
    REQUIRED
)
find_path(QPDF_INCLUDE_DIR qpdf/QPDF.hh
    HINTS "/opt/homebrew/opt/qpdf/include" "/usr/local/opt/qpdf/include"
    REQUIRED
)

find_package(ZLIB REQUIRED)

# ══════════════════════════════════════════════════════════════════════════════
# 1. Pure C++20 Core Library (libedgedrop_core — ZERO Qt headers)
# ══════════════════════════════════════════════════════════════════════════════

set(CORE_SOURCES
    core/src/format_detector.cpp
    core/src/security_guards.cpp
    core/src/image_compressor.cpp
    core/src/pdf_compressor.cpp
    core/src/office_compressor.cpp
    core/src/media_compressor.cpp
    core/src/compressor_router.cpp
    core/src/default_subprocess_runner.cpp
    core/src/c_api.cpp
    core/src/vendor/lodepng.cpp
    # Legacy helpers for backwards compatibility
    core/src/legacy/JfifDpiPatcher.cpp
    core/src/legacy/SignatureCleaner.cpp
    core/src/legacy/PortraitEnhancer.cpp
    core/src/legacy/ImageProcessor.cpp
    core/src/legacy/PdfProcessor.cpp
)

add_library(edgedrop_core STATIC ${CORE_SOURCES})

target_include_directories(edgedrop_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core/src
    ${CMAKE_CURRENT_SOURCE_DIR}/core/src/legacy
    ${OpenCV_INCLUDE_DIRS}
    ${QPDF_INCLUDE_DIR}
)

target_link_libraries(edgedrop_core PUBLIC
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    ${QPDF_LIB}
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

if(APPLE)
    target_link_options(edgedrop_core PUBLIC "-Wl,-dead_strip_dylibs")
    target_link_libraries(edgedrop_core PUBLIC
        "-framework ImageIO"
        "-framework CoreFoundation"
    )
endif()

# ══════════════════════════════════════════════════════════════════════════════
# 2. Desktop Shell (Qt 6 + QML)
# ══════════════════════════════════════════════════════════════════════════════

if(EDGEDROP_BUILD_DESKTOP)
    find_package(Qt6 6.5 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
        QuickEffects
        Widgets
    )

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(DESKTOP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/desktop/src")
    set(DESKTOP_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/desktop/qml")

    set(DESKTOP_SOURCES
        ${DESKTOP_SRC_DIR}/app/PresetManager.cpp
        ${DESKTOP_SRC_DIR}/app/SettingsManager.cpp
        ${DESKTOP_SRC_DIR}/app/EdgeWindowManager.cpp
        ${DESKTOP_SRC_DIR}/app/WallpaperThemeEngine.cpp
        ${DESKTOP_SRC_DIR}/app/AppController.cpp
        ${DESKTOP_SRC_DIR}/engine/JobRunner.cpp
        ${DESKTOP_SRC_DIR}/engine/QtSubprocessRunner.cpp
    )

    if(APPLE)
        list(APPEND DESKTOP_SOURCES ${DESKTOP_SRC_DIR}/platform/PlatformHelper_mac.mm)
    elseif(WIN32)
        list(APPEND DESKTOP_SOURCES ${DESKTOP_SRC_DIR}/platform/PlatformHelper_win.cpp)
    else()
        list(APPEND DESKTOP_SOURCES ${DESKTOP_SRC_DIR}/platform/PlatformHelper_linux.cpp)
    endif()

    add_library(edgedrop_desktop_lib STATIC ${DESKTOP_SOURCES})
    target_include_directories(edgedrop_desktop_lib PUBLIC
        ${DESKTOP_SRC_DIR}
    )
    target_link_libraries(edgedrop_desktop_lib PUBLIC
        edgedrop_core
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickEffects
        Qt6::Widgets
    )
    if(APPLE)
        target_link_libraries(edgedrop_desktop_lib PUBLIC
            "-framework Cocoa"
            "-framework CoreServices"
        )
    endif()

    qt_add_executable(edgedrop
        ${DESKTOP_SRC_DIR}/main.cpp
    )

    set_source_files_properties(desktop/qml/Theme.qml
        PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
    )

    qt_add_qml_module(edgedrop
        URI EdgeDrop
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        QML_FILES
            desktop/qml/Main.qml
            desktop/qml/Theme.qml
            desktop/qml/components/EdgeHandle.qml
            desktop/qml/components/DropZoneCard.qml
            desktop/qml/components/ConfigPanel.qml
            desktop/qml/components/ResultCard.qml
            desktop/qml/components/SettingsPopover.qml
            desktop/qml/components/TintedIcon.qml
            desktop/qml/components/AppToolTip.qml
        RESOURCES
            resources/presets.json
            resources/icons/drop.svg
            resources/icons/close.svg
            resources/icons/pin.svg
            resources/icons/settings.svg
            resources/icons/check.svg
            resources/icons/copy.svg
            resources/icons/folder.svg
            resources/icons/photo.svg
            resources/icons/signature.svg
            resources/icons/document.svg
            resources/icons/drag_handle.svg
            resources/icons/quit.svg
    )

    target_link_libraries(edgedrop PRIVATE
        edgedrop_desktop_lib
        edgedrop_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Widgets
    )

    if(APPLE)
        target_link_options(edgedrop PRIVATE "-Wl,-dead_strip_dylibs")
        set_target_properties(edgedrop PROPERTIES
            MACOSX_BUNDLE YES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist"
        )
    endif()
endif()

# ══════════════════════════════════════════════════════════════════════════════
# 3. Test Suite
# ══════════════════════════════════════════════════════════════════════════════

if(EDGEDROP_BUILD_TESTS)
    enable_testing()

    # Pure C++ Core Tests (Zero Qt dependency)
    add_executable(test_format_detector tests/test_format_detector.cpp)
    target_link_libraries(test_format_detector PRIVATE edgedrop_core)
    add_test(NAME test_format_detector COMMAND test_format_detector)

    add_executable(test_image_compressor tests/test_image_compressor.cpp)
    target_link_libraries(test_image_compressor PRIVATE edgedrop_core)
    add_test(NAME test_image_compressor COMMAND test_image_compressor)

    add_executable(test_pdf_compressor tests/test_pdf_compressor.cpp)
    target_link_libraries(test_pdf_compressor PRIVATE edgedrop_core)
    add_test(NAME test_pdf_compressor COMMAND test_pdf_compressor)

    add_executable(test_office_compressor tests/test_office_compressor.cpp)
    target_link_libraries(test_office_compressor PRIVATE edgedrop_core)
    add_test(NAME test_office_compressor COMMAND test_office_compressor)

    add_executable(test_concurrency_and_safety tests/test_concurrency_and_safety.cpp)
    target_link_libraries(test_concurrency_and_safety PRIVATE edgedrop_core)
    add_test(NAME test_concurrency_and_safety COMMAND test_concurrency_and_safety)

    add_executable(test_binary_search_compressor tests/test_binary_search_compressor.cpp)
    target_link_libraries(test_binary_search_compressor PRIVATE edgedrop_core)
    add_test(NAME test_binary_search_compressor COMMAND test_binary_search_compressor)

    add_executable(test_jfif_dpi_patcher tests/test_jfif_dpi_patcher.cpp)
    target_link_libraries(test_jfif_dpi_patcher PRIVATE edgedrop_core)
    add_test(NAME test_jfif_dpi_patcher COMMAND test_jfif_dpi_patcher)

    add_executable(test_signature_cleaner tests/test_signature_cleaner.cpp)
    target_link_libraries(test_signature_cleaner PRIVATE edgedrop_core)
    add_test(NAME test_signature_cleaner COMMAND test_signature_cleaner)

    add_executable(test_security tests/test_security.cpp)
    target_link_libraries(test_security PRIVATE edgedrop_core)
    add_test(NAME test_security COMMAND test_security)

    if(EDGEDROP_BUILD_DESKTOP)
        add_executable(test_preset_validation tests/test_preset_validation.cpp)
        target_link_libraries(test_preset_validation PRIVATE edgedrop_desktop_lib edgedrop_core Qt6::Core)
        add_test(NAME test_preset_validation COMMAND test_preset_validation)
    endif()
endif()
