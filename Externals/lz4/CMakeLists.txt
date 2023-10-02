# We only want the static library, nothing else.
set(LZ4_BUILD_CLI OFF)
set(LZ4_BUILD_LEGACY_LZ4C OFF)
set(LZ4_BUNDLED_MODE ON)

add_subdirectory(lz4/build/cmake)

dolphin_disable_warnings_msvc(lz4_static)
add_library(LZ4::LZ4 ALIAS lz4_static)

