# Crayon native plugins.
#
#   From the main project  : cmake --build build            (plugins are part of the normal build)
#                            cmake --build build --target crayon_plugins
#   Standalone             : plugins/build.bat | plugins/build.sh [plugin_name]
#
# Every sub-folder that contains a CMakeLists.txt is built as a plugin, so your own plugin only needs
# plugins/<name>/CMakeLists.txt calling crayon_add_plugin(<name> SOURCES ...).  See plugins/README.md.
cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(crayon_plugins LANGUAGES C CXX)
    set(CRAYON_PLUGINS_STANDALONE ON)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()
    if(MSVC)
        add_compile_options(/W4 /O2 /permissive-)
    else()
        add_compile_options(-Wall -Wextra -O3)
        if(NOT APPLE AND NOT ANDROID AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
            add_compile_options(-march=x86-64)
        endif()
    endif()
    set(CRAYON_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH "Where the plugin libraries are written")
else()
    set(CRAYON_PLUGINS_STANDALONE OFF)
    set(CRAYON_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}")   # next to crayon(.exe)
endif()

get_filename_component(CRAYON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CrayonPlugin.cmake")

file(GLOB _children RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(_c ${_children})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_c}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_c}/CMakeLists.txt")
        # -DCRAYON_PLUGIN_<name>=OFF skips one
        if(NOT DEFINED CRAYON_PLUGIN_${_c} OR CRAYON_PLUGIN_${_c})
            add_subdirectory(${_c})
        endif()
    endif()
endforeach()
