#pragma once

#include <scaler/algorithm.hh>
#include <scaler/gpu/opengl_utils.hh>
#include <scaler/gpu/shader_cache.hh>
#include <scaler/gpu/algorithm_traits_impl.hh>
#include <GL/glew.h>
#include <memory>
#include <vector>
#include <stdexcept>

namespace scaler::gpu {

    /**
     * Pure OpenGL texture scaler - no SDL dependencies
     * Designed for game engines with preallocated textures
     */
    class opengl_texture_scaler {
    private:
        shader_cache cache_;
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        bool initialized_ = false;

        // Vertex data for full-screen quad
        static constexpr float quad_vertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 0.0f,  // top-left
             1.0f,  1.0f,  1.0f, 0.0f,  // top-right
            -1.0f, -1.0f,  0.0f, 1.0f,  // bottom-left
             1.0f, -1.0f,  1.0f, 1.0f   // bottom-right
        };

        void ensure_initialized() {
            if (initialized_) return;

            // Create and bind VAO
            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);

            // Create and bind VBO
            glGenBuffers(1, &vbo_);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

            // Position attribute
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // Texture coordinate attribute
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);

            // Unbind
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            initialized_ = true;
        }

        const shader_program& get_or_compile_shader(algorithm algo, float scale_factor) {
            // Get the appropriate shader source based on algorithm and scale
            const char* fragment_source = get_shader_for_algorithm_and_scale(algo, scale_factor);
            if (!fragment_source) {
                throw std::runtime_error("No shader available for algorithm " +
                                       std::to_string(static_cast<int>(algo)) +
                                       " at scale " + std::to_string(scale_factor));
            }

            // Use the common vertex shader
            const char* vertex_source = shader_source::vertex_shader_source;

            // Create a unique key for this algorithm/scale combination
            std::string shader_key = "scaler_" + std::to_string(static_cast<int>(algo)) +
                                   "_" + std::to_string(scale_factor);

            return cache_.get_or_compile(shader_key, vertex_source, fragment_source);
        }

    public:
        opengl_texture_scaler() = default;

        ~opengl_texture_scaler() {
            if (vao_) glDeleteVertexArrays(1, &vao_);
            if (vbo_) glDeleteBuffers(1, &vbo_);
        }

        // Non-copyable but moveable
        opengl_texture_scaler(const opengl_texture_scaler&) = delete;
        opengl_texture_scaler& operator=(const opengl_texture_scaler&) = delete;
        opengl_texture_scaler(opengl_texture_scaler&& other) noexcept
            : cache_(std::move(other.cache_))
            , vao_(other.vao_)
            , vbo_(other.vbo_)
            , initialized_(other.initialized_) {
            other.vao_ = 0;
            other.vbo_ = 0;
            other.initialized_ = false;
        }
        opengl_texture_scaler& operator=(opengl_texture_scaler&& other) noexcept {
            if (this != &other) {
                if (vao_) glDeleteVertexArrays(1, &vao_);
                if (vbo_) glDeleteBuffers(1, &vbo_);

                cache_ = std::move(other.cache_);
                vao_ = other.vao_;
                vbo_ = other.vbo_;
                initialized_ = other.initialized_;

                other.vao_ = 0;
                other.vbo_ = 0;
                other.initialized_ = false;
            }
            return *this;
        }

        /**
         * Calculate output dimensions for preallocating textures
         */
        static output_dimensions get_output_size(
            GLsizei input_width,
            GLsizei input_height,
            algorithm algo,
            float scale_factor) {
            return calculate_output_size(input_width, input_height, algo, scale_factor);
        }

        /**
         * Scale texture to preallocated texture
         * @param input_texture Source texture to scale
         * @param input_width Width of input texture
         * @param input_height Height of input texture
         * @param output_texture Target texture (must be preallocated as render target)
         * @param output_width Width of output texture
         * @param output_height Height of output texture
         * @param algo Scaling algorithm to use
         */
        void scale_texture_to_texture(
            GLuint input_texture,
            GLsizei input_width,
            GLsizei input_height,
            GLuint output_texture,
            GLsizei output_width,
            GLsizei output_height,
            algorithm algo) {

            ensure_initialized();

            // Calculate actual scale factor
            float scale_factor = static_cast<float>(output_width) / static_cast<float>(input_width);

            // Verify the algorithm supports this scale on GPU
            if (!gpu_algorithm_traits::is_scale_supported_on_gpu(algo, scale_factor)) {
                throw std::runtime_error("Algorithm does not support scale factor " +
                                       std::to_string(scale_factor) + " on GPU");
            }

            // Get or compile the appropriate shader
            const auto& shader = get_or_compile_shader(algo, scale_factor);

            // Create framebuffer for output
            GLuint fbo;
            glGenFramebuffers(1, &fbo);
            detail::scoped_framebuffer_bind fb_bind(fbo);

            // Attach output texture to framebuffer
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, output_texture, 0);

            // Check framebuffer completeness
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                glDeleteFramebuffers(1, &fbo);
                throw std::runtime_error("Framebuffer incomplete");
            }

            // Save and set viewport
            GLint old_viewport[4];
            glGetIntegerv(GL_VIEWPORT, old_viewport);
            glViewport(0, 0, output_width, output_height);

            // Clear the output texture
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Use shader program
            shader.use();

            // Set uniforms
            glUniform1i(shader.u_texture, 0);
            glUniform2f(shader.u_texture_size,
                       static_cast<float>(input_width),
                       static_cast<float>(input_height));
            glUniform2f(shader.u_output_size,
                       static_cast<float>(output_width),
                       static_cast<float>(output_height));

            // Bind input texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, input_texture);

            // Set texture parameters for pixel art
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Render the quad
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            // Cleanup
            glUseProgram(0);
            glBindTexture(GL_TEXTURE_2D, 0);

            // Restore viewport
            glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);

            // Delete framebuffer
            glDeleteFramebuffers(1, &fbo);

            detail::check_gl_error("After scale_texture_to_texture");
        }

        /**
         * Scale texture to framebuffer (for render-to-texture)
         * @param input_texture Source texture to scale
         * @param input_width Width of input texture
         * @param input_height Height of input texture
         * @param target_fbo Target framebuffer (0 for default framebuffer)
         * @param fbo_width Width of framebuffer viewport
         * @param fbo_height Height of framebuffer viewport
         * @param algo Scaling algorithm to use
         */
        void scale_texture_to_framebuffer(
            GLuint input_texture,
            GLsizei input_width,
            GLsizei input_height,
            GLuint target_fbo,
            GLsizei fbo_width,
            GLsizei fbo_height,
            algorithm algo) {

            ensure_initialized();

            // Calculate scale factor
            float scale_factor = static_cast<float>(fbo_width) / static_cast<float>(input_width);

            // Verify the algorithm supports this scale on GPU
            if (!gpu_algorithm_traits::is_scale_supported_on_gpu(algo, scale_factor)) {
                throw std::runtime_error("Algorithm does not support scale factor " +
                                       std::to_string(scale_factor) + " on GPU");
            }

            // Get or compile the appropriate shader
            const auto& shader = get_or_compile_shader(algo, scale_factor);

            // Bind target framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);

            // Save and set viewport
            GLint old_viewport[4];
            glGetIntegerv(GL_VIEWPORT, old_viewport);
            glViewport(0, 0, fbo_width, fbo_height);

            // Use shader program
            shader.use();

            // Set uniforms
            glUniform1i(shader.u_texture, 0);
            glUniform2f(shader.u_texture_size,
                       static_cast<float>(input_width),
                       static_cast<float>(input_height));
            glUniform2f(shader.u_output_size,
                       static_cast<float>(fbo_width),
                       static_cast<float>(fbo_height));

            // Bind input texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, input_texture);

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Render the quad
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            // Cleanup
            glUseProgram(0);
            glBindTexture(GL_TEXTURE_2D, 0);

            // Restore viewport
            glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);

            detail::check_gl_error("After scale_texture_to_framebuffer");
        }

        /**
         * Helper to create properly sized output texture
         * @param width Width of texture
         * @param height Height of texture
         * @param format Pixel format (default GL_RGBA)
         * @return OpenGL texture ID
         */
        static GLuint create_output_texture(
            GLsizei width,
            GLsizei height,
            GLenum format = GL_RGBA) {

            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            // Allocate texture storage
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                        format, GL_UNSIGNED_BYTE, nullptr);

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glBindTexture(GL_TEXTURE_2D, 0);

            detail::check_gl_error("After create_output_texture");

            return texture;
        }

        /**
         * Batch processing for multiple textures
         */
        struct texture_info {
            GLuint texture;
            GLsizei width;
            GLsizei height;
        };

        /**
         * Process multiple textures efficiently
         * @param inputs Vector of input texture information
         * @param algo Scaling algorithm to use
         * @param scale_factor Scale factor to apply
         * @return Vector of output texture IDs (caller owns these textures)
         */
        std::vector<GLuint> scale_batch(
            const std::vector<texture_info>& inputs,
            algorithm algo,
            float scale_factor) {

            std::vector<GLuint> outputs;
            outputs.reserve(inputs.size());

            for (const auto& input : inputs) {
                // Calculate output dimensions
                auto dims = get_output_size(input.width, input.height, algo, scale_factor);

                // Create output texture
                GLuint output = create_output_texture(dims.width, dims.height);

                // Scale the texture
                scale_texture_to_texture(
                    input.texture, input.width, input.height,
                    output, dims.width, dims.height,
                    algo
                );

                outputs.push_back(output);
            }

            return outputs;
        }

        /**
         * Precompile shaders for faster first use
         * @param algo Algorithm to precompile shaders for
         * @param scale_factor Scale factor to precompile for
         */
        void precompile_shader(algorithm algo, float scale_factor) {
            get_or_compile_shader(algo, scale_factor);
        }

        /**
         * Precompile all GPU-accelerated shaders
         */
        void precompile_all_shaders() {
            auto algorithms = gpu_algorithm_traits::get_gpu_algorithms();

            for (algorithm algo : algorithms) {
                auto scales = gpu_algorithm_traits::get_gpu_supported_scales(algo);

                if (scales.empty() && gpu_algorithm_traits::supports_arbitrary_scale(algo)) {
                    // For arbitrary scale algorithms, precompile common scales
                    precompile_shader(algo, 2.0f);
                    precompile_shader(algo, 3.0f);
                    precompile_shader(algo, 4.0f);
                } else {
                    // For fixed scale algorithms, precompile all supported scales
                    for (float scale : scales) {
                        precompile_shader(algo, scale);
                    }
                }
            }
        }

        /**
         * Check if an algorithm is available for GPU acceleration
         */
        static bool is_algorithm_available(algorithm algo) {
            return gpu_algorithm_traits::is_gpu_accelerated(algo);
        }

        /**
         * Check if a specific scale is supported for an algorithm
         */
        static bool is_scale_supported(algorithm algo, float scale) {
            return gpu_algorithm_traits::is_scale_supported_on_gpu(algo, scale);
        }
    };

} // namespace scaler::gpu