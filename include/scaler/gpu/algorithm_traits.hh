#pragma once

#include <scaler/algorithm.hh>
#include <vector>
#include <unordered_map>
#include <string>

namespace scaler::gpu {

    /**
     * GPU-specific algorithm information
     */
    struct gpu_algorithm_info {
        bool supported;                    // Is this algorithm available on GPU?
        bool arbitrary_scale;              // Can handle any scale factor?
        int required_gl_version_major;     // Minimum OpenGL version required
        int required_gl_version_minor;
        int kernel_size;                   // Size of sampling kernel (3 for 3x3, 5 for 5x5, etc.)
        bool needs_neighborhood_access;    // Requires texture neighbor sampling
        std::vector<float> fixed_scales;   // For algorithms with fixed scale factors (empty if arbitrary)
        const char* shader_vertex_source;  // Vertex shader source (nullptr to use default)
        const char* shader_fragment_source;// Fragment shader source
    };

    /**
     * GPU algorithm traits and capabilities
     */
    class gpu_algorithm_traits {
    private:
        static const std::unordered_map<algorithm, gpu_algorithm_info>& get_algorithm_map();

    public:
        /**
         * Check if algorithm is GPU-accelerated
         */
        static bool is_gpu_accelerated(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);
            return it != map.end() && it->second.supported;
        }

        /**
         * Check if algorithm supports arbitrary scale factors on GPU
         */
        static bool supports_arbitrary_scale(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);
            return it != map.end() && it->second.arbitrary_scale;
        }

        /**
         * Get GPU-supported scale factors for an algorithm
         */
        static std::vector<float> get_gpu_supported_scales(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);

            if (it == map.end() || !it->second.supported) {
                return {};
            }

            if (it->second.arbitrary_scale) {
                // Return empty vector to indicate any scale is supported
                return {};
            }

            return it->second.fixed_scales;
        }

        /**
         * Check if a specific scale factor is supported on GPU
         */
        static bool is_scale_supported_on_gpu(algorithm algo, float scale) {
            if (!is_gpu_accelerated(algo)) {
                return false;
            }

            if (supports_arbitrary_scale(algo)) {
                // Arbitrary scale algorithms typically support 1.0 to 8.0
                return scale >= 1.0f && scale <= 8.0f;
            }

            auto scales = get_gpu_supported_scales(algo);
            return std::find(scales.begin(), scales.end(), scale) != scales.end();
        }

        /**
         * Get minimum OpenGL version required for algorithm
         */
        static std::pair<int, int> get_required_gl_version(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);

            if (it == map.end()) {
                return {3, 3}; // Default to OpenGL 3.3
            }

            return {it->second.required_gl_version_major,
                   it->second.required_gl_version_minor};
        }

        /**
         * Get shader sources for algorithm
         */
        static std::pair<const char*, const char*> get_shader_sources(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);

            if (it == map.end() || !it->second.supported) {
                return {nullptr, nullptr};
            }

            return {it->second.shader_vertex_source,
                   it->second.shader_fragment_source};
        }

        /**
         * Get kernel size for algorithm (for texture sampling)
         */
        static int get_kernel_size(algorithm algo) {
            auto& map = get_algorithm_map();
            auto it = map.find(algo);

            if (it == map.end()) {
                return 1;
            }

            return it->second.kernel_size;
        }

        /**
         * Get all GPU-accelerated algorithms
         */
        static std::vector<algorithm> get_gpu_algorithms() {
            std::vector<algorithm> result;
            auto& map = get_algorithm_map();

            for (const auto& [algo, info] : map) {
                if (info.supported) {
                    result.push_back(algo);
                }
            }

            return result;
        }

        /**
         * Get human-readable information about GPU support
         */
        static std::string get_gpu_support_info(algorithm algo) {
            if (!is_gpu_accelerated(algo)) {
                return "Not GPU-accelerated";
            }

            auto scales = get_gpu_supported_scales(algo);
            if (scales.empty()) {
                if (supports_arbitrary_scale(algo)) {
                    return "GPU-accelerated (any scale factor)";
                } else {
                    return "GPU-accelerated";
                }
            }

            std::string info = "GPU-accelerated (";
            for (size_t i = 0; i < scales.size(); ++i) {
                if (i > 0) info += ", ";
                info += std::to_string(static_cast<int>(scales[i])) + "x";
            }
            info += ")";
            return info;
        }

        /**
         * Compare GPU vs CPU capabilities for an algorithm
         */
        struct capability_comparison {
            std::vector<float> cpu_scales;
            std::vector<float> gpu_scales;
            bool gpu_has_arbitrary_scale;
            bool cpu_has_arbitrary_scale;
        };

        static capability_comparison compare_cpu_gpu_capabilities(algorithm algo);
    };

} // namespace scaler::gpu