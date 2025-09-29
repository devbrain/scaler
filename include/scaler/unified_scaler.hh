/**
 * @file unified_scaler.hh
 * @brief Unified interface for CPU-based image scaling algorithms
 *
 * This header provides a template-based, type-safe interface for scaling images
 * using various algorithms. It supports both dynamic output creation and scaling
 * into preallocated buffers, with automatic validation and error handling.
 *
 * @example Basic usage:
 * @code
 * // Scale an image 2x using EPX algorithm
 * auto scaled = scaler::unified_scaler<Image, Image>::scale(
 *     input, scaler::algorithm::EPX, 2.0f
 * );
 *
 * // Scale into preallocated buffer (scale inferred from dimensions)
 * Image output(input.width() * 3, input.height() * 3);
 * scaler::unified_scaler<Image, Image>::scale(
 *     input, output, scaler::algorithm::HQ
 * );
 * @endcode
 *
 * @note All methods are stateless and thread-safe.
 * @see algorithm.hh for available scaling algorithms
 * @see algorithm_capabilities.hh for querying algorithm support
 */
#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <cmath>

// Include algorithm definitions (shared with GPU)
#include <scaler/algorithm.hh>
#include <scaler/algorithm_capabilities.hh>
#include <scaler/warning_macros.hh>

// Include all algorithm implementations
#include <scaler/cpu/epx.hh>
#include <scaler/cpu/eagle.hh>
#include <scaler/cpu/scale3x.hh>
#include <scaler/cpu/scale2x_sfx.hh>
#include <scaler/cpu/scale3x_sfx.hh>
#include <scaler/cpu/2xsai.hh>
#include <scaler/cpu/hq2x.hh>
#include <scaler/cpu/hq3x.hh>
#include <scaler/cpu/aascale.hh>
#include <scaler/cpu/xbr.hh>
#include <scaler/cpu/omniscale.hh>
#include <scaler/cpu/bilinear.hh>
#include <scaler/cpu/trilinear.hh>

namespace scaler {

    /**
     * @class unsupported_scale_exception
     * @brief Exception thrown when an algorithm doesn't support the requested scale factor
     *
     * This exception provides detailed information about what scale was requested,
     * what algorithm was being used, and what scales are actually supported.
     *
     * @example
     * @code
     * try {
     *     auto scaled = unified_scaler<I,O>::scale(input, algorithm::EPX, 3.0f);
     * } catch (const unsupported_scale_exception& e) {
     *     std::cerr << "Algorithm: " << get_algorithm_name(e.m_algorithm)
     *               << " doesn't support " << e.m_requested_scale << "x\n";
     *     std::cerr << "Supported scales: ";
     *     for (float s : e.m_supported_scales) std::cerr << s << "x ";
     * }
     * @endcode
     */
    class unsupported_scale_exception : public std::runtime_error {
        public:
            /**
             * @brief Construct exception with details about the unsupported scale
             * @param algo The algorithm that was requested
             * @param requested The scale factor that was requested
             * @param supported List of scale factors that are actually supported
             */
            unsupported_scale_exception(algorithm algo, float requested,
                                        const std::vector <float>& supported)
                : std::runtime_error(make_message(algo, requested, supported))
                  , m_algorithm(algo)
                  , m_requested_scale(requested)
                  , m_supported_scales(supported) {
            }

            algorithm m_algorithm;           ///< Algorithm that was requested
            float m_requested_scale;         ///< Scale factor that was requested
            std::vector <float> m_supported_scales; ///< List of supported scales

        private:
            static std::string make_message(algorithm algo, float requested,
                                            const std::vector <float>& supported);
    };

    /**
     * @class dimension_mismatch_exception
     * @brief Exception thrown when preallocated output dimensions don't match requirements
     *
     * This exception is thrown when using the preallocated output overload and the
     * output buffer dimensions don't match what the algorithm expects for the
     * inferred scale factor.
     *
     * @example
     * @code
     * Image output(100, 100);  // Wrong size
     * try {
     *     unified_scaler<I,O>::scale(input, output, algorithm::HQ);
     * } catch (const dimension_mismatch_exception& e) {
     *     std::cerr << "Expected: " << e.m_expected_width << "x"
     *               << e.m_expected_height << "\n";
     *     std::cerr << "Provided: " << e.m_output_width << "x"
     *               << e.m_output_height << "\n";
     * }
     * @endcode
     */
    class dimension_mismatch_exception : public std::runtime_error {
        public:
            /**
             * @brief Construct exception with dimension mismatch details
             * @param algo Algorithm being used
             * @param input_width Width of input image
             * @param input_height Height of input image
             * @param output_width Width of provided output buffer
             * @param output_height Height of provided output buffer
             * @param expected_width Expected output width for the algorithm
             * @param expected_height Expected output height for the algorithm
             */
            dimension_mismatch_exception(algorithm algo,
                                        size_t input_width, size_t input_height,
                                        size_t output_width, size_t output_height,
                                        size_t expected_width, size_t expected_height)
                : std::runtime_error(make_message(algo, input_width, input_height,
                                                  output_width, output_height,
                                                  expected_width, expected_height))
                  , m_algorithm(algo)
                  , m_input_width(input_width), m_input_height(input_height)
                  , m_output_width(output_width), m_output_height(output_height)
                  , m_expected_width(expected_width), m_expected_height(expected_height) {
            }

            algorithm m_algorithm;                        ///< Algorithm being used
            size_t m_input_width, m_input_height;        ///< Input image dimensions
            size_t m_output_width, m_output_height;      ///< Provided output dimensions
            size_t m_expected_width, m_expected_height;  ///< Expected output dimensions

        private:
            static std::string make_message(algorithm algo,
                                            size_t input_width, size_t input_height,
                                            size_t output_width, size_t output_height,
                                            size_t expected_width, size_t expected_height);
    };

    /**
     * @class scaler_capabilities
     * @brief Query capabilities and properties of scaling algorithms
     *
     * This class provides static methods to query what scales each algorithm
     * supports, get human-readable names, and find algorithms that match
     * specific requirements.
     *
     * @note This is a compatibility wrapper around algorithm_capabilities
     */
    class scaler_capabilities {
    public:
        /**
         * @brief Get human-readable name for a scaling algorithm
         * @param algo Algorithm enum value
         * @return String name of the algorithm
         */
        static std::string get_algorithm_name(algorithm algo) {
            return algorithm_capabilities::get_algorithm_name(algo);
        }

        /**
         * @brief Get list of supported scale factors for an algorithm
         * @param algo Algorithm to query
         * @return Vector of supported scale factors (e.g., {2.0, 3.0, 4.0})
         */
        static std::vector<float> get_supported_scales(algorithm algo) {
            const auto& info = algorithm_capabilities::get_info(algo);
            return info.cpu_supported_scales;
        }

        /**
         * @brief Check if algorithm supports arbitrary (continuous) scaling
         * @param algo Algorithm to query
         * @return true if any scale factor is supported, false if only specific scales
         */
        static bool supports_arbitrary_scale(algorithm algo) {
            const auto& info = algorithm_capabilities::get_info(algo);
            return info.cpu_arbitrary_scale;
        }

        /**
         * @brief Check if a specific scale factor is supported
         * @param algo Algorithm to check
         * @param scale Scale factor to verify
         * @return true if the scale is supported, false otherwise
         */
        static bool is_scale_supported(algorithm algo, float scale) {
            return algorithm_capabilities::is_cpu_scale_supported(algo, scale);
        }

        /**
         * @brief Get list of all available scaling algorithms
         * @return Vector of all algorithm enum values
         */
        static std::vector<algorithm> get_all_algorithms() {
            return algorithm_capabilities::get_all_algorithms();
        }

        /**
         * @brief Find all algorithms that support a specific scale factor
         * @param scale Scale factor to search for
         * @return Vector of algorithms that support the given scale
         *
         * @example
         * @code
         * // Find all algorithms that can do 4x scaling
         * auto algos = scaler_capabilities::get_algorithms_for_scale(4.0f);
         * @endcode
         */
        static std::vector<algorithm> get_algorithms_for_scale(float scale) {
            return algorithm_capabilities::get_cpu_algorithms_for_scale(scale);
        }
    };

    /**
     * @class unified_scaler
     * @brief Template class providing unified interface for image scaling
     *
     * @tparam InputImage Type of input image (must have width(), height(), get_pixel())
     * @tparam OutputImage Type of output image (must have width(), height(), get_pixel(), set_pixel())
     *
     * This template class provides a consistent interface for scaling images using
     * various algorithms. It handles validation, dispatching, and error reporting.
     *
     * Required image interface:
     * - size_t width() const
     * - size_t height() const
     * - pixel_type get_pixel(size_t x, size_t y) const
     * - void set_pixel(size_t x, size_t y, const pixel_type& p)
     *
     * @example Type requirements:
     * @code
     * struct MyImage {
     *     using pixel_type = vec3<uint8_t>;
     *     size_t width() const;
     *     size_t height() const;
     *     pixel_type get_pixel(size_t x, size_t y) const;
     *     void set_pixel(size_t x, size_t y, const pixel_type& p);
     * };
     * @endcode
     */
    template<typename InputImage, typename OutputImage>
    class unified_scaler {
        public:
            /**
             * @struct dimensions
             * @brief Holds calculated output dimensions for a scaling operation
             */
            struct dimensions {
                size_t width;  ///< Output width in pixels
                size_t height; ///< Output height in pixels
            };

            /**
             * @brief Calculate expected output dimensions for scaling operation
             *
             * @param input Source image
             * @param algo Scaling algorithm (currently unused, for future algorithm-specific sizing)
             * @param scale_factor Scaling multiplier
             * @return dimensions struct with calculated width and height
             *
             * @note Most algorithms use simple multiplication, but this may vary for
             *       specialized algorithms in the future.
             */
            static dimensions calculate_output_dimensions(const InputImage& input,
                                                         [[maybe_unused]] algorithm algo,
                                                         float scale_factor) {
                // For most algorithms, output size is input size * scale
                size_t width = static_cast<size_t>(SCALER_SIZE_TO_FLOAT(input.width()) * scale_factor);
                size_t height = static_cast<size_t>(SCALER_SIZE_TO_FLOAT(input.height()) * scale_factor);
                return {width, height};
            }

            /**
             * @brief Infer scale factor from input and output image dimensions
             *
             * @param input Source image
             * @param output Target image
             * @return Calculated scale factor
             * @throws std::runtime_error if non-uniform scaling is detected (X != Y scale)
             *
             * @note Requires uniform scaling (same factor for width and height)
             */
            static float infer_scale_factor(const InputImage& input,
                                           const OutputImage& output) {
                float scale_x = SCALER_SIZE_TO_FLOAT(output.width()) / SCALER_SIZE_TO_FLOAT(input.width());
                float scale_y = SCALER_SIZE_TO_FLOAT(output.height()) / SCALER_SIZE_TO_FLOAT(input.height());

                // For uniform scaling, x and y scales should be equal
                // Allow small tolerance for floating point errors
                if (std::abs(scale_x - scale_y) > 0.01f) {
                    throw std::runtime_error("Non-uniform scaling detected");
                }

                return scale_x;
            }

            /**
             * @brief Verify output dimensions match algorithm requirements
             *
             * @param input Source image
             * @param output Target image to verify
             * @param algo Scaling algorithm to check against
             * @return true if dimensions are valid, false otherwise
             *
             * @note This method checks both scale factor support and dimension matching
             */
            static bool verify_dimensions(const InputImage& input,
                                         const OutputImage& output,
                                         algorithm algo) {
                float scale = infer_scale_factor(input, output);

                // Check if this scale is supported by the algorithm
                if (!scaler_capabilities::is_scale_supported(algo, scale)) {
                    return false;
                }

                auto expected = calculate_output_dimensions(input, algo, scale);
                return output.width() == expected.width && output.height() == expected.height;
            }

            /**
             * @brief Scale image and create new output (dynamic allocation)
             *
             * @param input Source image to scale
             * @param algo Scaling algorithm to use
             * @param scale_factor Scaling multiplier (default: 2.0)
             * @return New scaled image
             * @throws unsupported_scale_exception if algorithm doesn't support the scale
             *
             * @example
             * @code
             * auto scaled = unified_scaler<Image, Image>::scale(
             *     input, algorithm::EPX, 2.0f
             * );
             * @endcode
             */
            static OutputImage scale(const InputImage& input,
                                     algorithm algo,
                                     float scale_factor = 2.0f) {
                // Validate scale factor
                if (!scaler_capabilities::is_scale_supported(algo, scale_factor)) {
                    throw unsupported_scale_exception(algo, scale_factor,
                                                      scaler_capabilities::get_supported_scales(algo));
                }

                // Dispatch to appropriate implementation
                return dispatch_scale_algorithm(input, algo, scale_factor);
            }

            /**
             * @brief Scale image into preallocated output buffer
             *
             * @param input Source image to scale
             * @param output Preallocated destination image
             * @param algo Scaling algorithm to use
             * @throws unsupported_scale_exception if inferred scale is not supported
             * @throws dimension_mismatch_exception if output size doesn't match requirements
             *
             * Scale factor is automatically inferred from the dimension ratio.
             *
             * @example
             * @code
             * Image output(input.width() * 2, input.height() * 2);
             * unified_scaler<Image, Image>::scale(input, output, algorithm::HQ);
             * @endcode
             *
             * @note This overload is more efficient as it avoids memory allocation
             */
            static void scale(const InputImage& input,
                             OutputImage& output,
                             algorithm algo) {
                // Infer scale from dimensions
                float scale_factor = infer_scale_factor(input, output);

                // Validate scale factor
                if (!scaler_capabilities::is_scale_supported(algo, scale_factor)) {
                    throw unsupported_scale_exception(algo, scale_factor,
                                                      scaler_capabilities::get_supported_scales(algo));
                }

                // Verify dimensions
                auto expected = calculate_output_dimensions(input, algo, scale_factor);
                if (output.width() != expected.width || output.height() != expected.height) {
                    throw dimension_mismatch_exception(algo,
                                                       input.width(), input.height(),
                                                       output.width(), output.height(),
                                                       expected.width, expected.height);
                }

                // Dispatch to appropriate implementation - writes directly to output
                dispatch_scale_algorithm_into(input, output, algo, scale_factor);
            }

        private:
            // Dispatch method that writes directly to output (efficient version)
            static void dispatch_scale_algorithm_into(const InputImage& input,
                                                     OutputImage& output,
                                                     algorithm algo,
                                                     float scale_factor) {
                // Dispatch to appropriate implementation
                switch (algo) {
                    case algorithm::Nearest:
                        scale_nearest_into(input, output, scale_factor);
                        break;

                    case algorithm::Bilinear:
                        scale_bilinear <InputImage, OutputImage>(input, output, scale_factor);
                        break;

                    case algorithm::Trilinear:
                        scale_trilinear_into(input, output, scale_factor);
                        break;

                    case algorithm::EPX:
                        scale_epx <InputImage, OutputImage>(input, output, 2);
                        break;

                    case algorithm::Eagle:
                        scale_eagle <InputImage, OutputImage>(input, output, 2);
                        break;

                    case algorithm::Scale:
                        dispatch_scale_into(input, output, scale_factor);
                        break;

                    case algorithm::ScaleSFX:
                        dispatch_scale_sfx_into(input, output, scale_factor);
                        break;

                    case algorithm::Super2xSaI:
                        scale_2x_sai_into(input, output, 2);
                        break;

                    case algorithm::HQ:
                        dispatch_hq_into(input, output, scale_factor);
                        break;

                    case algorithm::AAScale:
                        dispatch_aa_scale_into(input, output, scale_factor);
                        break;

                    case algorithm::xBR:
                        dispatch_xbr_into(input, output, scale_factor);
                        break;

                    case algorithm::OmniScale:
                        dispatch_omniscale_into(input, output, scale_factor);
                        break;

                    default:
                        throw std::runtime_error("algorithm not implemented yet");
                }
            }

            // Main dispatch method that creates output (original behavior)
            static OutputImage dispatch_scale_algorithm(const InputImage& input,
                                                       algorithm algo,
                                                       float scale_factor) {
                // Dispatch to appropriate implementation
                switch (algo) {
                    case algorithm::Nearest:
                        return scale_nearest(input, scale_factor);

                    case algorithm::Bilinear:
                        return scale_bilinear <InputImage, OutputImage>(input, scale_factor);

                    case algorithm::Trilinear:
                        return scale_trilinear <InputImage, OutputImage>(input, scale_factor);

                    case algorithm::EPX:
                        return scale_epx <InputImage, OutputImage>(input, 2);

                    case algorithm::Eagle:
                        return scale_eagle <InputImage, OutputImage>(input, 2);

                    case algorithm::Scale:
                        return dispatch_scale(input, scale_factor);

                    case algorithm::ScaleSFX:
                        return dispatch_scale_sfx(input, scale_factor);

                    case algorithm::Super2xSaI:
                        return scale_2x_sai <InputImage, OutputImage>(input, 2);

                    case algorithm::HQ:
                        return dispatch_hq(input, scale_factor);

                    case algorithm::AAScale:
                        return dispatch_aa_scale(input, scale_factor);

                    case algorithm::xBR:
                        return dispatch_xbr(input, scale_factor);

                    case algorithm::OmniScale:
                        SCALER_DISABLE_WARNING_PUSH
                        SCALER_DISABLE_WARNING_FLOAT_EQUAL
                        if (scale_factor == 2.0f) {
                            return scale_omni_scale_2x <InputImage, OutputImage>(input, 2);
                        } else if (scale_factor == 3.0f) {
                            return scale_omni_scale_3x <InputImage, OutputImage>(input, 3);
                        SCALER_DISABLE_WARNING_POP
                        } else {
                            // For other scales, use repeated application or nearest neighbor
                            // This is a temporary solution
                            auto temp = scale_omni_scale_2x <InputImage, OutputImage>(input, 2);
                            float remaining_scale = scale_factor / 2.0f;
                            return scale_nearest(temp, remaining_scale);
                        }

                    default:
                        throw std::runtime_error("algorithm not implemented yet");
                }
            }

            // Dispatch helpers for multi-scale algorithms
            static OutputImage dispatch_scale(const InputImage& input, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        return scale_adv_mame <InputImage, OutputImage>(input, 2);
                    case 3:
                        return scale_scale_3x <InputImage, OutputImage>(input, 3);
                    case 4: {
                        // Scale4x is Scale2x applied twice
                        auto temp = scale_adv_mame <InputImage, OutputImage>(input, 2);
                        return scale_adv_mame <decltype(temp), OutputImage>(temp, 2);
                    }
                    default:
                        throw std::logic_error("Invalid scale factor for Scale algorithm");
                }
            }

            static OutputImage dispatch_scale_sfx(const InputImage& input, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        return scale_scale_2x_sfx <InputImage, OutputImage>(input, 2);
                    case 3:
                        return scale_scale_3x_sfx <InputImage, OutputImage>(input, 3);
                    default:
                        throw std::logic_error("Invalid scale factor for ScaleSFX algorithm");
                }
            }

            static OutputImage dispatch_hq(const InputImage& input, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        return scale_hq2x <InputImage, OutputImage>(input, 2);
                    case 3:
                        return scale_hq_3x <InputImage, OutputImage>(input);
                    case 4: {
                        // HQ4x would go here when implemented
                        // For now, use HQ2x twice
                        auto temp = scale_hq2x <InputImage, OutputImage>(input, 2);
                        return scale_hq2x <decltype(temp), OutputImage>(temp, 2);
                    }
                    default:
                        throw std::logic_error("Invalid scale factor for HQ algorithm");
                }
            }

            static OutputImage dispatch_aa_scale(const InputImage& input, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        return scale_aa_scale_2x <InputImage, OutputImage>(input, 2);
                    case 4:
                        return scale_aa_scale_4x <InputImage, OutputImage>(input, 4);
                    default:
                        throw std::logic_error("Invalid scale factor for AAScale algorithm");
                }
            }

            static OutputImage dispatch_xbr(const InputImage& input, float scale_factor) {
                // XBR currently only supports 2x, for 3x and 4x we can apply it multiple times
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        return scale_xbr <InputImage, OutputImage>(input, 2);
                    case 3:
                        // Apply 2x then scale up with nearest neighbor to 3x
                        // This is a temporary solution until native 3x is implemented
                    {
                        auto temp = scale_xbr <InputImage, OutputImage>(input, 2);
                        return scale_nearest(temp, 1.5f);
                    }
                    case 4:
                        // Apply 2x twice for 4x
                    {
                        auto temp = scale_xbr <InputImage, OutputImage>(input, 2);
                        return scale_xbr <decltype(temp), OutputImage>(temp, 2);
                    }
                    default:
                        throw std::logic_error("Invalid scale factor for xBR algorithm");
                }
            }

            // Helper dispatch functions for _into versions
            static void dispatch_scale_into(const InputImage& input, OutputImage& output, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        scale_adv_mame <InputImage, OutputImage>(input, output, 2);
                        break;
                    case 3:
                        scale_scale_3x <InputImage, OutputImage>(input, output, 3);
                        break;
                    case 4: {
                        // Scale4x is Scale2x applied twice - need temporary
                        auto temp = scale_adv_mame <InputImage, OutputImage>(input, 2);
                        scale_adv_mame <decltype(temp), OutputImage>(temp, output, 2);
                        break;
                    }
                    default:
                        throw std::logic_error("Invalid scale factor for Scale algorithm");
                }
            }

            static void dispatch_scale_sfx_into(const InputImage& input, OutputImage& output, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        scale_scale_2x_sfx <InputImage, OutputImage>(input, output, 2);
                        break;
                    case 3:
                        scale_scale_3x_sfx <InputImage, OutputImage>(input, output, 3);
                        break;
                    default:
                        throw std::logic_error("Invalid scale factor for ScaleSFX algorithm");
                }
            }

            static void dispatch_hq_into(const InputImage& input, OutputImage& output, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        scale_hq2x <InputImage, OutputImage>(input, output, 2);
                        break;
                    case 3:
                        scale_hq_3x <InputImage, OutputImage>(input, output);
                        break;
                    case 4: {
                        // HQ4x would go here when implemented
                        // For now, use HQ2x twice - need temporary
                        auto temp = scale_hq2x <InputImage, OutputImage>(input, 2);
                        scale_hq2x <decltype(temp), OutputImage>(temp, output, 2);
                        break;
                    }
                    default:
                        throw std::logic_error("Invalid scale factor for HQ algorithm");
                }
            }

            static void dispatch_aa_scale_into(const InputImage& input, OutputImage& output, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        scale_aa_scale_2x <InputImage, OutputImage>(input, output, 2);
                        break;
                    case 4:
                        scale_aa_scale_4x <InputImage, OutputImage>(input, output, 4);
                        break;
                    default:
                        throw std::logic_error("Invalid scale factor for AAScale algorithm");
                }
            }

            static void dispatch_xbr_into(const InputImage& input, OutputImage& output, float scale_factor) {
                switch (static_cast <int>(scale_factor)) {
                    case 2:
                        scale_xbr <InputImage, OutputImage>(input, output, 2);
                        break;
                    case 3:
                    case 4:
                        // For 3x and 4x, we still need to use the temporary approach
                        // as they require multiple passes
                        {
                            OutputImage temp = dispatch_xbr(input, scale_factor);
                            for (size_t y = 0; y < output.height(); ++y) {
                                for (size_t x = 0; x < output.width(); ++x) {
                                    output.set_pixel(x, y, temp.get_pixel(x, y));
                                }
                            }
                        }
                        break;
                    default:
                        throw std::logic_error("Invalid scale factor for xBR algorithm");
                }
            }

            static void dispatch_omniscale_into(const InputImage& input, OutputImage& output, float scale_factor) {
                SCALER_DISABLE_WARNING_PUSH
                SCALER_DISABLE_WARNING_FLOAT_EQUAL
                if (scale_factor == 2.0f) {
                    scale_omni_scale_2x <InputImage, OutputImage>(input, output, 2);
                } else if (scale_factor == 3.0f) {
                    scale_omni_scale_3x <InputImage, OutputImage>(input, output, 3);
                SCALER_DISABLE_WARNING_POP
                } else {
                    // For other scales, use nearest neighbor fallback
                    scale_nearest_into(input, output, scale_factor);
                }
            }

            // All CPU scalers have been refactored to accept output reference directly

            static void scale_2x_sai_into(const InputImage& input, OutputImage& output, int scale) {
                OutputImage temp = scale_2x_sai<InputImage, OutputImage>(input, static_cast<size_t>(scale));
                for (size_t y = 0; y < output.height(); ++y) {
                    for (size_t x = 0; x < output.width(); ++x) {
                        output.set_pixel(x, y, temp.get_pixel(x, y));
                    }
                }
            }

            static void scale_trilinear_into(const InputImage& input, OutputImage& output, float scale_factor) {
                OutputImage temp = scale_trilinear<InputImage, OutputImage>(input, scale_factor);
                for (size_t y = 0; y < output.height(); ++y) {
                    for (size_t x = 0; x < output.width(); ++x) {
                        output.set_pixel(x, y, temp.get_pixel(x, y));
                    }
                }
            }

            // Simple nearest neighbor scaling that writes to output
            template<typename AnyInput>
            static void scale_nearest_into(const AnyInput& input, OutputImage& output, float scale_factor) {
                auto out_width = output.width();
                auto out_height = output.height();

                for (size_t y = 0; y < out_height; ++y) {
                    auto src_y = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(y) / scale_factor);
                    for (size_t x = 0; x < out_width; ++x) {
                        auto src_x = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(x) / scale_factor);
                        output.set_pixel(x, y, input.get_pixel(src_x, src_y));
                    }
                }
            }

            // Simple nearest neighbor scaling (for any scale factor)
            template<typename AnyInput>
            static OutputImage scale_nearest(const AnyInput& input, float scale_factor) {
                auto out_width = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(input.width()) * scale_factor);
                auto out_height = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(input.height()) * scale_factor);

                OutputImage output(out_width, out_height, input);

                for (size_t y = 0; y < out_height; ++y) {
                    auto src_y = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(y) / scale_factor);
                    for (size_t x = 0; x < out_width; ++x) {
                        auto src_x = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(x) / scale_factor);
                        output.set_pixel(x, y, input.get_pixel(src_x, src_y));
                    }
                }

                return output;
            }
    };

    // Convenience typedef for common use case
    template<typename InputImage, typename OutputImage>
    using Scaler = unified_scaler <InputImage, OutputImage>;

    // Implementation of unsupported_scale_exception::make_message
    // Must be defined after scaler_capabilities
    inline std::string unsupported_scale_exception::make_message(
        algorithm algo, float requested, const std::vector <float>& supported) {
        std::stringstream ss;
        ss << scaler_capabilities::get_algorithm_name(algo)
            << " algorithm doesn't support " << requested << "x scaling. ";

        if (supported.empty()) {
            ss << "This algorithm supports arbitrary scaling.";
        } else {
            ss << "Supported scales: ";
            for (size_t i = 0; i < supported.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << supported[i] << "x";
            }
        }
        return ss.str();
    }

    // Implementation of dimension_mismatch_exception::make_message
    inline std::string dimension_mismatch_exception::make_message(
        algorithm algo,
        size_t input_width, size_t input_height,
        size_t output_width, size_t output_height,
        size_t expected_width, size_t expected_height) {
        std::stringstream ss;
        ss << "Dimension mismatch for " << scaler_capabilities::get_algorithm_name(algo)
           << " algorithm. Input: " << input_width << "x" << input_height
           << ", Output: " << output_width << "x" << output_height
           << ", Expected: " << expected_width << "x" << expected_height;
        return ss.str();
    }
    /**
     * @example Basic Usage Examples
     *
     * @code
     * // Example 1: Simple 2x scaling with EPX algorithm
     * auto input = load_image("input.png");
     * auto scaled = scaler::unified_scaler<Image, Image>::scale(
     *     input, scaler::algorithm::EPX
     * );
     * save_image(scaled, "output_2x.png");
     *
     * // Example 2: Custom scale factor with bilinear interpolation
     * auto scaled_custom = scaler::unified_scaler<Image, Image>::scale(
     *     input, scaler::algorithm::Bilinear, 1.5f
     * );
     *
     * // Example 3: Scaling into preallocated buffer
     * Image output(input.width() * 3, input.height() * 3);
     * scaler::unified_scaler<Image, Image>::scale(
     *     input, output, scaler::algorithm::HQ
     * );
     *
     * // Example 4: Query algorithm capabilities
     * auto algos_for_4x = scaler::scaler_capabilities::get_algorithms_for_scale(4.0f);
     * for (auto algo : algos_for_4x) {
     *     std::cout << scaler::scaler_capabilities::get_algorithm_name(algo) << "\n";
     * }
     *
     * // Example 5: Error handling
     * try {
     *     auto result = scaler::unified_scaler<Image, Image>::scale(
     *         input, scaler::algorithm::EPX, 5.0f  // EPX only supports 2x
     *     );
     * } catch (const scaler::unsupported_scale_exception& e) {
     *     std::cerr << "Error: " << e.what() << "\n";
     *     // Use fallback algorithm
     *     auto result = scaler::unified_scaler<Image, Image>::scale(
     *         input, scaler::algorithm::Bilinear, 5.0f
     *     );
     * }
     *
     * // Example 6: Batch processing with dimension verification
     * std::vector<Image> outputs;
     * for (const auto& input : inputs) {
     *     Image output(input.width() * 2, input.height() * 2);
     *     if (scaler::unified_scaler<Image, Image>::verify_dimensions(
     *             input, output, scaler::algorithm::HQ)) {
     *         scaler::unified_scaler<Image, Image>::scale(
     *             input, output, scaler::algorithm::HQ
     *         );
     *         outputs.push_back(std::move(output));
     *     }
     * }
     *
     * // Example 7: Finding best algorithm for content type
     * scaler::algorithm best_algo;
     * if (is_pixel_art) {
     *     // For pixel art, prefer algorithms that preserve sharp edges
     *     best_algo = scaler::algorithm::EPX;  // or Eagle, Scale, HQ
     * } else if (is_photograph) {
     *     // For photos, use smooth interpolation
     *     best_algo = scaler::algorithm::Bilinear;  // or Trilinear
     * } else {
     *     // General purpose
     *     best_algo = scaler::algorithm::xBR;  // or OmniScale
     * }
     * @endcode
     */

} // namespace scaler
