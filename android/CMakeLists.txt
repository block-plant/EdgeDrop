cmake_minimum_required(VERSION 3.22)
project(edgedrop_android C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android NDK JNI Bridge for EdgeDrop Core
# ZERO Qt headers or dependencies

set(EDGEDROP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Include directories
include_directories(
    ${EDGEDROP_ROOT}/core/include
    ${EDGEDROP_ROOT}/core/src
    ${EDGEDROP_ROOT}/core/src/legacy
    ${EDGEDROP_ROOT}/core/src/vendor
)

# Core sources
file(GLOB CORE_SOURCES
    ${EDGEDROP_ROOT}/core/src/*.cpp
    ${EDGEDROP_ROOT}/core/src/legacy/*.cpp
    ${EDGEDROP_ROOT}/core/src/vendor/*.cpp
)

add_library(edgedrop_jni SHARED
    jni/edgedrop_jni.cpp
    ${CORE_SOURCES}
)

target_include_directories(edgedrop_jni PUBLIC
    ${EDGEDROP_ROOT}/core/include
)

# Android system libraries
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(edgedrop_jni
    ${log-lib}
    ${android-lib}
)
