cmake_minimum_required(VERSION 3.20)
project(crayon_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options to separate crayon and editor builds
option(BUILD_CRAYON "Build the Crayon Engine Player (crayon.exe)" ON)
option(BUILD_EDITOR "Build the Crayon Project Manager (editor.exe)" ON)

# Compiler optimization and warnings
if(MSVC)
    add_compile_options(/W4 /O2 /permissive-)
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
    vendor/glad/include
    vendor/glm
    vendor/sdl3/include
    vendor/luajit/include
    vendor/stb/include
    vendor/tinyobj/include
    vendor/tinygltf/include
    vendor/JoltPhysics
    vendor/box2d/include
    vendor/imgui
    vendor/imgui/backends
)

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
    vendor/glad/src/glad.c
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