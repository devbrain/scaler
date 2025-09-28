#pragma once

#include <vector>
#include <cstddef>

namespace scaler {

    // Scale-independent algorithm names (shared between CPU and GPU)
    enum class algorithm {
        // Simple scalers
        Nearest,        // Nearest neighbor (any scale)
        Bilinear,       // Bilinear interpolation (any scale)
        Trilinear,      // Trilinear interpolation with mipmapping (any scale)

        // Classic pixel art scalers
        EPX,            // Eric's Pixel Expansion (2x only)
        Eagle,          // Eagle algorithm (2x only)
        Scale,          // AdvMAME Scale (2x, 3x, 4x)
        ScaleSFX,       // Sp00kyFox variant (2x, 3x)
        Super2xSaI,     // 2xSaI interpolation (2x only)

        // High quality family
        HQ,             // High Quality (2x, 3x, 4x)

        // Anti-aliased scaling
        AAScale,        // Anti-aliased Scale (2x, 4x)

        // Advanced algorithms
        xBR,            // Hyllian's xBR (2x, 3x, 4x)
        xBRZ,           // Zenju's xBRZ variant (2x-6x)

        // Resolution independent
        OmniScale,      // Any scale factor

        // Aliases for backward compatibility
        AdvMAME = Scale,
    };

    // Output dimensions for preallocating buffers
    struct output_dimensions {
        size_t width;
        size_t height;
    };

    /**
     * Calculate output dimensions for a given algorithm and scale factor
     * This is used by both CPU and GPU scalers to preallocate buffers
     */
    inline output_dimensions calculate_output_size(
        size_t input_width,
        size_t input_height,
        algorithm algo,
        float scale_factor) {

        // Most algorithms simply multiply by scale factor
        size_t out_width = static_cast<size_t>(input_width * scale_factor);
        size_t out_height = static_cast<size_t>(input_height * scale_factor);

        // Some algorithms may have specific requirements
        switch(algo) {
            case algorithm::EPX:
                // EPX is always 2x
                out_width = input_width * 2;
                out_height = input_height * 2;
                break;

            case algorithm::Eagle:
                // Eagle is always 2x
                out_width = input_width * 2;
                out_height = input_height * 2;
                break;

            case algorithm::Super2xSaI:
                // Super2xSaI is always 2x
                out_width = input_width * 2;
                out_height = input_height * 2;
                break;

            default:
                // Use the calculated dimensions
                break;
        }

        return {out_width, out_height};
    }

} // namespace scaler