#pragma once

#include <scaler/algorithm.hh>
#include <scaler/gpu/opengl_texture_scaler.hh>  // This includes opengl_utils.hh with platform detection
#include <scaler/sdl/sdl_compat.hh>
#include <SDL.h>
#include <stdexcept>
#include <memory>

namespace scaler::gpu {

    /**
     * SDL texture adapter for GPU scaling
     * Wraps the pure OpenGL texture scaler for use with SDL
     */
    class sdl_texture_adapter {
    private:
        opengl_texture_scaler gl_scaler_;
        SDL_Renderer* renderer_ = nullptr;
        bool renderer_is_opengl_ = false;

        /**
         * Get the OpenGL texture ID from an SDL texture
         * Only works if the renderer is using OpenGL backend
         */
        GLuint get_gl_texture_id(SDL_Texture* texture) {
            GLuint texture_id = 0;

            // This only works with OpenGL renderer backend
            if (!renderer_is_opengl_) {
                throw std::runtime_error("SDL renderer is not using OpenGL backend");
            }

            // Bind the texture to get its GL ID
            if (SDL_GL_BindTexture(texture, nullptr, nullptr) != 0) {
                throw std::runtime_error(std::string("Failed to bind SDL texture: ") + SDL_GetError());
            }

            // Get the currently bound texture
            glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&texture_id));

            // Unbind
            SDL_GL_UnbindTexture(texture);

            return texture_id;
        }

        /**
         * Check if renderer is using OpenGL backend
         */
        bool is_opengl_renderer(SDL_Renderer* renderer) {
            SDL_RendererInfo info;
            if (SDL_GetRendererInfo(renderer, &info) != 0) {
                return false;
            }

            // Check if renderer name contains "opengl"
            std::string name(info.name);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            return name.find("opengl") != std::string::npos;
        }

    public:
        /**
         * Create an SDL texture adapter
         * @param renderer SDL renderer (must be using OpenGL backend)
         */
        explicit sdl_texture_adapter(SDL_Renderer* renderer)
            : renderer_(renderer) {

            if (!renderer) {
                throw std::invalid_argument("Renderer cannot be null");
            }

            // Check if renderer is using OpenGL
            renderer_is_opengl_ = is_opengl_renderer(renderer);
            if (!renderer_is_opengl_) {
                throw std::runtime_error("SDL renderer must be using OpenGL backend for GPU scaling");
            }

            // Initialize GLEW if not already done (not needed on macOS)
            #ifndef SCALER_PLATFORM_MACOS
            static bool glew_initialized = false;
            if (!glew_initialized) {
                GLenum err = glewInit();
                if (err != GLEW_OK) {
                    throw std::runtime_error(std::string("Failed to initialize GLEW: ") +
                                           reinterpret_cast<const char*>(glewGetErrorString(err)));
                }
                glew_initialized = true;
            }
            #endif
        }

        /**
         * Calculate output dimensions for a given input texture
         * @param input Input SDL texture
         * @param algo Scaling algorithm
         * @param scale_factor Scale factor to apply
         * @return Output dimensions
         */
        static output_dimensions get_output_size(
            SDL_Texture* input,
            algorithm algo,
            float scale_factor) {

            int w, h;
            if (SDL_QueryTexture(input, nullptr, nullptr, &w, &h) != 0) {
                throw std::runtime_error(std::string("Failed to query texture: ") + SDL_GetError());
            }

            return calculate_output_size(w, h, algo, scale_factor);
        }

        /**
         * Scale SDL texture to preallocated SDL texture
         * @param input Source SDL texture
         * @param output Target SDL texture (must be preallocated as render target)
         * @param algo Scaling algorithm to use
         * @return true on success, false on failure
         */
        bool scale_texture(
            SDL_Texture* input,
            SDL_Texture* output,
            algorithm algo) {

            try {
                // Validate input
                if (!input || !output) {
                    return false;
                }

                // Check that output is a render target
                Uint32 format;
                int access, w_out, h_out;
                if (SDL_QueryTexture(output, &format, &access, &w_out, &h_out) != 0) {
                    return false;
                }

                if (access != SDL_TEXTUREACCESS_TARGET) {
                    SDL_SetError("Output texture must be created with SDL_TEXTUREACCESS_TARGET");
                    return false;
                }

                // Get input dimensions
                int w_in, h_in;
                if (SDL_QueryTexture(input, nullptr, nullptr, &w_in, &h_in) != 0) {
                    return false;
                }

                // Get OpenGL texture IDs
                GLuint input_id = get_gl_texture_id(input);
                GLuint output_id = get_gl_texture_id(output);

                // Perform the scaling
                gl_scaler_.scale_texture_to_texture(
                    input_id, w_in, h_in,
                    output_id, w_out, h_out,
                    algo
                );

                return true;

            } catch (const std::exception& e) {
                SDL_SetError("%s", e.what());
                return false;
            }
        }

        /**
         * Scale a region of an SDL texture to another region
         * @param input Source SDL texture
         * @param src_rect Source rectangle (nullptr for entire texture)
         * @param output Target SDL texture (must be render target)
         * @param dst_rect Destination rectangle (nullptr for entire texture)
         * @param algo Scaling algorithm to use
         * @return true on success, false on failure
         */
        bool scale_texture_region(
            SDL_Texture* input,
            const SDL_Rect* src_rect,
            SDL_Texture* output,
            const SDL_Rect* dst_rect,
            algorithm algo) {

            try {
                // For region scaling, we need to adjust texture coordinates
                // This would require modifying the shader or using a viewport
                // For now, we'll scale the entire texture if no rect specified

                if (!src_rect && !dst_rect) {
                    return scale_texture(input, output, algo);
                }

                // Get texture dimensions
                int w_in, h_in, w_out, h_out;
                if (SDL_QueryTexture(input, nullptr, nullptr, &w_in, &h_in) != 0) {
                    return false;
                }
                if (SDL_QueryTexture(output, nullptr, nullptr, &w_out, &h_out) != 0) {
                    return false;
                }

                // Calculate actual regions
                SDL_Rect src = src_rect ? *src_rect : SDL_Rect{0, 0, w_in, h_in};
                SDL_Rect dst = dst_rect ? *dst_rect : SDL_Rect{0, 0, w_out, h_out};

                // Get OpenGL texture IDs
                GLuint input_id = get_gl_texture_id(input);
                GLuint output_id = get_gl_texture_id(output);

                // Save current viewport
                GLint old_viewport[4];
                glGetIntegerv(GL_VIEWPORT, old_viewport);

                // Create framebuffer for output
                GLuint fbo;
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, output_id, 0);

                // Set viewport to destination rectangle
                glViewport(dst.x, dst.y, dst.w, dst.h);

                // Scale the region
                // Note: This currently scales the entire input to the destination region
                // Full region support would require texture coordinate adjustment
                gl_scaler_.scale_texture_to_framebuffer(
                    input_id, src.w, src.h,
                    fbo, dst.w, dst.h,
                    algo
                );

                // Cleanup
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &fbo);
                glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);

                return true;

            } catch (const std::exception& e) {
                SDL_SetError("%s", e.what());
                return false;
            }
        }

        /**
         * Render scaled texture directly to current render target
         * @param input Source SDL texture
         * @param algo Scaling algorithm to use
         * @param scale_factor Scale factor to apply
         */
        void render_scaled_to_target(
            SDL_Texture* input,
            algorithm algo,
            float scale_factor) {

            // Get input dimensions
            int w_in, h_in;
            if (SDL_QueryTexture(input, nullptr, nullptr, &w_in, &h_in) != 0) {
                throw std::runtime_error(std::string("Failed to query texture: ") + SDL_GetError());
            }

            // Calculate output dimensions
            auto dims = opengl_texture_scaler::get_output_size(w_in, h_in, algo, scale_factor);

            // Get the current render target
            SDL_Texture* current_target = SDL_GetRenderTarget(renderer_);

            GLuint target_fbo = 0;
            int target_width = dims.width;
            int target_height = dims.height;

            if (current_target) {
                // Rendering to a texture
                GLuint target_id = get_gl_texture_id(current_target);

                // Create FBO for the target texture
                glGenFramebuffers(1, &target_fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, target_id, 0);

                // Get actual target dimensions
                SDL_QueryTexture(current_target, nullptr, nullptr, &target_width, &target_height);
            } else {
                // Rendering to screen (default framebuffer)
                SDL_GetRendererOutputSize(renderer_, &target_width, &target_height);
            }

            // Get input GL texture ID
            GLuint input_id = get_gl_texture_id(input);

            // Perform the scaling
            gl_scaler_.scale_texture_to_framebuffer(
                input_id, w_in, h_in,
                target_fbo, target_width, target_height,
                algo
            );

            // Cleanup if we created an FBO
            if (target_fbo != 0) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &target_fbo);
            }
        }

        /**
         * Create an output texture suitable for scaling
         * @param renderer SDL renderer
         * @param width Width of texture
         * @param height Height of texture
         * @param format Pixel format (0 for default RGBA8888)
         * @return SDL texture configured as render target
         */
        static SDL_Texture* create_output_texture(
            SDL_Renderer* renderer,
            int width,
            int height,
            Uint32 format = SDL_PIXELFORMAT_RGBA8888) {

            SDL_Texture* texture = SDL_CreateTexture(
                renderer,
                format,
                SDL_TEXTUREACCESS_TARGET,
                width,
                height
            );

            if (!texture) {
                throw std::runtime_error(std::string("Failed to create texture: ") + SDL_GetError());
            }

            return texture;
        }

        /**
         * Batch processing for SDL textures
         */
        struct sdl_texture_info {
            SDL_Texture* texture;
            // Dimensions are queried automatically
        };

        /**
         * Process multiple SDL textures efficiently
         * @param inputs Vector of input SDL textures
         * @param algo Scaling algorithm to use
         * @param scale_factor Scale factor to apply
         * @return Vector of output SDL textures (caller owns these)
         */
        std::vector<SDL_Texture*> scale_batch(
            const std::vector<SDL_Texture*>& inputs,
            algorithm algo,
            float scale_factor) {

            std::vector<SDL_Texture*> outputs;
            outputs.reserve(inputs.size());

            for (SDL_Texture* input : inputs) {
                // Get input dimensions
                int w, h;
                if (SDL_QueryTexture(input, nullptr, nullptr, &w, &h) != 0) {
                    // Clean up already created textures before throwing
                    for (SDL_Texture* tex : outputs) {
                        SDL_DestroyTexture(tex);
                    }
                    throw std::runtime_error(std::string("Failed to query texture: ") + SDL_GetError());
                }

                // Calculate output dimensions
                auto dims = get_output_size(input, algo, scale_factor);

                // Create output texture
                SDL_Texture* output = create_output_texture(
                    renderer_, dims.width, dims.height
                );

                // Scale the texture
                if (!scale_texture(input, output, algo)) {
                    // Clean up on failure
                    SDL_DestroyTexture(output);
                    for (SDL_Texture* tex : outputs) {
                        SDL_DestroyTexture(tex);
                    }
                    throw std::runtime_error("Failed to scale texture in batch");
                }

                outputs.push_back(output);
            }

            return outputs;
        }

        /**
         * Check if an algorithm is available for GPU acceleration
         */
        static bool is_algorithm_available(algorithm algo) {
            return opengl_texture_scaler::is_algorithm_available(algo);
        }

        /**
         * Check if a specific scale is supported for an algorithm
         */
        static bool is_scale_supported(algorithm algo, float scale) {
            return opengl_texture_scaler::is_scale_supported(algo, scale);
        }

        /**
         * Precompile shaders for faster first use
         */
        void precompile_shaders() {
            gl_scaler_.precompile_all_shaders();
        }
    };

} // namespace scaler::gpu