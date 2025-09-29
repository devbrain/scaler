/**
 * @file unified_gpu_scaler.hh
 * @brief GPU-accelerated unified interface for image scaling using OpenGL
 *
 * This header provides a specialized version of the unified_scaler template for
 * GPU textures. It maintains the same interface as the CPU version but operates
 * on OpenGL textures, providing hardware-accelerated scaling with shader-based
 * algorithms.
 *
 * @example Basic GPU scaling:
 * @code
 * // Create input texture wrapper
 * gpu::input_texture input(texture_id, width, height);
 *
 * // Scale 2x using GPU-accelerated EPX
 * auto scaled = GPUScaler::scale(input, algorithm::EPX, 2.0f);
 *
 * // Or scale into existing texture
 * gpu::output_texture output(output_tex_id, width * 2, height * 2);
 * GPUScaler::scale(input, output, algorithm::EPX);
 * @endcode
 *
 * @note Requires OpenGL context to be active when calling scale methods
 * @note Not all algorithms are available on GPU - use is_gpu_accelerated() to check
 * @see unified_scaler.hh for the CPU version
 * @see opengl_texture_scaler.hh for the underlying GPU implementation
 */
#pragma once

#include <scaler/unified_scaler.hh>
#include <scaler/gpu/texture_ref.hh>
#include <scaler/gpu/opengl_texture_scaler.hh>
#include <scaler/algorithm_capabilities.hh>
#include <memory>

namespace scaler {
    /**
     * @class unified_scaler<gpu::input_texture, gpu::output_texture>
     * @brief Template specialization for GPU-accelerated texture scaling
     *
     * This specialization provides hardware-accelerated image scaling using OpenGL
     * shaders. It maintains API compatibility with the CPU version while leveraging
     * GPU parallelism for improved performance on large images.
     *
     * Key differences from CPU version:
     * - Works with OpenGL texture IDs instead of pixel buffers
     * - Requires active OpenGL context
     * - Limited algorithm support (not all CPU algorithms have GPU versions)
     * - Better performance for large images
     * - Thread-local GPU scaler instance for efficiency
     *
     * @note The GPU scaler uses a thread-local singleton pattern to avoid
     *       recreating shader programs for each scaling operation.
     */
    template<>
    class unified_scaler <gpu::input_texture, gpu::output_texture> {
        public:
            /**
             * @struct dimensions
             * @brief Holds calculated output dimensions for a scaling operation
             *
             * Identical to the CPU version to maintain interface compatibility.
             */
            struct dimensions {
                size_t width; ///< Output width in pixels
                size_t height; ///< Output height in pixels
            };

            /**
             * @brief Calculate expected output dimensions for GPU scaling operation
             *
             * @param input Source texture to scale
             * @param algo Scaling algorithm (currently unused, for future algorithm-specific sizing)
             * @param scale_factor Scaling multiplier
             * @return dimensions struct with calculated width and height
             *
             * @note Identical behavior to CPU version for consistency
             */
            static dimensions calculate_output_dimensions(const gpu::input_texture& input,
                                                          [[maybe_unused]] algorithm algo,
                                                          float scale_factor) {
                auto width = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(input.width()) * scale_factor);
                auto height = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(input.height()) * scale_factor);
                return {width, height};
            }

            /**
             * @brief Infer scale factor from input and output texture dimensions
             *
             * @param input Source texture
             * @param output Target texture
             * @return Calculated scale factor
             * @throws std::runtime_error if non-uniform scaling is detected (X != Y scale)
             *
             * @note Requires uniform scaling (same factor for width and height)
             */
            static float infer_scale_factor(const gpu::input_texture& input,
                                            const gpu::output_texture& output) {
                float scale_x = SCALER_SIZE_TO_FLOAT(output.width()) / SCALER_SIZE_TO_FLOAT(input.width());
                float scale_y = SCALER_SIZE_TO_FLOAT(output.height()) / SCALER_SIZE_TO_FLOAT(input.height());

                // For uniform scaling, x and y scales should be equal
                if (std::abs(scale_x - scale_y) > 0.01f) {
                    throw std::runtime_error("Non-uniform scaling detected");
                }

                return scale_x;
            }

            /**
             * @brief Verify output texture dimensions match algorithm requirements
             *
             * @param input Source texture
             * @param output Target texture to verify
             * @param algo Scaling algorithm to check against
             * @return true if dimensions are valid for GPU scaling, false otherwise
             *
             * @note Checks both GPU algorithm availability and dimension matching
             */
            static bool verify_dimensions(const gpu::input_texture& input,
                                          const gpu::output_texture& output,
                                          algorithm algo) {
                float scale = infer_scale_factor(input, output);

                // Check if this scale is supported by the GPU algorithm
                if (!gpu::gpu_algorithm_traits::is_scale_supported_on_gpu(algo, scale)) {
                    return false;
                }

                auto expected = calculate_output_dimensions(input, algo, scale);
                return output.width() == expected.width && output.height() == expected.height;
            }

            /**
             * @brief Scale texture into preallocated output texture
             *
             * @param input Source texture to scale
             * @param output Preallocated destination texture
             * @param algo Scaling algorithm to use
             * @throws std::runtime_error if algorithm is not GPU-accelerated
             * @throws unsupported_scale_exception if inferred scale is not supported
             * @throws dimension_mismatch_exception if output size doesn't match requirements
             *
             * This is the primary GPU scaling pattern as it avoids texture allocation.
             * Scale factor is automatically inferred from the dimension ratio.
             *
             * @example
             * @code
             * GLuint output_tex = create_texture(width * 2, height * 2);
             * gpu::output_texture output(output_tex, width * 2, height * 2);
             * GPUScaler::scale(input, output, algorithm::EPX);
             * @endcode
             *
             * @note Requires active OpenGL context
             * @note Uses thread-local GPU scaler instance for efficiency
             */
            static void scale(const gpu::input_texture& input,
                              gpu::output_texture& output,
                              algorithm algo) {
                // Check if algorithm is available on GPU
                if (!algorithm_capabilities::is_gpu_accelerated(algo)) {
                    throw std::runtime_error(
                        std::string("Algorithm ") + scaler_capabilities::get_algorithm_name(algo) +
                        " is not available for GPU acceleration"
                    );
                }

                // Infer scale from dimensions
                float scale_factor = infer_scale_factor(input, output);

                // Validate scale factor
                if (!algorithm_capabilities::is_gpu_scale_supported(algo, scale_factor)) {
                    throw unsupported_scale_exception(algo, scale_factor,
                                                      algorithm_capabilities::get_info(algo).gpu_supported_scales);
                }

                // Verify dimensions
                auto expected = calculate_output_dimensions(input, algo, scale_factor);
                if (output.width() != expected.width || output.height() != expected.height) {
                    throw dimension_mismatch_exception(algo,
                                                       input.width(), input.height(),
                                                       output.width(), output.height(),
                                                       expected.width, expected.height);
                }

                // Get or create the GPU scaler instance
                static thread_local std::unique_ptr <gpu::opengl_texture_scaler> gpu_scaler;
                if (!gpu_scaler) {
                    gpu_scaler = std::make_unique <gpu::opengl_texture_scaler>();
                }

                // Perform the scaling
                gpu_scaler->scale_texture_to_texture(
                    input.id(),
                    SCALER_SIZE_TO_GLSIZEI(input.width()),
                    SCALER_SIZE_TO_GLSIZEI(input.height()),
                    output.id(),
                    SCALER_SIZE_TO_GLSIZEI(output.width()),
                    SCALER_SIZE_TO_GLSIZEI(output.height()),
                    algo
                );
            }

            /**
             * @brief Scale texture and create new output texture
             *
             * @param input Source texture to scale
             * @param algo Scaling algorithm to use
             * @param scale_factor Scaling multiplier
             * @return New output texture containing scaled result
             * @throws std::runtime_error if algorithm is not GPU-accelerated
             * @throws unsupported_scale_exception if scale is not supported
             *
             * Convenience method that creates the output texture automatically.
             * Less efficient than preallocated version due to texture allocation.
             *
             * @example
             * @code
             * auto scaled = GPUScaler::scale(input, algorithm::HQ, 3.0f);
             * // scaled.id() contains the OpenGL texture ID
             * @endcode
             *
             * @note The returned texture wrapper owns the OpenGL texture
             */
            static gpu::output_texture scale(const gpu::input_texture& input,
                                             algorithm algo,
                                             float scale_factor) {
                // Check if algorithm is available on GPU
                if (!algorithm_capabilities::is_gpu_accelerated(algo)) {
                    throw std::runtime_error(
                        std::string("Algorithm ") + scaler_capabilities::get_algorithm_name(algo) +
                        " is not available for GPU acceleration"
                    );
                }

                // Validate scale factor
                if (!algorithm_capabilities::is_gpu_scale_supported(algo, scale_factor)) {
                    throw unsupported_scale_exception(algo, scale_factor,
                                                      algorithm_capabilities::get_info(algo).gpu_supported_scales);
                }

                // Calculate output dimensions
                auto dims = calculate_output_dimensions(input, algo, scale_factor);

                // Create output texture
                GLuint output_tex = gpu::opengl_texture_scaler::create_output_texture(
                    SCALER_SIZE_TO_GLSIZEI(dims.width),
                    SCALER_SIZE_TO_GLSIZEI(dims.height)
                );

                // Create output wrapper
                gpu::output_texture output(output_tex, dims.width, dims.height);

                // Scale into the output texture
                scale(input, output, algo);

                return output;
            }

            /**
             * @brief Check if an algorithm has GPU acceleration support
             *
             * @param algo Algorithm to check
             * @return true if GPU implementation exists, false otherwise
             *
             * @example
             * @code
             * if (GPUScaler::is_gpu_accelerated(algorithm::EPX)) {
             *     // Use GPU scaling
             *     GPUScaler::scale(input, output, algorithm::EPX);
             * } else {
             *     // Fall back to CPU
             *     CPUScaler::scale(input, output, algorithm::EPX);
             * }
             * @endcode
             */
            static bool is_gpu_accelerated(algorithm algo) {
                return algorithm_capabilities::is_gpu_accelerated(algo);
            }

            /**
             * @brief Get list of scales supported by GPU implementation
             *
             * @param algo Algorithm to query
             * @return Vector of supported scale factors for GPU version
             *
             * @note GPU implementations may support different scales than CPU versions
             */
            static std::vector <float> get_gpu_scales(algorithm algo) {
                const auto& info = algorithm_capabilities::get_info(algo);
                return info.gpu_supported_scales;
            }
    };

    /**
     * @typedef GPUScaler
     * @brief Convenient alias for GPU texture scaler
     *
     * Provides shorter syntax for GPU scaling operations.
     *
     * @example
     * @code
     * // Instead of: unified_scaler<gpu::input_texture, gpu::output_texture>::scale(...)
     * // Use: GPUScaler::scale(...)
     * @endcode
     */
    using GPUScaler = unified_scaler <gpu::input_texture, gpu::output_texture>;

    /**
     * @example GPU Scaling Usage Examples
     *
     * @code
     * // Example 1: Basic GPU scaling with texture creation
     * GLuint input_tex = load_texture_from_file("input.png");
     * gpu::input_texture input(input_tex, width, height);
     *
     * // Scale 2x using GPU-accelerated EPX
     * auto scaled = GPUScaler::scale(input, algorithm::EPX, 2.0f);
     * // scaled.id() contains the output texture ID
     *
     * // Example 2: Scaling into preallocated texture (more efficient)
     * GLuint output_tex;
     * glGenTextures(1, &output_tex);
     * glBindTexture(GL_TEXTURE_2D, output_tex);
     * glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
     *              width * 3, height * 3, 0,
     *              GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
     *
     * gpu::output_texture output(output_tex, width * 3, height * 3);
     * GPUScaler::scale(input, output, algorithm::HQ);
     *
     * // Example 3: Check GPU availability before scaling
     * algorithm algo = algorithm::xBR;
     * if (GPUScaler::is_gpu_accelerated(algo)) {
     *     auto scaled = GPUScaler::scale(input, algo, 4.0f);
     *     // Use GPU result
     * } else {
     *     // Fall back to CPU scaling
     *     // Convert texture to CPU image first
     * }
     *
     * // Example 4: Query GPU-specific capabilities
     * auto gpu_scales = GPUScaler::get_gpu_scales(algorithm::HQ);
     * std::cout << "GPU HQ supports scales: ";
     * for (float scale : gpu_scales) {
     *     std::cout << scale << "x ";
     * }
     * std::cout << "\n";
     *
     * // Example 5: Batch processing with dimension verification
     * std::vector<GLuint> textures_to_scale = {...};
     * for (GLuint tex_id : textures_to_scale) {
     *     gpu::input_texture input(tex_id, width, height);
     *     gpu::output_texture output(output_tex, width * 2, height * 2);
     *
     *     if (GPUScaler::verify_dimensions(input, output, algorithm::EPX)) {
     *         GPUScaler::scale(input, output, algorithm::EPX);
     *         // Process scaled texture
     *     }
     * }
     *
     * // Example 6: Error handling for GPU scaling
     * try {
     *     // Try to use an algorithm that might not be on GPU
     *     auto scaled = GPUScaler::scale(input, algorithm::OmniScale, 2.0f);
     * } catch (const std::runtime_error& e) {
     *     std::cerr << "GPU scaling failed: " << e.what() << "\n";
     *     // Fall back to CPU or different algorithm
     * } catch (const unsupported_scale_exception& e) {
     *     std::cerr << "Scale not supported on GPU\n";
     *     // Try a different scale or algorithm
     * }
     *
     * // Example 7: Render-to-texture pattern
     * // Render scene to texture, then scale it
     * GLuint scene_tex = render_scene_to_texture();
     * gpu::input_texture scene(scene_tex, render_width, render_height);
     *
     * // Upscale for display
     * gpu::output_texture display(display_tex, display_width, display_height);
     * GPUScaler::scale(scene, display, algorithm::Bilinear);
     *
     * // Example 8: Multi-pass scaling
     * // Scale 6x using two 3x passes (some algorithms work better this way)
     * gpu::input_texture original(tex_id, w, h);
     *
     * // First pass: 3x
     * auto intermediate = GPUScaler::scale(original, algorithm::HQ, 3.0f);
     *
     * // Second pass: 2x (total 6x)
     * auto final = GPUScaler::scale(intermediate, algorithm::HQ, 2.0f);
     *
     * // Example 9: Performance comparison
     * auto start = std::chrono::high_resolution_clock::now();
     * GPUScaler::scale(input, output, algorithm::EPX);
     * glFinish(); // Wait for GPU to complete
     * auto gpu_time = std::chrono::high_resolution_clock::now() - start;
     *
     * std::cout << "GPU scaling took: "
     *           << std::chrono::duration<double, std::milli>(gpu_time).count()
     *           << " ms\n";
     * @endcode
     *
     * @note GPU scaling requires:
     * - Active OpenGL context
     * - OpenGL 3.3+ or OpenGL ES 3.0+
     * - Shader compilation support
     * - Framebuffer object support
     *
     * @note Performance considerations:
     * - GPU scaling is typically faster for large images (> 512x512)
     * - CPU might be faster for very small images due to GPU overhead
     * - Preallocated textures avoid allocation overhead
     * - Batch operations benefit from shader program caching
     */
} // namespace scaler
