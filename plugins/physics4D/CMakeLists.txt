# physics4D plugin: 4D rigid-body physics (hv4d).  Output: physics4D.dll / physics4D.so / physics4D.dylib
set(HV4D_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)        # hv4d is a static lib linked into a shared one
if(NOT TARGET hypervis4d)
    add_subdirectory("${CRAYON_ROOT}/vendor/hypervis4d" "${CMAKE_CURRENT_BINARY_DIR}/hv4d")
endif()

crayon_add_plugin(physics4D
    SOURCES
        src/physics4d_plugin.cpp
        src/physics4d_system.cpp
        src/physics4d_lua.cpp
    INCLUDES
        src
        "${CRAYON_ROOT}/vendor/glm"
    DEFINES
        GLM_ENABLE_EXPERIMENTAL
    LIBS
        hypervis4d)
