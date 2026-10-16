# devKit plugin: in-game developer overlay & live debugger for Crayon Engine.
# Output: devKit.dll / devKit.so / devKit.dylib

set(_EXTRA_LIBS "")
if(WIN32)
    list(APPEND _EXTRA_LIBS "${CRAYON_ROOT}/vendor/sdl3/lib/libSDL3.dll.a" opengl32)
else()
    list(APPEND _EXTRA_LIBS SDL3 GL)
endif()

crayon_add_plugin(devKit
    SOURCES
        src/devkit_plugin.cpp
        src/devkit_core.cpp
        src/devkit_lua.cpp
        "${CRAYON_ROOT}/vendor/imgui/imgui.cpp"
        "${CRAYON_ROOT}/vendor/imgui/imgui_draw.cpp"
        "${CRAYON_ROOT}/vendor/imgui/imgui_tables.cpp"
        "${CRAYON_ROOT}/vendor/imgui/imgui_widgets.cpp"
        "${CRAYON_ROOT}/vendor/imgui/backends/imgui_impl_sdl3.cpp"
        "${CRAYON_ROOT}/vendor/imgui/backends/imgui_impl_opengl3.cpp"
    INCLUDES
        src
        "${CRAYON_ROOT}/vendor/imgui"
        "${CRAYON_ROOT}/vendor/imgui/backends"
        "${CRAYON_ROOT}/vendor/sdl3/include"
    LIBS
        ${_EXTRA_LIBS}
)
