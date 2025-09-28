#pragma once

/**
 * GPU Test Helper
 * Provides convenient surface scaling for GPU tests using the new modular implementation
 */

#include <scaler/gpu/opengl_texture_scaler.hh>
#include <scaler/algorithm.hh>
#include <scaler/warning_macros.hh>
#include <SDL.h>
#include <memory>
#include <vector>
#include <cstring>

namespace scaler::gpu::test {

    /**
     * Helper class for GPU scaling tests
     * Handles the conversion between SDL surfaces and OpenGL textures
     */
    class gpu_test_helper {
    private:
        std::unique_ptr<opengl_texture_scaler> gl_scaler_;
        bool initialized_ = false;

    public:
        gpu_test_helper() = default;

        /**
         * Initialize the GPU scaler
         * Assumes OpenGL context is already current
         */
        bool initialize() {
            if (initialized_) return true;

            // Initialize GLEW if not already done (not needed on macOS)
            #ifndef SCALER_PLATFORM_MACOS
            static bool glew_initialized = false;
            if (!glew_initialized) {
                GLenum err = glewInit();
                if (err != GLEW_OK) {
                    return false;
                }
                glew_initialized = true;
            }
            #endif

            try {
                gl_scaler_ = std::make_unique<opengl_texture_scaler>();
                initialized_ = true;
                return true;
            } catch (const std::exception&) {
                return false;
            }
        }

        /**
         * Scale an SDL surface using GPU
         * Returns a new surface that must be freed by the caller
         */
        SDL_Surface* scale_surface(SDL_Surface* input, algorithm algo, float scale_factor = 0.0f) {
            if (!input || !initialized_ || !gl_scaler_) {
                return nullptr;
            }

            try {
                // Auto-detect scale factor from algorithm if not provided
                if (scale_factor <= 0.0f) {
                    scale_factor = get_default_scale_for_algorithm(algo);
                }

                // Convert surface to RGBA if needed
                SDL_Surface* converted = SDL_ConvertSurfaceFormat(input, SDL_PIXELFORMAT_RGBA8888, 0);
                if (!converted) {
                    return nullptr;
                }

                // Create OpenGL texture from surface
                GLuint input_texture;
                glGenTextures(1, &input_texture);
                glBindTexture(GL_TEXTURE_2D, input_texture);

                // Upload surface data to texture
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                            converted->w, converted->h, 0,
                            GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                SDL_FreeSurface(converted);

                // Calculate output dimensions
                auto dims = opengl_texture_scaler::get_output_size(
                    input->w, input->h, algo, scale_factor
                );

                // Create output texture
                GLuint output_texture = opengl_texture_scaler::create_output_texture(
                    SCALER_SIZE_TO_GLSIZEI(dims.width), SCALER_SIZE_TO_GLSIZEI(dims.height)
                );

                // Perform scaling
                gl_scaler_->scale_texture_to_texture(
                    input_texture, input->w, input->h,
                    output_texture, SCALER_SIZE_TO_GLSIZEI(dims.width), SCALER_SIZE_TO_GLSIZEI(dims.height),
                    algo
                );

                // Create result surface
                SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(
                    0, static_cast<int>(dims.width), static_cast<int>(dims.height), 32, SDL_PIXELFORMAT_RGBA8888
                );

                if (!result) {
                    glDeleteTextures(1, &output_texture);
                    glDeleteTextures(1, &input_texture);
                    return nullptr;
                }

                // Read pixels from output texture
                GLuint fbo;
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, output_texture, 0);

                // OpenGL reads pixels bottom-to-top, but SDL expects top-to-bottom
                std::vector<Uint8> temp_pixels(dims.width * dims.height * 4);
                glReadPixels(0, 0, SCALER_SIZE_TO_GLSIZEI(dims.width), SCALER_SIZE_TO_GLSIZEI(dims.height),
                            GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels.data());

                // Flip vertically while copying to result
                Uint8* dst = static_cast<Uint8*>(result->pixels);
                for (size_t y = 0; y < dims.height; ++y) {
                    const Uint8* src = temp_pixels.data() + (dims.height - 1 - y) * dims.width * 4;
                    memcpy(dst + y * static_cast<size_t>(result->pitch), src, dims.width * 4);
                }

                // Cleanup
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &fbo);
                glDeleteTextures(1, &output_texture);
                glDeleteTextures(1, &input_texture);

                return result;

            } catch (const std::exception&) {
                return nullptr;
            }
        }

        /**
         * Check if an algorithm is available for GPU acceleration
         */
        static bool is_algorithm_available(algorithm algo) {
            return opengl_texture_scaler::is_algorithm_available(algo);
        }

        /**
         * Get default scale factor for an algorithm
         */
        static float get_default_scale_for_algorithm(algorithm algo) {
            switch (algo) {
                case algorithm::Scale:
                case algorithm::ScaleSFX:
                case algorithm::HQ:
                    // These have 2x, 3x, 4x variants, default to 2x
                    return 2.0f;
                case algorithm::Nearest:
                case algorithm::Bilinear:
                case algorithm::OmniScale:
                    // These support arbitrary scaling, default to 2x
                    return 2.0f;
                default:
                    // Most algorithms are 2x only
                    return 2.0f;
            }
        }
    };

} // namespace scaler::gpu::test