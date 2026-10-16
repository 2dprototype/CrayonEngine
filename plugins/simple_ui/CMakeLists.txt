# simple_ui plugin: native high-resolution Dear ImGui UI system immune to retro/post-process filters.
# Output: simple_ui.dll / simple_ui.so / simple_ui.dylib

set(_EXTRA_LIBS "")
if(WIN32)
    list(APPEND _EXTRA_LIBS "${CRAYON_ROOT}/vendor/sdl3/lib/libSDL3.dll.a" opengl32)
else()
    list(APPEND _EXTRA_LIBS SDL3 GL)
endif()

crayon_add_plugin(simpleUI
    SOURCES
        src/simple_ui_plugin.cpp
        src/simple_ui_core.cpp
        src/simple_ui_lua.cpp
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
        "${CRAYON_ROOT}/vendor/glad/include"
    LIBS
        ${_EXTRA_LIBS}
)
