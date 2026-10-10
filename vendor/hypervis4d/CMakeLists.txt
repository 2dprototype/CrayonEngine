# hypervis4d (hv4d) - a standalone 4D rigid-body physics library.
#
# A C++ re-creation of the "hypervis" 4D physics sandbox (originally written in Rust by
# Tyler Zhang), as a self-contained static library with NO dependency on the Crayon engine,
# a renderer, or any third-party code.
#
# Use from another CMake project:
#     add_subdirectory(vendor/hypervis4d)
#     target_link_libraries(my_target PRIVATE hypervis4d)      # or hypervis4d::hypervis4d
#
# Build + run the self-tests on their own:
#     cmake -S vendor/hypervis4d -B build_hv4d -DHV4D_BUILD_TESTS=ON
#     cmake --build build_hv4d && ctest --test-dir build_hv4d --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(hypervis4d VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(HV4D_IS_TOP_LEVEL ON)
else()
    set(HV4D_IS_TOP_LEVEL OFF)
endif()

option(HV4D_BUILD_TESTS "Build the hv4d self-test executable" ${HV4D_IS_TOP_LEVEL})

add_library(hypervis4d STATIC
    src/math.cpp
    src/todd_coxeter.cpp
    src/mesh.cpp
    src/clip.cpp
    src/physics.cpp
    src/collision.cpp
    src/gjk.cpp
    src/slice.cpp
)
add_library(hypervis4d::hypervis4d ALIAS hypervis4d)

target_include_directories(hypervis4d PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hypervis4d PUBLIC cxx_std_17)

if(HV4D_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(HV4D_BUILD_TESTS)
    add_executable(hv4d_tests tests/test_hv4d.cpp)
    target_link_libraries(hv4d_tests PRIVATE hypervis4d)
    if(HV4D_IS_TOP_LEVEL)
        enable_testing()
    endif()
    add_test(NAME hv4d_tests COMMAND hv4d_tests)
endif()
