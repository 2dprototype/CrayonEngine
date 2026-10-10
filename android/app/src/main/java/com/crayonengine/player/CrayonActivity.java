package com.crayonengine.player;

import org.libsdl.app.SDLActivity;

/**
 * SDL does all the work: it creates the surface, forwards touch/keys/gamepads and
 * calls SDL_main() (our main() in src/main.cpp) on its own thread.
 * We only say which native libraries to load: libSDL3.so, then libmain.so (= crayon).
 */
public class CrayonActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
