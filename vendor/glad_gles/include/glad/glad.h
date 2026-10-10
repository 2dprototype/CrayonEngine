// OpenGL ES 3.0 "loader" for Android.
//
// Desktop Windows needs glad because opengl32.dll only exports GL 1.1; every newer
// function must be fetched at runtime. On Android every OpenGL ES 3.0 function is
// exported directly by libGLESv3.so, so there is nothing to load: this header just
// pulls in the NDK's ES 3.0 declarations and keeps the same `gladLoad...` entry
// point the engine calls, so window.cpp needs no special-casing beyond the name.
//
// The engine only uses GL calls that exist in ES 3.0 core (audited).
#ifndef CRAYON_GLAD_GLES_H
#define CRAYON_GLAD_GLES_H

#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char* name);

static inline int gladLoadGLES2Loader(GLADloadproc load) {
    (void)load;   // symbols are resolved by the dynamic linker (-lGLESv3)
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* CRAYON_GLAD_GLES_H */
