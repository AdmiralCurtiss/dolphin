# Disable CLI building, which breaks for Android due to lack of 64-bit support
SET(LZ4_BUILD_CLI OFF CACHE BOOL "Build lz4 program")
SET(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "Build lz4c program with legacy argument support")

add_subdirectory(lz4/build/cmake)

target_include_directories(lz4_static PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(LZ4::LZ4 ALIAS lz4_static)