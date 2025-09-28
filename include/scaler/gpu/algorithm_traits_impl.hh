#pragma once

#include <scaler/gpu/algorithm_traits.hh>
#include <scaler/gpu/shader_source.hh>
#include <scaler/warning_macros.hh>

namespace scaler::gpu {

    inline const std::unordered_map<algorithm, gpu_algorithm_info>&
    gpu_algorithm_traits::get_algorithm_map() {
        // Note: shader sources will be linked from shader_source.hh
        static const std::unordered_map<algorithm, gpu_algorithm_info> algorithm_map = {
            // Simple scalers
            {algorithm::Nearest, {
                true,                           // supported
                true,                           // arbitrary_scale
                3, 3,                           // GL version 3.3
                1,                              // kernel_size
                false,                          // needs_neighborhood_access
                {},                             // fixed_scales (empty = arbitrary)
                nullptr,                        // vertex shader (use default)
                shader_source::nearest_fragment_shader
            }},

            {algorithm::Bilinear, {
                true,                           // supported
                true,                           // arbitrary_scale
                3, 3,                           // GL version 3.3
                2,                              // kernel_size
                true,                           // needs_neighborhood_access
                {},                             // fixed_scales
                nullptr,
                shader_source::bilinear_fragment_shader
            }},

            // Classic pixel art scalers
            {algorithm::EPX, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f},                         // fixed_scales
                nullptr,
                shader_source::epx_fragment_shader
            }},

            {algorithm::Eagle, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f},                         // fixed_scales
                nullptr,
                shader_source::eagle_fragment_shader
            }},

            {algorithm::Scale, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f, 3.0f, 4.0f},            // fixed_scales
                nullptr,
                nullptr  // Will be selected based on scale
            }},

            {algorithm::ScaleSFX, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                5,                              // kernel_size (Scale2xSFX uses 5x5)
                true,                           // needs_neighborhood_access
                {2.0f, 3.0f},                  // fixed_scales
                nullptr,
                nullptr  // Will be selected based on scale
            }},

            {algorithm::Super2xSaI, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                4,                              // kernel_size (4x4 for 2xSaI)
                true,                           // needs_neighborhood_access
                {2.0f},                         // fixed_scales
                nullptr,
                shader_source::twoxsai_fragment_shader
            }},

            // High quality family - complex, not all GPU-friendly
            {algorithm::HQ, {
                false,                          // NOT supported (too complex for GPU)
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f, 3.0f, 4.0f},            // fixed_scales
                nullptr,
                nullptr
            }},

            // Anti-aliased scaling
            {algorithm::AAScale, {
                true,                           // supported
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f, 4.0f},                  // fixed_scales
                nullptr,
                nullptr  // Will be selected based on scale
            }},

            // Advanced algorithms
            {algorithm::xBR, {
                false,                          // NOT supported (too complex)
                false,                          // arbitrary_scale
                3, 3,                           // GL version 3.3
                5,                              // kernel_size
                true,                           // needs_neighborhood_access
                {2.0f, 3.0f, 4.0f},            // fixed_scales
                nullptr,
                nullptr
            }},

            // Resolution independent - SPECIAL CASE
            {algorithm::OmniScale, {
                true,                           // supported
                true,                           // arbitrary_scale on GPU!
                3, 3,                           // GL version 3.3
                3,                              // kernel_size
                true,                           // needs_neighborhood_access
                {},                             // GPU supports arbitrary scale
                nullptr,
                shader_source::omniscale_fragment_shader
            }},
        };

        return algorithm_map;
    }

    inline gpu_algorithm_traits::capability_comparison
    gpu_algorithm_traits::compare_cpu_gpu_capabilities(algorithm algo) {
        capability_comparison result;

        // Get GPU capabilities
        result.gpu_scales = get_gpu_supported_scales(algo);
        result.gpu_has_arbitrary_scale = supports_arbitrary_scale(algo);

        // CPU capabilities (hardcoded for now, should be moved to CPU traits)
        switch (algo) {
            case algorithm::OmniScale:
                // CPU only supports 2x and 3x
                result.cpu_scales = {2.0f, 3.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::Nearest:
            case algorithm::Bilinear:
            case algorithm::Trilinear:
                result.cpu_has_arbitrary_scale = true;
                break;

            case algorithm::EPX:
            case algorithm::Eagle:
            case algorithm::Super2xSaI:
                result.cpu_scales = {2.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::Scale:
                result.cpu_scales = {2.0f, 3.0f, 4.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::ScaleSFX:
                result.cpu_scales = {2.0f, 3.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::HQ:
                result.cpu_scales = {2.0f, 3.0f, 4.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::AAScale:
                result.cpu_scales = {2.0f, 4.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            case algorithm::xBR:
                result.cpu_scales = {2.0f, 3.0f, 4.0f};
                result.cpu_has_arbitrary_scale = false;
                break;

            default:
                result.cpu_has_arbitrary_scale = false;
                break;
        }

        return result;
    }

    /**
     * Helper to get the appropriate shader source based on algorithm and scale
     */
    inline const char* get_shader_for_algorithm_and_scale(algorithm algo, float scale) {
        // Handle algorithms with scale-specific shaders
        SCALER_DISABLE_WARNING_PUSH
        SCALER_DISABLE_WARNING_FLOAT_EQUAL
        switch (algo) {
            case algorithm::Scale:
                if (scale == 2.0f) return shader_source::scale2x_fragment_shader;
                if (scale == 3.0f) return shader_source::scale3x_fragment_shader;
                if (scale == 4.0f) return shader_source::scale4x_fragment_shader;
                return nullptr;

            case algorithm::ScaleSFX:
                if (scale == 2.0f) return shader_source::scale2x_sfx_fragment_shader;
                if (scale == 3.0f) return shader_source::scale3x_sfx_fragment_shader;
                return nullptr;

            case algorithm::AAScale:
                if (scale == 2.0f) return shader_source::aascale2x_fragment_shader;
                if (scale == 4.0f) return shader_source::aascale4x_fragment_shader;
                return nullptr;
        SCALER_DISABLE_WARNING_POP

            default:
                // Use the shader specified in the algorithm map
                auto [vertex, fragment] = gpu_algorithm_traits::get_shader_sources(algo);
                return fragment;
        }
    }

} // namespace scaler::gpu