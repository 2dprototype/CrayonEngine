#pragma once

// Platform / graphics-API feature macros.
//
//   CRAYON_PLATFORM_ANDROID  - building for Android (NDK)
//   CRAYON_PLATFORM_WINDOWS  - building for Windows
//   CRAYON_GLES              - renderer uses OpenGL ES 3.0 instead of desktop GL 3.3 core
//
// Everything platform-specific in the engine should key off these, not off
// raw compiler macros, so there is exactly one place to change.

#if defined(__ANDROID__)
    #define CRAYON_PLATFORM_ANDROID 1
    #define CRAYON_GLES 1
#elif defined(_WIN32)
    #define CRAYON_PLATFORM_WINDOWS 1
#endif
