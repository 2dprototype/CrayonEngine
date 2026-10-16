# activation plugin: offline license-key activation.  Output: activation.dll / .so / .dylib
# plus tools/crayon_keygen(.exe) to generate keys.
#
# IMPORTANT: set your own secret (it signs every key; plugin and keygen must share it):
#     cmake -S . -B build -DCRAYON_ACTIVATION_SECRET="some-long-random-string"
set(CRAYON_ACTIVATION_SECRET "" CACHE STRING "Secret used to sign / verify license keys")
if(CRAYON_ACTIVATION_SECRET STREQUAL "")
    set(_act_secret "CHANGE-ME-set-CRAYON_ACTIVATION_SECRET-in-cmake")
    message(WARNING "activation: CRAYON_ACTIVATION_SECRET is not set - using an insecure default. "
                    "Configure with -DCRAYON_ACTIVATION_SECRET=<long random string> before shipping.")
else()
    set(_act_secret "${CRAYON_ACTIVATION_SECRET}")
endif()
set(_act_def "CRAYON_ACTIVATION_SECRET=\"${_act_secret}\"")

crayon_add_plugin(activation
    SOURCES
        src/activation_plugin.cpp
        src/activation_core.cpp
    INCLUDES
        src
    DEFINES
        "${_act_def}")
if(WIN32)
    target_link_libraries(activation PRIVATE advapi32)
endif()

add_executable(crayon_keygen tools/keygen.cpp src/activation_core.cpp)
target_include_directories(crayon_keygen PRIVATE src)
target_compile_features(crayon_keygen PRIVATE cxx_std_20)
target_compile_definitions(crayon_keygen PRIVATE "${_act_def}")
if(WIN32)
    target_link_libraries(crayon_keygen PRIVATE advapi32)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND NOT APPLE)
    target_link_options(crayon_keygen PRIVATE -static-libstdc++ -static-libgcc)
endif()
set_target_properties(crayon_keygen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CRAYON_PLUGIN_OUTPUT_DIR}/tools>")
add_dependencies(crayon_plugins crayon_keygen)
add_dependencies(activation crayon_keygen)
