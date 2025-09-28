#pragma once

/**
 * Compatibility wrapper for sdl_opengl_multi_scaler
 * Provides backward compatibility for existing tests while using the new modular implementation
 */

#include <scaler/gpu/sdl/sdl_texture_adapter.hh>
#include <scaler/gpu/opengl_texture_scaler.hh>
#include <scaler/algorithm.hh>
#include <GL/glew.h>
#include <SDL.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>
#include <cstring>

namespace scaler::gpu {

    /**
     * Compatibility wrapper that mimics the old sdl_opengl_multi_scaler interface
     * but uses the new modular implementation underneath
     */
    class sdl_opengl_multi_scaler {
    public:
        // Old algorithm enum for compatibility
        enum algorithm {
            NEAREST = 0,
            BILINEAR,
            EPX,
            EAGLE,
            Eagle = EAGLE,          // Alias
            SCALE2X,
            Scale2x = SCALE2X,      // Alias
            SCALE3X,
            Scale3x = SCALE3X,      // Alias
            SCALE4X,
            Scale4x = SCALE4X,      // Alias
            SCALE2X_SFX,
            Scale2xSFX = SCALE2X_SFX,  // Alias
            SCALE3X_SFX,
            Scale3xSFX = SCALE3X_SFX,  // Alias
            TWOXSAI,
            TwoXSaI = TWOXSAI,         // Alias
            SUPER_EAGLE,
            HQ2X,
            HQ3X,
            HQ4X,
            XBRZ2X,
            XBRZ3X,
            XBRZ4X,
            OMNISCALE,
            OmniScale = OMNISCALE,  // Alias
            AASCALE2X,
            AAScale2x = AASCALE2X,  // Alias
            AASCALE4X,
            AAScale4x = AASCALE4X,  // Alias
            AdvMAME2x = SCALE2X,    // AdvMAME is Scale2x
            AdvMAME3x = SCALE3X     // AdvMAME is Scale3x
        };

    private:
        SDL_Window* window_ = nullptr;
        SDL_Renderer* renderer_ = nullptr;
        bool owns_renderer_ = false;
        std::unique_ptr<sdl_texture_adapter> adapter_;
        std::unique_ptr<opengl_texture_scaler> gl_scaler_;  // For pure OpenGL path
        bool use_pure_opengl_ = false;

        // Convert old enum to new enum
        static scaler::algorithm convert_algorithm(algorithm old_algo) {
            switch (old_algo) {
                case NEAREST: return scaler::algorithm::Nearest;
                case BILINEAR: return scaler::algorithm::Bilinear;
                case EPX: return scaler::algorithm::EPX;
                case EAGLE: return scaler::algorithm::Eagle;
                case SCALE2X:
                case SCALE3X:
                case SCALE4X: return scaler::algorithm::Scale;
                case SCALE2X_SFX:
                case SCALE3X_SFX: return scaler::algorithm::ScaleSFX;
                case TWOXSAI: return scaler::algorithm::Super2xSaI;
                case HQ2X:
                case HQ3X:
                case HQ4X: return scaler::algorithm::HQ;
                case OMNISCALE: return scaler::algorithm::OmniScale;
                case AASCALE2X:
                case AASCALE4X: return scaler::algorithm::AAScale;
                default: return scaler::algorithm::EPX;
            }
        }

        // Get scale factor from algorithm
        static float get_scale_factor(algorithm old_algo) {
            switch (old_algo) {
                case SCALE3X:
                case SCALE3X_SFX:
                case HQ3X:
                case XBRZ3X:
                    return 3.0f;
                case SCALE4X:
                case HQ4X:
                case XBRZ4X:
                case AASCALE4X:
                    return 4.0f;
                default:
                    return 2.0f;
            }
        }

    public:
        sdl_opengl_multi_scaler() = default;

        ~sdl_opengl_multi_scaler() {
            cleanup();
        }

        /**
         * Initialize with an existing window
         */
        bool initialize(SDL_Window* window) {
            if (!window) return false;

            window_ = window;

            // Check if window already has an OpenGL context
            SDL_GLContext existing_context = SDL_GL_GetCurrentContext();

            // If there's already a GL context, we should NOT create an SDL_Renderer
            // as it will conflict. Instead, work directly with the existing context.
            if (existing_context) {
                // Window already has GL context, don't use SDL_Renderer
                renderer_ = nullptr;
                use_pure_opengl_ = true;

                // Initialize GLEW if not already done
                static bool glew_initialized = false;
                if (!glew_initialized) {
                    GLenum err = glewInit();
                    if (err != GLEW_OK) {
                        return false;
                    }
                    glew_initialized = true;
                }

                // Create pure OpenGL scaler
                try {
                    gl_scaler_ = std::make_unique<opengl_texture_scaler>();
                    return true;
                } catch (const std::exception&) {
                    return false;
                }
            }

            // No existing GL context, try to create SDL_Renderer
            renderer_ = SDL_GetRenderer(window);
            if (!renderer_) {
                // Make sure we request OpenGL renderer explicitly
                SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

                // Create renderer with OpenGL backend
                renderer_ = SDL_CreateRenderer(
                    window, -1,
                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE
                );

                if (!renderer_) {
                    return false;
                }
                owns_renderer_ = true;
            }

            // Check if the renderer is actually using OpenGL
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(renderer_, &info) == 0) {
                std::string name(info.name);
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.find("opengl") == std::string::npos &&
                    name.find("opengles") == std::string::npos) {
                    // Not an OpenGL renderer, can't use GPU acceleration
                    if (owns_renderer_) {
                        SDL_DestroyRenderer(renderer_);
                        renderer_ = nullptr;
                        owns_renderer_ = false;
                    }
                    return false;
                }
            }

            try {
                adapter_ = std::make_unique<sdl_texture_adapter>(renderer_);
                return true;
            } catch (const std::exception&) {
                if (owns_renderer_) {
                    SDL_DestroyRenderer(renderer_);
                    renderer_ = nullptr;
                    owns_renderer_ = false;
                }
                return false;
            }
        }

        /**
         * Scale an SDL surface (old interface)
         * Returns a new surface that must be freed by the caller
         */
        SDL_Surface* scale_surface(SDL_Surface* input, float scale_factor, algorithm algo) {
            if (!input) {
                return nullptr;
            }

            try {
                // Convert algorithm and get actual scale factor
                scaler::algorithm new_algo = convert_algorithm(algo);
                float actual_scale = (scale_factor > 0) ? scale_factor : get_scale_factor(algo);

                if (use_pure_opengl_ && gl_scaler_) {
                    // Pure OpenGL path - convert surface to OpenGL texture directly

                    // Create OpenGL texture from surface
                    GLuint input_texture;
                    glGenTextures(1, &input_texture);
                    glBindTexture(GL_TEXTURE_2D, input_texture);

                    // Convert surface to RGBA if needed and upload
                    SDL_Surface* converted = SDL_ConvertSurfaceFormat(input, SDL_PIXELFORMAT_RGBA8888, 0);
                    if (!converted) {
                        glDeleteTextures(1, &input_texture);
                        return nullptr;
                    }

                    // Upload surface data to texture
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                                converted->w, converted->h, 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

                    SDL_FreeSurface(converted);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                    // Calculate output dimensions
                    auto dims = opengl_texture_scaler::get_output_size(
                        input->w, input->h, new_algo, actual_scale
                    );

                    // Create output texture
                    GLuint output_texture = opengl_texture_scaler::create_output_texture(
                        dims.width, dims.height
                    );

                    // Perform scaling
                    gl_scaler_->scale_texture_to_texture(
                        input_texture, input->w, input->h,
                        output_texture, dims.width, dims.height,
                        new_algo
                    );

                    // Read back to surface
                    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(
                        0, dims.width, dims.height, 32, SDL_PIXELFORMAT_RGBA8888
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
                    // Read directly into a temporary buffer first
                    std::vector<Uint8> temp_pixels(dims.width * dims.height * 4);
                    glReadPixels(0, 0, dims.width, dims.height,
                                GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels.data());

                    // Flip vertically while copying to result
                    Uint8* dst = static_cast<Uint8*>(result->pixels);
                    for (int y = 0; y < dims.height; ++y) {
                        const Uint8* src = temp_pixels.data() + (dims.height - 1 - y) * dims.width * 4;
                        memcpy(dst + y * result->pitch, src, dims.width * 4);
                    }

                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &fbo);
                    glDeleteTextures(1, &output_texture);
                    glDeleteTextures(1, &input_texture);

                    return result;

                } else if (adapter_) {
                    // SDL Renderer path

                    // Create texture from surface
                    SDL_Texture* input_texture = SDL_CreateTextureFromSurface(renderer_, input);
                    if (!input_texture) {
                        return nullptr;
                    }

                    // Calculate output dimensions
                    auto dims = sdl_texture_adapter::get_output_size(
                        input_texture, new_algo, actual_scale
                    );

                    // Create output texture
                    SDL_Texture* output_texture = sdl_texture_adapter::create_output_texture(
                        renderer_, dims.width, dims.height
                    );

                    // Perform scaling
                    bool success = adapter_->scale_texture(
                        input_texture, output_texture, new_algo
                    );

                    SDL_DestroyTexture(input_texture);

                    if (!success) {
                        SDL_DestroyTexture(output_texture);
                        return nullptr;
                    }

                    // Read back to surface
                    SDL_Surface* result = SDL_CreateRGBSurfaceWithFormat(
                        0, dims.width, dims.height, 32, SDL_PIXELFORMAT_RGBA8888
                    );

                    if (!result) {
                        SDL_DestroyTexture(output_texture);
                        return nullptr;
                    }

                    // Set render target to output texture and read pixels
                    SDL_SetRenderTarget(renderer_, output_texture);
                    SDL_RenderReadPixels(renderer_, nullptr,
                                        SDL_PIXELFORMAT_RGBA8888,
                                        result->pixels, result->pitch);
                    SDL_SetRenderTarget(renderer_, nullptr);

                    SDL_DestroyTexture(output_texture);
                    return result;
                }

                return nullptr;

            } catch (const std::exception&) {
                return nullptr;
            }
        }

        /**
         * Overload for backward compatibility - ignore explicit scale factor
         */
        SDL_Surface* scale_surface(SDL_Surface* input, algorithm algo) {
            return scale_surface(input, 0.0f, algo);
        }

        /**
         * Clean up resources
         */
        void cleanup() {
            adapter_.reset();
            gl_scaler_.reset();
            if (owns_renderer_ && renderer_) {
                SDL_DestroyRenderer(renderer_);
                renderer_ = nullptr;
                owns_renderer_ = false;
            }
            window_ = nullptr;
            use_pure_opengl_ = false;
        }
    };

} // namespace scaler::gpu