#pragma once

#include <SDL.h>
#include <iostream>

#ifndef SCALER_PLATFORM_MACOS
#include <GL/glew.h>
#endif

namespace scaler::test {

/**
 * Shared GPU context manager for all GPU tests
 *
 * This ensures a single OpenGL context is used across all test files,
 * preventing issues with cached shader programs becoming invalid.
 */
class gpu_context {
private:
    static SDL_Window* window_;
    static SDL_GLContext context_;
    static bool initialized_;
    static int reference_count_;

public:
    /**
     * Initialize or get existing GPU context
     * Returns true if context is available, false otherwise
     */
    static bool ensure_context() {
        if (!initialized_) {
            // Make sure SDL video is initialized
            if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
                if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
                    std::cerr << "Failed to initialize SDL video: " << SDL_GetError() << std::endl;
                    return false;
                }
            }

            // Create hidden window with OpenGL context
            window_ = SDL_CreateWindow(
                "GPU Test Context",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                640, 480,
                SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
            );

            if (!window_) {
                std::cerr << "Failed to create OpenGL window: " << SDL_GetError() << std::endl;
                return false;
            }

            context_ = SDL_GL_CreateContext(window_);
            if (!context_) {
                std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                return false;
            }

            #ifndef SCALER_PLATFORM_MACOS
            GLenum glew_error = glewInit();
            if (glew_error != GLEW_OK) {
                std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glew_error) << std::endl;
                SDL_GL_DeleteContext(context_);
                SDL_DestroyWindow(window_);
                window_ = nullptr;
                context_ = nullptr;
                return false;
            }
            #endif

            initialized_ = true;
        }

        // Make sure context is current
        if (SDL_GL_MakeCurrent(window_, context_) != 0) {
            std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
            return false;
        }

        reference_count_++;
        return true;
    }

    /**
     * Release reference to GPU context
     * Context is kept alive as long as any test needs it
     */
    static void release() {
        if (reference_count_ > 0) {
            reference_count_--;
        }

        // We intentionally don't destroy the context here
        // It will be cleaned up at program exit
        // This prevents issues with doctest running tests multiple times
    }

    /**
     * Force cleanup of GPU context
     * Only used for explicit cleanup if needed
     */
    static void cleanup() {
        if (initialized_) {
            if (context_) {
                SDL_GL_DeleteContext(context_);
                context_ = nullptr;
            }
            if (window_) {
                SDL_DestroyWindow(window_);
                window_ = nullptr;
            }
            initialized_ = false;
            reference_count_ = 0;
        }
    }

    // RAII wrapper for automatic reference management
    class scoped_context {
    public:
        scoped_context() : valid_(ensure_context()) {}
        ~scoped_context() { if (valid_) release(); }

        bool is_valid() const { return valid_; }
        operator bool() const { return valid_; }

    private:
        bool valid_;
    };
};

// Static member definitions
inline SDL_Window* gpu_context::window_ = nullptr;
inline SDL_GLContext gpu_context::context_ = nullptr;
inline bool gpu_context::initialized_ = false;
inline int gpu_context::reference_count_ = 0;

} // namespace scaler::test