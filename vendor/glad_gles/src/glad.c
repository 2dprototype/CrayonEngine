/* Intentionally empty: on Android the GLES 3.0 entry points come from libGLESv3.so.
   This file exists only so CMakeLists.txt can treat ${CRAYON_GLAD_DIR}/src/glad.c
   uniformly on every platform. */
typedef int crayon_glad_gles_placeholder;
