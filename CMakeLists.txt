cmake_minimum_required(VERSION 3.20)
project(crayon_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options to separate crayon and editor builds
option(BUILD_CRAYON "Build the Crayon Engine Player (crayon.exe)" ON)
option(BUILD_EDITOR "Build the Crayon Project Manager (editor.exe)" ON)

# ---- Android ---------------------------------------------------------------
# Configure with the NDK toolchain (Gradle does this for you, see android/).
# On Android `crayon` becomes libmain.so (loaded by SDL's Java SDLActivity), the
# desktop-only editor is skipped, and the renderer uses OpenGL ES 3.0.
if(ANDROID)
    set(BUILD_EDITOR OFF CACHE BOOL "" FORCE)
    # SDL3 is built from source for Android (SDL's own CMake handles the NDK).
    #   android/setup_deps.sh clones it (tag release-3.4.16, same as the desktop vendor/sdl3).
    set(CRAYON_SDL3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3-src" CACHE PATH "SDL3 source tree (Android only)")
    # LuaJIT can't be built by this CMake project; android/setup_deps.sh builds it per ABI:
    #   vendor/android/<abi>/lib/libluajit-5.1.a
    set(CRAYON_ANDROID_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/android/${ANDROID_ABI}/lib" CACHE PATH "Prebuilt Android static libs")
    # GLES 3.0 needs no generated loader; vendor/glad_gles is a tiny shim over <GLES3/gl3.h>.
    set(CRAYON_GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad_gles")
else()
    set(CRAYON_GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad")
endif()

# Compiler optimization and warnings
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
elseif(ANDROID)
    # No -march: the NDK toolchain already targets the right ARM/x86 ABI.
    add_compile_options(-Wall -Wextra -O3)
else()
    add_compile_options(-Wall -Wextra -O3 -march=x86-64)
endif()

add_compile_definitions(
    GLM_ENABLE_EXPERIMENTAL
    JPH_DEBUG_RENDERER
    JPH_PROFILE_ENABLED
    JPH_OBJECT_STREAM
)

# Include directories
# NOTE: `src` is kept here for the crayon player target only.
# The editor target below overrides its own includes and does not rely on this.
include_directories(
    src
    editor
    icon
    ${CRAYON_GLAD_DIR}/include
    vendor/glm
    vendor/luajit/include
    vendor/stb/include
    vendor/tinyobj/include
    vendor/tinygltf/include
    vendor/JoltPhysics
    vendor/box2d/include
    vendor/imgui
    vendor/imgui/backends
)
if(NOT ANDROID)
    # Prebuilt desktop SDL3 headers. On Android the SDL3::SDL3 target provides its own.
    include_directories(vendor/sdl3/include)
endif()

# hv4d: the standalone 4D physics library (lives in vendor/, deliberately NOT part of src/).
# Provides the `hypervis4d` static-library target and its public include directory.
if(NOT TARGET hypervis4d)
    set(HV4D_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/hypervis4d)
endif()

file(GLOB_RECURSE BOX2D_SOURCES
    "vendor/box2d/src/*.cpp"
)

# Core Engine source files (shared between engine player and editor)
set(ENGINE_SOURCES
    ${CRAYON_GLAD_DIR}/src/glad.c
    vendor/tinygltf/src/tiny_gltf_v3.c
    ${BOX2D_SOURCES}
    src/core/window.cpp
    src/core/input.cpp
    src/core/audio_system.cpp
    src/core/engine.cpp
    src/graphics/fbo.cpp
    src/graphics/shader.cpp
    src/graphics/texture.cpp
    src/graphics/font.cpp
    src/graphics/canvas.cpp
    src/graphics/particles.cpp
    src/graphics/camera.cpp
    src/graphics/batch2d.cpp
    src/graphics/mesh3d.cpp
    src/graphics/model3d.cpp
    src/graphics/animator.cpp
    src/graphics/camera2d.cpp
    src/graphics/post_process_chain.cpp
    src/graphics/rich_text.cpp
    src/physics/physics_system.cpp
    src/physics/physics2d_system.cpp
    src/physics/physics4d_system.cpp
    src/scripting/lua_runtime.cpp
    src/scripting/bind_window.cpp
    src/scripting/bind_graphics.cpp
    src/scripting/bind_input.cpp
    src/scripting/bind_time.cpp
    src/scripting/bind_audio.cpp
    src/scripting/bind_physics3d.cpp
    src/scripting/bind_physics2d.cpp
    src/scripting/bind_physics4d.cpp
    src/scripting/bind_particles.cpp
    src/scripting/bind_math.cpp
    src/scripting/bind_fs.cpp
    src/scripting/bind_print.cpp
)
if(ANDROID)
    list(APPEND ENGINE_SOURCES src/core/android_storage.cpp)
endif()

set_source_files_properties(vendor/tinygltf/src/tiny_gltf_v3.c PROPERTIES
    COMPILE_DEFINITIONS "TINYGLTF3_ENABLE_FS"
)
if(NOT MSVC)
    set_source_files_properties(vendor/tinygltf/src/tiny_gltf_v3.c PROPERTIES
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable -Wno-missing-field-initializers"
    )
endif()

# Dear ImGui sources
set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
    vendor/imgui/misc/cpp/imgui_stdlib.cpp
)

# Editor sources (standalone project manager - no engine dependency)
set(EDITOR_SOURCES
    editor/editor_theme.cpp
    editor/project_store.cpp
    editor/process_runner.cpp
    editor/editor_app.cpp
    editor/main.cpp
)

# -------------------------------------------------------------
# Target 1: crayon -> crayon.exe (Standalone Runtime Player)
# -------------------------------------------------------------
if(BUILD_CRAYON)
    set(CRAYON_RESOURCES "")
    if(WIN32)
        set(CRAYON_RESOURCES icon/crayon.rc)
    endif()

    if(ANDROID)
        # ---- Android: shared library "libmain.so" ------------------------------
        # SDL's SDLActivity (Java) loads libSDL3.so then libmain.so and calls SDL_main().
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(${CRAYON_SDL3_SOURCE_DIR} ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL)

        # Jolt is compiled from source for the target ABI (vendor/JoltPhysics/CMakeLists.txt is
        # your wrapper). The engine defines JPH_DEBUG_RENDERER / JPH_PROFILE_ENABLED /
        # JPH_OBJECT_STREAM globally, so Jolt must be built with them in EVERY config,
        # otherwise class layouts differ between the engine and the library.
        set(DEBUG_RENDERER_IN_DISTRIBUTION ON CACHE BOOL "" FORCE)
        set(PROFILER_IN_DISTRIBUTION ON CACHE BOOL "" FORCE)
        set(ENABLE_OBJECT_STREAM ON CACHE BOOL "" FORCE)
        set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)   # keep the NDK's own flags
        set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
        set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
        set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
        set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
        add_subdirectory(vendor/JoltPhysics ${CMAKE_BINARY_DIR}/jolt EXCLUDE_FROM_ALL)

        add_library(crayon SHARED ${ENGINE_SOURCES} src/main.cpp)
        set_target_properties(crayon PROPERTIES
            OUTPUT_NAME "main"
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )

        target_link_libraries(crayon PRIVATE
            hypervis4d
            Jolt
            ${CRAYON_ANDROID_LIB_DIR}/libluajit-5.1.a
            SDL3::SDL3
            GLESv3
            EGL
            android
            log
        )
    else()
    add_executable(crayon ${ENGINE_SOURCES} src/main.cpp ${CRAYON_RESOURCES})

    set_target_properties(crayon PROPERTIES
        OUTPUT_NAME "crayon"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(crayon PRIVATE
        hypervis4d
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/JoltPhysics/lib/libJolt.a
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/lib/libSDL3.dll.a
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/lib/libluajit-5.1.dll.a
        opengl32
    )

    add_custom_command(TARGET crayon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/bin/SDL3.dll"
            "$<TARGET_FILE_DIR:crayon>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/bin/lua51.dll"
            "$<TARGET_FILE_DIR:crayon>"
    )
    endif()
endif()

# -------------------------------------------------------------
# Target 2: editor -> editor.exe (Standalone Project Manager)
# -------------------------------------------------------------
#
# The editor is a pure ImGui + SDL3 application. It launches crayon.exe
# as a child process and edits project files on disk. It does NOT link
# against the engine, Lua, or physics libraries, and it does NOT include
# anything from src/. GLAD is only needed to load GL function pointers
# for the ImGui OpenGL3 backend.
#
if(BUILD_EDITOR)
    add_executable(editor
        vendor/glad/src/glad.c
        ${IMGUI_SOURCES}
        ${EDITOR_SOURCES}
    )

    set_target_properties(editor PROPERTIES
        OUTPUT_NAME "editor"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Explicit includes for the editor only. Do NOT include `src` here:
    # if the editor ever tries to `#include "../src/..."` again, this
    # will fail at compile time instead of silently pulling in the engine.
    target_include_directories(editor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/editor
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/include
    )

    target_link_libraries(editor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/lib/libSDL3.dll.a
        opengl32
    )

    add_custom_command(TARGET editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/bin/SDL3.dll"
            "$<TARGET_FILE_DIR:editor>"
    )
endif()