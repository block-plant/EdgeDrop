cmake_minimum_required(VERSION 3.22)
project(edgedrop_jni C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Root of the EdgeDrop repository (5 levels up from android/app/src/main/cpp)
get_filename_component(EDGEDROP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EDGEDROP_ROOT}/core/include
    ${EDGEDROP_ROOT}/core/src
    ${EDGEDROP_ROOT}/core/src/vendor
)

set(CORE_SOURCES
    ${EDGEDROP_ROOT}/core/src/format_detector.cpp
    ${EDGEDROP_ROOT}/core/src/security_guards.cpp
    ${EDGEDROP_ROOT}/core/src/vendor/lodepng.cpp
)

add_library(edgedrop_jni SHARED
    jni_bridge.cpp
    ${CORE_SOURCES}
)

target_include_directories(edgedrop_jni PUBLIC
    ${EDGEDROP_ROOT}/core/include
)

find_library(log-lib log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)
find_library(z-lib z)

target_link_libraries(edgedrop_jni
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
    ${z-lib}
)
