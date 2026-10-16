# CrayonPlugin.cmake - build helper for Crayon native plugins.
#
#   include(path/to/plugins/cmake/CrayonPlugin.cmake)
#   crayon_add_plugin(myplugin
#       SOURCES   src/myplugin.cpp ...
#       [INCLUDES dir ...]       # extra private include dirs
#       [LIBS     lib ...]       # extra link libraries (static libs, targets, ...)
#       [DEFINES  FOO=1 ...])
#
# Produces  <name>.dll (Windows) | <name>.so (Linux/Android) | <name>.dylib (macOS)  in
# ${CRAYON_PLUGIN_OUTPUT_DIR}, i.e. next to the crayon binary when built from the main project.
#
# What it takes care of
#   * only the plugin C ABI header (plugins/include) + the LuaJIT headers are on the include path
#   * Windows : links the same lua51.dll import library the engine uses (shared lua_State!)
#   * Linux   : lua_* symbols stay undefined and are resolved from the crayon binary at load time
#   * macOS   : same, via -undefined dynamic_lookup
#   * hidden symbol visibility - only crayon_plugin_entry (CRAYON_PLUGIN_EXPORT) is exported
#   * GCC/MinGW: libstdc++/libgcc are linked statically so no extra runtime DLLs are needed

get_filename_component(_CRAYON_ROOT_GUESS "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT CRAYON_ROOT)
    set(CRAYON_ROOT "${_CRAYON_ROOT_GUESS}")
endif()
if(NOT CRAYON_PLUGIN_OUTPUT_DIR)
    set(CRAYON_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}")
endif()

if(NOT TARGET crayon_plugins)
    add_custom_target(crayon_plugins)   # `cmake --build . --target crayon_plugins` builds them all
endif()

function(crayon_add_plugin name)
    cmake_parse_arguments(P "" "" "SOURCES;INCLUDES;LIBS;DEFINES" ${ARGN})
    if(NOT P_SOURCES)
        message(FATAL_ERROR "crayon_add_plugin(${name}): SOURCES is required")
    endif()

    add_library(${name} MODULE ${P_SOURCES})
    add_dependencies(crayon_plugins ${name})

    target_include_directories(${name} PRIVATE
        "${CRAYON_ROOT}/plugins/include"
        "${CRAYON_ROOT}/vendor/luajit/include"
        ${P_INCLUDES})
    target_compile_features(${name} PRIVATE cxx_std_20)
    if(P_DEFINES)
        target_compile_definitions(${name} PRIVATE ${P_DEFINES})
    endif()

    # <name>.dll / <name>.so / <name>.dylib  (no "lib" prefix: that is what t.plugins.<name> looks for)
    set_target_properties(${name} PROPERTIES
        PREFIX ""
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CRAYON_PLUGIN_OUTPUT_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CRAYON_PLUGIN_OUTPUT_DIR}>")
    if(APPLE)
        set_target_properties(${name} PROPERTIES SUFFIX ".dylib")
    endif()

    if(WIN32)
        set(_lua_implib "${CRAYON_ROOT}/vendor/luajit/lib/libluajit-5.1.dll.a")
        if(EXISTS "${_lua_implib}")
            target_link_libraries(${name} PRIVATE "${_lua_implib}")
        else()
            message(WARNING "crayon_add_plugin(${name}): LuaJIT import library not found at ${_lua_implib}")
        endif()
    elseif(APPLE)
        target_link_options(${name} PRIVATE -undefined dynamic_lookup)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND NOT APPLE AND NOT ANDROID)
        target_link_options(${name} PRIVATE -static-libstdc++ -static-libgcc)
    endif()

    if(P_LIBS)
        target_link_libraries(${name} PRIVATE ${P_LIBS})
    endif()
endfunction()
