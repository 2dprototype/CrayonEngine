cmake_minimum_required(VERSION 3.20)
project(crayon_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    vendor/glad/include
    vendor/glm
    vendor/sdl3/include
    vendor/luajit/include
    vendor/stb/include
    vendor/tinyobj/include
    vendor/tinygltf/include
    vendor/JoltPhysics
)

# Source files
set(SOURCES
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
    src/physics/physics_system.cpp
    src/scripting/lua_runtime.cpp
    src/scripting/bind_window.cpp
    src/scripting/bind_graphics.cpp
    src/scripting/bind_input.cpp
    src/scripting/bind_time.cpp
    src/scripting/bind_physics3d.cpp
    src/scripting/bind_physics2d.cpp
    src/main.cpp
)

set_source_files_properties(vendor/tinygltf/src/tiny_gltf_v3.c PROPERTIES
    COMPILE_DEFINITIONS "TINYGLTF3_ENABLE_FS"
)
if(NOT MSVC)
    set_source_files_properties(vendor/tinygltf/src/tiny_gltf_v3.c PROPERTIES
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable -Wno-missing-field-initializers"
    )
endif()

add_executable(crayon_engine ${SOURCES})

set_target_properties(crayon_engine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Link libraries
target_link_libraries(crayon_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/JoltPhysics/lib/libJolt.a
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/lib/libSDL3.dll.a
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/lib/libluajit-5.1.dll.a
    opengl32
)

# Post-build: copy required DLLs to binary directory
add_custom_command(TARGET crayon_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3/bin/SDL3.dll"
        "$<TARGET_FILE_DIR:crayon_engine>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/luajit/bin/lua51.dll"
        "$<TARGET_FILE_DIR:crayon_engine>"
)
