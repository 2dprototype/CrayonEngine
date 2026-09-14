cmake_minimum_required(VERSION 3.20)
project(crayon_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options to separate crayon and editor builds
option(BUILD_CRAYON "Build the Crayon Engine Player (crayon.exe)" ON)
option(BUILD_EDITOR "Build the Crayon Game Editor (editor.exe)" ON)

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
include_directories(
    src
    editor
    vendor/glad/include
    vendor/glm
    vendor/sdl3/include
    vendor/luajit/include
    vendor/stb/include
    vendor/tinyobj/include
    vendor/tinygltf/include
    vendor/JoltPhysics
    vendor/imgui
    vendor/imgui/backends
)

# Core Engine source files (shared between engine player and editor)
set(ENGINE_SOURCES
    vendor/glad/src/glad.c
    vendor/tinygltf/src/tiny_gltf_v3.c
    src/core/window.cpp
    src/core/input.cpp
    src/core/engine.cpp
    src/graphics/fbo.cpp
    src/graphics/shader.cpp
    src/graphics/texture.cpp
    src/graphics/camera.cpp
    src/graphics/batch2d.cpp
    src/graphics/mesh3d.cpp
    src/graphics/model3d.cpp
    src/graphics/animator.cpp
    src/physics/physics_system.cpp
    src/scripting/lua_runtime.cpp
    src/scripting/bind_window.cpp
    src/scripting/bind_graphics.cpp
    src/scripting/bind_input.cpp
    src/scripting/bind_time.cpp
    src/scripting/bind_physics3d.cpp
    src/scripting/bind_physics2d.cpp
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

# Editor sources
set(EDITOR_SOURCES
    editor/editor_theme.cpp
    editor/editor_app.cpp
    editor/panels/viewport_panel.cpp
    editor/panels/scene_panel.cpp
    editor/panels/inspector_panel.cpp
    editor/panels/asset_browser_panel.cpp
    editor/panels/retro_fx_panel.cpp
    editor/panels/console_panel.cpp
    editor/panels/stats_panel.cpp
    editor/main.cpp
)

# -------------------------------------------------------------
# Target 1: crayon -> crayon.exe (Standalone Runtime Player)
# -------------------------------------------------------------
if(BUILD_CRAYON)
    add_executable(crayon ${ENGINE_SOURCES} src/main.cpp)

    set_target_properties(crayon PROPERTIES
        OUTPUT_NAME "crayon"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(crayon PRIVATE
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
# Target 2: editor -> editor.exe (Visual Game Editor IDE)
# -------------------------------------------------------------
if(BUILD_EDITOR)
    add_executable(editor ${ENGINE_SOURCES} ${IMGUI_SOURCES} ${EDITOR_SOURCES})

    set_target_properties(editor PROPERTIES
        OUTPUT_NAME "editor"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(editor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/JoltPhysics/lib/libJolt.a
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/lib/libSDL3.dll.a
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/lib/libluajit-5.1.dll.a
        opengl32
    )

    add_custom_command(TARGET editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/bin/SDL3.dll"
            "$<TARGET_FILE_DIR:editor>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/bin/lua51.dll"
            "$<TARGET_FILE_DIR:editor>"
    )
endif()
