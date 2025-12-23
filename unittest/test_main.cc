// Main test runner for scaler unit tests
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <SDL.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    // Set SDL to use dummy audio driver for testing
    // This avoids needing actual audio hardware
    setenv("SDL_AUDIODRIVER", "dummy", 1);

    // Only set video driver if not already set (allows CI to override)
    // Use x11 video driver for GPU tests (needs OpenGL support)
    // The dummy driver doesn't support OpenGL
    if (getenv("SDL_VIDEODRIVER") == nullptr) {
        setenv("SDL_VIDEODRIVER", "x11", 1);
    }

    // Initialize SDL once for all tests
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Warning: SDL initialization failed: " << SDL_GetError() << std::endl;
        std::cerr << "GPU tests will be skipped." << std::endl;
    }

    // Set OpenGL attributes for GPU tests (only if SDL initialized successfully)
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

    // Run the tests
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = context.run();

    // Clean up SDL
    SDL_Quit();

    if(context.shouldExit()) {
        return res;
    }

    return res;
}