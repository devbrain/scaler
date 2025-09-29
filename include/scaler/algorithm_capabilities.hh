#pragma once

#include <scaler/algorithm.hh>
#include <scaler/warning_macros.hh>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace scaler {

    /**
     * Extended algorithm information including both CPU and GPU capabilities
     */
    class algorithm_capabilities {
    public:
        struct algorithm_info {
            std::string name;
            std::string description;

            // CPU capabilities
            std::vector<float> cpu_supported_scales;  // Empty = any scale
            bool cpu_arbitrary_scale;

            // GPU capabilities
            std::vector<float> gpu_supported_scales;  // Empty = any scale
            bool gpu_arbitrary_scale;
            bool gpu_accelerated;

            // Common limits
            float min_scale = 1.0f;
            float max_scale = 8.0f;
        };

        /**
         * Get complete algorithm information
         */
        static const algorithm_info& get_info(algorithm algo) {
            static const auto& db = get_algorithm_database();
            auto it = db.find(algo);
            if (it != db.end()) {
                return it->second;
            }

            static const algorithm_info unknown = {
                "Unknown", "Unknown algorithm", {}, false, {}, false, false, 1.0f, 8.0f
            };
            return unknown;
        }

        /**
         * Get algorithm name
         */
        static std::string get_algorithm_name(algorithm algo) {
            return get_info(algo).name;
        }

        /**
         * Get algorithm description
         */
        static std::string get_algorithm_description(algorithm algo) {
            return get_info(algo).description;
        }

        /**
         * Check if scale is supported on CPU
         */
        static bool is_cpu_scale_supported(algorithm algo, float scale) {
            const auto& info = get_info(algo);
            if (info.cpu_arbitrary_scale) {
                return scale >= info.min_scale && scale <= info.max_scale;
            }
            const auto& supported = info.cpu_supported_scales;
            return std::find(supported.begin(), supported.end(), scale) != supported.end();
        }

        /**
         * Check if scale is supported on GPU
         */
        static bool is_gpu_scale_supported(algorithm algo, float scale) {
            const auto& info = get_info(algo);
            if (!info.gpu_accelerated) {
                return false;
            }
            if (info.gpu_arbitrary_scale) {
                return scale >= info.min_scale && scale <= info.max_scale;
            }
            const auto& supported = info.gpu_supported_scales;
            return std::find(supported.begin(), supported.end(), scale) != supported.end();
        }

        /**
         * Check if algorithm is GPU-accelerated
         */
        static bool is_gpu_accelerated(algorithm algo) {
            return get_info(algo).gpu_accelerated;
        }

        /**
         * Get list of all algorithms
         */
        static std::vector<algorithm> get_all_algorithms() {
            return {
                algorithm::Nearest,
                algorithm::Bilinear,
                algorithm::Trilinear,
                algorithm::EPX,
                algorithm::Eagle,
                algorithm::Scale,
                algorithm::ScaleSFX,
                algorithm::Super2xSaI,
                algorithm::HQ,
                algorithm::AAScale,
                algorithm::xBR,
                algorithm::OmniScale
            };
        }

        /**
         * Get algorithms that support a specific scale on CPU
         */
        static std::vector<algorithm> get_cpu_algorithms_for_scale(float scale) {
            std::vector<algorithm> result;
            for (algorithm algo : get_all_algorithms()) {
                if (is_cpu_scale_supported(algo, scale)) {
                    result.push_back(algo);
                }
            }
            return result;
        }

        /**
         * Get algorithms that support a specific scale on GPU
         */
        static std::vector<algorithm> get_gpu_algorithms_for_scale(float scale) {
            std::vector<algorithm> result;
            for (algorithm algo : get_all_algorithms()) {
                if (is_gpu_scale_supported(algo, scale)) {
                    result.push_back(algo);
                }
            }
            return result;
        }

        /**
         * Get all GPU-accelerated algorithms
         */
        static std::vector<algorithm> get_gpu_algorithms() {
            std::vector<algorithm> result;
            for (algorithm algo : get_all_algorithms()) {
                if (is_gpu_accelerated(algo)) {
                    result.push_back(algo);
                }
            }
            return result;
        }

        /**
         * Get GPU-supported scales for a specific algorithm
         */
        static std::vector<float> get_gpu_scales_for_algorithm(algorithm algo) {
            const auto& info = get_info(algo);
            return info.gpu_supported_scales;
        }

        /**
         * Check if algorithm supports arbitrary scale on GPU
         */
        static bool gpu_supports_arbitrary_scale(algorithm algo) {
            const auto& info = get_info(algo);
            return info.gpu_accelerated && info.gpu_arbitrary_scale;
        }

        /**
         * Backend recommendation based on algorithm and image size
         */
        enum class recommended_backend { CPU, GPU, EITHER };

        static recommended_backend recommend_backend(algorithm algo,
                                                    size_t width,
                                                    size_t height,
                                                    float scale_factor) {
            const auto& info = get_info(algo);
            size_t pixels = width * height;

            // Not GPU accelerated? Use CPU
            if (!info.gpu_accelerated) {
                return recommended_backend::CPU;
            }

            // Check scale support
            bool cpu_supports = is_cpu_scale_supported(algo, scale_factor);
            bool gpu_supports = is_gpu_scale_supported(algo, scale_factor);

            if (!gpu_supports) {
                return recommended_backend::CPU;
            }
            if (!cpu_supports) {
                return recommended_backend::GPU;
            }

            // Special case: OmniScale with non-2x/3x scale - GPU only
            SCALER_DISABLE_WARNING_PUSH
            SCALER_DISABLE_WARNING_FLOAT_EQUAL
            if (algo == algorithm::OmniScale &&
                scale_factor != 2.0f && scale_factor != 3.0f) {
                return recommended_backend::GPU;
            }
            SCALER_DISABLE_WARNING_POP

            // For small images, CPU is often faster due to GPU overhead
            if (pixels < 64 * 64) {
                return recommended_backend::CPU;
            }

            // For large images, GPU is usually better
            if (pixels > 512 * 512) {
                return recommended_backend::GPU;
            }

            // Medium size - either is fine
            return recommended_backend::EITHER;
        }

    private:
        static const std::map<algorithm, algorithm_info>& get_algorithm_database() {
            static const std::map<algorithm, algorithm_info> db = {
                {algorithm::Nearest, {
                    "Nearest", "Nearest neighbor - fastest, pixelated",
                    {}, true,      // CPU: any scale
                    {}, true, true, // GPU: any scale, accelerated
                    0.1f, 10.0f
                }},

                {algorithm::Bilinear, {
                    "Bilinear", "Bilinear interpolation - smooth but blurry",
                    {}, true,      // CPU: any scale
                    {}, true, true, // GPU: any scale, accelerated
                    0.1f, 10.0f
                }},

                {algorithm::Trilinear, {
                    "Trilinear", "Trilinear with mipmapping - good for downscaling",
                    {}, true,      // CPU: any scale
                    {}, false, false, // GPU: not implemented yet
                    0.1f, 10.0f
                }},

                {algorithm::EPX, {
                    "EPX", "Eric's Pixel Expansion - good for pixel art",
                    {2.0f}, false,  // CPU: 2x only
                    {2.0f}, false, true, // GPU: 2x only, accelerated
                    2.0f, 2.0f
                }},

                {algorithm::Eagle, {
                    "Eagle", "Eagle algorithm - smooth diagonal lines",
                    {2.0f}, false,  // CPU: 2x only
                    {2.0f}, false, true, // GPU: 2x only, accelerated
                    2.0f, 2.0f
                }},

                {algorithm::Scale, {
                    "Scale", "AdvMAME Scale2x/3x/4x - sharp pixel art",
                    {2.0f, 3.0f, 4.0f}, false,  // CPU: 2x, 3x, 4x
                    {2.0f, 3.0f, 4.0f}, false, true, // GPU: same, accelerated
                    2.0f, 4.0f
                }},

                {algorithm::ScaleSFX, {
                    "ScaleSFX", "Sp00kyFox improved Scale - better edges",
                    {2.0f, 3.0f}, false,  // CPU: 2x, 3x
                    {2.0f, 3.0f}, false, true, // GPU: same, accelerated
                    2.0f, 3.0f
                }},

                {algorithm::Super2xSaI, {
                    "Super2xSaI", "Super 2xSaI - smooth interpolation",
                    {2.0f}, false,  // CPU: 2x only
                    {2.0f}, false, true, // GPU: 2x only, accelerated
                    2.0f, 2.0f
                }},

                {algorithm::HQ, {
                    "HQ", "High Quality 2x/3x/4x - excellent quality",
                    {2.0f, 3.0f, 4.0f}, false,  // CPU: 2x, 3x, 4x
                    {}, false, false, // GPU: not accelerated (too complex)
                    2.0f, 4.0f
                }},

                {algorithm::AAScale, {
                    "AAScale", "Anti-Aliased Scale - smooth edges",
                    {2.0f, 4.0f}, false,  // CPU: 2x, 4x
                    {2.0f, 4.0f}, false, true, // GPU: same, accelerated
                    2.0f, 4.0f
                }},

                {algorithm::xBR, {
                    "xBR", "Hyllian's xBR - advanced edge interpolation",
                    {2.0f, 3.0f, 4.0f}, false,  // CPU: 2x, 3x, 4x
                    {}, false, false, // GPU: not accelerated (too complex)
                    2.0f, 4.0f
                }},

                {algorithm::OmniScale, {
                    "OmniScale", "OmniScale - resolution independent (GPU)",
                    {2.0f, 3.0f}, false,  // CPU: 2x, 3x only
                    {}, true, true,       // GPU: any scale, accelerated!
                    1.0f, 8.0f
                }},
            };

            return db;
        }
    };

} // namespace scaler