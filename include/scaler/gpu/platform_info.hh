#pragma once

/**
 * Platform-specific OpenGL configuration information
 * This header provides compile-time information about the OpenGL setup
 */

#include <scaler/gpu/opengl_utils.hh>
#include <string>

namespace scaler::gpu {

    /**
     * Get platform-specific OpenGL information
     */
    class platform_info {
    public:
        /**
         * Get the current platform name
         */
        static std::string get_platform() {
            #if defined(SCALER_PLATFORM_WINDOWS)
                return "Windows";
            #elif defined(SCALER_PLATFORM_MACOS)
                return "macOS";
            #elif defined(SCALER_PLATFORM_LINUX)
                return "Linux";
            #elif defined(SCALER_PLATFORM_UNIX)
                return "Unix";
            #else
                return "Unknown";
            #endif
        }

        /**
         * Check if GLEW is required on this platform
         */
        static bool requires_glew() {
            #ifdef SCALER_PLATFORM_MACOS
                return false;  // macOS doesn't need GLEW
            #else
                return true;   // Windows and Linux need GLEW
            #endif
        }

        /**
         * Get OpenGL header path used
         */
        static std::string get_gl_header_path() {
            #if defined(SCALER_PLATFORM_WINDOWS)
                return "GL/glew.h (Windows)";
            #elif defined(SCALER_PLATFORM_MACOS)
                return "OpenGL/gl3.h (macOS)";
            #else
                return "GL/glew.h (Linux/Unix)";
            #endif
        }

        /**
         * Check if platform supports OpenGL 3.3 core profile
         */
        static bool supports_gl33_core() {
            // All modern platforms support OpenGL 3.3 core
            return true;
        }

        /**
         * Get recommended OpenGL context flags for platform
         */
        static int get_recommended_gl_flags() {
            #ifdef SCALER_PLATFORM_MACOS
                // macOS requires forward-compatible context
                return SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
            #else
                // Other platforms don't require special flags
                return 0;
            #endif
        }
    };

} // namespace scaler::gpu