#pragma once

#include <scaler/compiler_compat.hh>
#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>
#include <cmath>
#include <algorithm>

namespace scaler {
    namespace omniscale_detail {
        // Use fixed-point arithmetic for color space conversion (16.16 fixed point)
        constexpr int FP_SHIFT = 16;
        constexpr int FP_ONE = 1 << FP_SHIFT;
        constexpr int FP_HALF = 1 << (FP_SHIFT - 1);

        // Pre-computed fixed-point coefficients
        constexpr int FP_0_250 = (FP_ONE >> 2); // 0.250
        constexpr int FP_0_125 = (FP_ONE >> 3); // 0.125

        // Fixed-point thresholds (scaled up)
        constexpr int FP_THRESH_X = 1179; // 0.018 * 65536
        constexpr int FP_THRESH_Y = 131; // 0.002 * 65536
        constexpr int FP_THRESH_Z = 328; // 0.005 * 65536

        struct ColorDiff {
            int x, y, z;
        };

        // Optimized color space conversion using integer arithmetic
        SCALER_FORCE_INLINE ColorDiff rgbToHqColorspaceFP(const vec3 <unsigned int>& rgb) {
            // Work directly with 0-255 range, scale up to fixed point
            int r = static_cast<int>(rgb.x);
            int g = static_cast<int>(rgb.y);
            int b = static_cast<int>(rgb.z);

            // Use multiplication instead of left shift for potentially negative values
            return ColorDiff{
                (r + g + b) * 64, // (r + g + b) / 4 in FP
                (r - b) * 64, // (r - b) / 4 in FP
                (-r + 2 * g - b) * 32 // (-r + 2g - b) / 8 in FP
            };
        }

        // Optimized difference check using integer arithmetic
        template<typename PixelType>
        SCALER_HOT SCALER_FORCE_INLINE bool isDifferent(const PixelType& a, const PixelType& b) {
            // Early exit for identical pixels
            if (SCALER_LIKELY(a == b)) return false;

            auto ca = rgbToHqColorspaceFP(a);
            auto cb = rgbToHqColorspaceFP(b);

            // Use absolute difference without std::abs overhead
            int dx = ca.x - cb.x;
            if (dx < 0) dx = -dx;
            if (dx > FP_THRESH_X) return true;

            int dy = ca.y - cb.y;
            if (dy < 0) dy = -dy;
            if (dy > FP_THRESH_Y) return true;

            int dz = ca.z - cb.z;
            if (dz < 0) dz = -dz;
            return dz > FP_THRESH_Z;
        }

#define P(m, r) ((pattern & (m)) == (r))

        // Pre-computed interpolation weights for common cases
        constexpr float WEIGHT_QUARTER = 0.25f;
        constexpr float WEIGHT_HALF = 0.5f;
        constexpr float WEIGHT_THREE_QUARTERS = 0.75f;

        // Pattern cache to avoid rebuilding for each quarter
        template<typename PixelType>
        struct PatternCache {
            // Store all 4 patterns for 2x scaling (normal, flipX, flipY, flipXY)
            // Or all patterns needed for 3x scaling
            unsigned int patterns[4];

            // Build all patterns at once from the 3x3 neighborhood
            SCALER_FORCE_INLINE void buildAllPatterns(const PixelType& w0, const PixelType& w1, const PixelType& w2,
                                                      const PixelType& w3, const PixelType& w4, const PixelType& w5,
                                                      const PixelType& w6, const PixelType& w7, const PixelType& w8) {
                // Original pattern (no flip)
                patterns[0] = 0;
                patterns[0] |= isDifferent(w0, w4) ? 0x01u : 0u;
                patterns[0] |= isDifferent(w1, w4) ? 0x02u : 0u;
                patterns[0] |= isDifferent(w2, w4) ? 0x04u : 0u;
                patterns[0] |= isDifferent(w3, w4) ? 0x08u : 0u;
                patterns[0] |= isDifferent(w5, w4) ? 0x10u : 0u;
                patterns[0] |= isDifferent(w6, w4) ? 0x20u : 0u;
                patterns[0] |= isDifferent(w7, w4) ? 0x40u : 0u;
                patterns[0] |= isDifferent(w8, w4) ? 0x80u : 0u;

                // FlipX pattern (w0↔w2, w3↔w5, w6↔w8)
                patterns[1] = 0;
                patterns[1] |= isDifferent(w2, w4) ? 0x01u : 0u; // w0 position
                patterns[1] |= isDifferent(w1, w4) ? 0x02u : 0u; // w1 stays
                patterns[1] |= isDifferent(w0, w4) ? 0x04u : 0u; // w2 position
                patterns[1] |= isDifferent(w5, w4) ? 0x08u : 0u; // w3 position
                patterns[1] |= isDifferent(w3, w4) ? 0x10u : 0u; // w5 position
                patterns[1] |= isDifferent(w8, w4) ? 0x20u : 0u; // w6 position
                patterns[1] |= isDifferent(w7, w4) ? 0x40u : 0u; // w7 stays
                patterns[1] |= isDifferent(w6, w4) ? 0x80u : 0u; // w8 position

                // FlipY pattern (w0↔w6, w1↔w7, w2↔w8)
                patterns[2] = 0;
                patterns[2] |= isDifferent(w6, w4) ? 0x01u : 0u; // w0 position
                patterns[2] |= isDifferent(w7, w4) ? 0x02u : 0u; // w1 position
                patterns[2] |= isDifferent(w8, w4) ? 0x04u : 0u; // w2 position
                patterns[2] |= isDifferent(w3, w4) ? 0x08u : 0u; // w3 stays
                patterns[2] |= isDifferent(w5, w4) ? 0x10u : 0u; // w5 stays
                patterns[2] |= isDifferent(w0, w4) ? 0x20u : 0u; // w6 position
                patterns[2] |= isDifferent(w1, w4) ? 0x40u : 0u; // w7 position
                patterns[2] |= isDifferent(w2, w4) ? 0x80u : 0u; // w8 position

                // FlipXY pattern (both flips)
                patterns[3] = 0;
                patterns[3] |= isDifferent(w8, w4) ? 0x01u : 0u; // w0 position
                patterns[3] |= isDifferent(w7, w4) ? 0x02u : 0u; // w1 position
                patterns[3] |= isDifferent(w6, w4) ? 0x04u : 0u; // w2 position
                patterns[3] |= isDifferent(w5, w4) ? 0x08u : 0u; // w3 position
                patterns[3] |= isDifferent(w3, w4) ? 0x10u : 0u; // w5 position
                patterns[3] |= isDifferent(w2, w4) ? 0x20u : 0u; // w6 position
                patterns[3] |= isDifferent(w1, w4) ? 0x40u : 0u; // w7 position
                patterns[3] |= isDifferent(w0, w4) ? 0x80u : 0u; // w8 position
            }
        };

        template<typename PixelType>
        struct OmniScaleCore {
            PixelType w0, w1, w2, w3, w4, w5, w6, w7, w8;
            unsigned int pattern;

            SCALER_FORCE_INLINE void loadNeighborhood(const PixelType& n0, const PixelType& n1, const PixelType& n2,
                                                      const PixelType& n3, const PixelType& n4, const PixelType& n5,
                                                      const PixelType& n6, const PixelType& n7, const PixelType& n8,
                                                      bool flipX, bool flipY) {
                if (!flipX && !flipY) {
                    w0 = n0;
                    w1 = n1;
                    w2 = n2;
                    w3 = n3;
                    w4 = n4;
                    w5 = n5;
                    w6 = n6;
                    w7 = n7;
                    w8 = n8;
                } else if (flipX && !flipY) {
                    w0 = n2;
                    w1 = n1;
                    w2 = n0;
                    w3 = n5;
                    w4 = n4;
                    w5 = n3;
                    w6 = n8;
                    w7 = n7;
                    w8 = n6;
                } else if (!flipX && flipY) {
                    w0 = n6;
                    w1 = n7;
                    w2 = n8;
                    w3 = n3;
                    w4 = n4;
                    w5 = n5;
                    w6 = n0;
                    w7 = n1;
                    w8 = n2;
                } else {
                    // flipX && flipY
                    w0 = n8;
                    w1 = n7;
                    w2 = n6;
                    w3 = n5;
                    w4 = n4;
                    w5 = n3;
                    w6 = n2;
                    w7 = n1;
                    w8 = n0;
                }
            }

            SCALER_FORCE_INLINE void setPattern(unsigned int p) {
                pattern = p;
            }

            SCALER_HOT SCALER_FLATTEN
            inline PixelType interpolateCorner(float px, float py) const noexcept {
                // Pre-compute all weights we might need
                const float weight_x = WEIGHT_HALF - px;
                const float weight_y = WEIGHT_HALF - py;

                // Use a switch for the most common patterns (better branch prediction)
                // Patterns are checked in order of frequency based on typical images
                switch (pattern) {
                    // Most common solid/edge cases (all same or all different)
                    case 0x00:
                    case 0xFF:
                        return w4;

                    // Simple horizontal edges
                    case 0x1B:
                    case 0x03:
                        return mix(w4, w3, weight_x);

                    // Simple vertical edges
                    case 0x4B:
                    case 0x09:
                        return mix(w4, w1, weight_y);

                    default:
                        break; // Handle other patterns below
                }

                // Use bit manipulation to reduce comparisons
                const unsigned int p_low = pattern & 0xFF;
                const unsigned int p_high = pattern & 0xF0;

                // Simple edge patterns - combine similar checks
                if ((p_low == 0x03 && p_high == 0x10) || // 0x1B,0x03
                    (p_low == 0x43 && p_high == 0x40) || // 0x4F,0x43
                    (p_low == 0x83 && p_high == 0x80) || // 0x8B,0x83
                    (p_low == 0x43 && p_high == 0x60)) {
                    // 0x6B,0x43
                    return mix(w4, w3, weight_x);
                }

                if ((p_low == 0x09 && p_high == 0x40) || // 0x4B,0x09
                    (p_low == 0x89 && p_high == 0x80) || // 0x8B,0x89
                    (p_low == 0x19 && p_high == 0x10) || // 0x1F,0x19
                    (p_low == 0x19 && p_high == 0x30)) {
                    // 0x3B,0x19
                    return mix(w4, w1, weight_y);
                }

                // Diagonal patterns - use lookup for isDifferent checks
                if (p_low == 0x37 || p_low == 0x13) {
                    if (p_high == 0xB0 || p_high == 0xD0) {
                        // 0xBF,0x37 or 0xDB,0x13
                        if (isDifferent(w1, w5)) {
                            return mix(w4, w3, weight_x);
                        }
                        return mix(w3, w4, px + WEIGHT_HALF);
                    }
                }

                if (p_low == 0x49 || p_low == 0x6D) {
                    if (p_high == 0xD0 || p_high == 0xE0) {
                        // 0xDB,0x49 or 0xEF,0x6D
                        if (isDifferent(w7, w3)) {
                            return mix(w4, w1, weight_y);
                        }
                        return mix(w1, w4, py + WEIGHT_HALF);
                    }
                }

                // Early return patterns
                if ((pattern == 0x0B || pattern == 0xFE4A || pattern == 0xFE1A) && isDifferent(w3, w1)) {
                    return w4;
                }

                // Pre-calculate diagonal weight for patterns that use it
                const float weight_diag = py - px + WEIGHT_HALF;

                // Complex multi-check pattern - use bitmask for efficiency
                constexpr unsigned int complex_patterns[] = {
                    0x6F2A, 0x5B0A, 0xBF3A, 0xDF5A, 0x9F8A, 0xCF8A,
                    0xEF4E, 0x3F0E, 0xFB5A, 0xBB8A, 0x7F5A, 0xAF8A, 0xEB8A
                };

                for (unsigned int cp : complex_patterns) {
                    if (pattern == cp && isDifferent(w3, w1)) {
                        // Pre-compute inner mix to avoid repeated calculation
                        auto inner = mix(w4, w0, weight_x);
                        return mix(w4, inner, weight_y);
                    }
                }

                // Blend patterns with pre-computed weights
                if (pattern == 0x0B08) {
                    // Pre-compute constants
                    constexpr float W_04 = 0.4f;
                    auto blend1 = mix(mix(w0, w1, W_04), w4, WEIGHT_HALF);
                    auto blend2 = mix(w4, w1, WEIGHT_HALF);
                    float px2 = px + px; // Avoid multiplication
                    float py2 = py + py;
                    return mix(mix(blend1, blend2, px2), w4, py2);
                }

                if (pattern == 0x0B02) {
                    constexpr float W_04 = 0.4f;
                    auto blend1 = mix(mix(w0, w3, W_04), w4, WEIGHT_HALF);
                    auto blend2 = mix(w4, w3, WEIGHT_HALF);
                    float py2 = py + py;
                    float px2 = px + px;
                    return mix(mix(blend1, blend2, py2), w4, px2);
                }

                // Repeated pattern group - combine similar patterns
                constexpr unsigned int diag_patterns[] = {
                    0x2F2F, 0xBF8F, 0x7E0E, 0x7E2A, 0xEFAB
                };

                for (unsigned int dp : diag_patterns) {
                    if (pattern == dp) {
                        if (isDifferent(w0, w1) || isDifferent(w0, w3)) {
                            return mix(w1, w3, weight_diag);
                        } else {
                            constexpr float W_04 = 0.4f;
                            auto blend = mix(mix(w1, w0, W_04), w3, WEIGHT_HALF);
                            float py2 = py + py;
                            float px2 = px + px;
                            return mix(mix(blend, w3, py2), w1, px2);
                        }
                    }
                }

                // Corner pattern
                constexpr unsigned int corner_patterns[] = {
                    0xFB6A, 0x6F6E, 0x3F3E, 0xFBFA, 0xDFDE, 0xDF1E
                };

                for (unsigned int cp : corner_patterns) {
                    if (pattern == cp) {
                        float corner_weight = (1.0f - px - py) * WEIGHT_HALF;
                        return mix(w4, w0, corner_weight);
                    }
                }

                // Another diagonal group
                constexpr unsigned int diag2_patterns[] = {
                    0x4F4B, 0x9F1B, 0x2F0B, 0xBE0A, 0xEE0A, 0x7E0A, 0xEB4B, 0x3B1B
                };

                for (unsigned int dp : diag2_patterns) {
                    if (pattern == dp) {
                        if (isDifferent(w0, w1) || isDifferent(w0, w3)) {
                            return mix(w1, w3, weight_diag);
                        } else {
                            constexpr float W_04 = 0.4f;
                            auto blend = mix(mix(w1, w0, W_04), w3, WEIGHT_HALF);
                            float py2 = py + py;
                            float px2 = px + px;
                            return mix(mix(blend, w3, py2), w1, px2);
                        }
                    }
                }

                // Final patterns
                if (pattern == 0x0B01) {
                    auto avg = mix(w1, w3, WEIGHT_HALF);
                    return mix(mix(w4, w3, weight_x), mix(w1, avg, weight_x), weight_y);
                }

                if (pattern == 0x0B00) {
                    return mix(mix(w4, w3, weight_x), mix(w1, w0, weight_x), weight_y);
                }

                return w4;
            }
        };
    } // namespace omniscale_detail

    // OmniScale 2x implementation with pattern caching
    template<typename InputImage, typename OutputImage>
    SCALER_HOT
    auto scaleOmniScale2x(const InputImage& src, [[maybe_unused]] int scale_factor = 2)
        -> OutputImage {
        OutputImage result(src.width() * 2, src.height() * 2, src);

        using PixelType = decltype(src.get_pixel(0, 0));
        using namespace omniscale_detail;

        SlidingWindow3x3 <PixelType> window(src.width());
        window.initialize(src, 0);

        // Pre-compute position values
        constexpr float POS_QUARTER = 0.25f;

        for (int y = 0; y < src.height(); y++) {
            if (y > 0) {
                window.advance(src);
            }

            // Cache row pointers for better memory access
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (int x = 0; x < src.width(); x++) {
                const size_t xp = static_cast<size_t>(x + pad);

                // Load 3x3 neighborhood once
                PixelType n0 = topRow[xp - 1], n1 = topRow[xp], n2 = topRow[xp + 1];
                PixelType n3 = midRow[xp - 1], n4 = midRow[xp], n5 = midRow[xp + 1];
                PixelType n6 = botRow[xp - 1], n7 = botRow[xp], n8 = botRow[xp + 1];

                // Build all patterns at once
                PatternCache <PixelType> cache;
                cache.buildAllPatterns(n0, n1, n2, n3, n4, n5, n6, n7, n8);

                OmniScaleCore <PixelType> core;

                // Top-left (pattern[0] - no flip)
                core.loadNeighborhood(n0, n1, n2, n3, n4, n5, n6, n7, n8, false, false);
                core.setPattern(cache.patterns[0]);
                auto e0 = core.interpolateCorner(POS_QUARTER, POS_QUARTER);

                // Top-right (pattern[1] - flipX)
                core.loadNeighborhood(n0, n1, n2, n3, n4, n5, n6, n7, n8, true, false);
                core.setPattern(cache.patterns[1]);
                auto e1 = core.interpolateCorner(POS_QUARTER, POS_QUARTER);

                // Bottom-left (pattern[2] - flipY)
                core.loadNeighborhood(n0, n1, n2, n3, n4, n5, n6, n7, n8, false, true);
                core.setPattern(cache.patterns[2]);
                auto e2 = core.interpolateCorner(POS_QUARTER, POS_QUARTER);

                // Bottom-right (pattern[3] - flipXY)
                core.loadNeighborhood(n0, n1, n2, n3, n4, n5, n6, n7, n8, true, true);
                core.setPattern(cache.patterns[3]);
                auto e3 = core.interpolateCorner(POS_QUARTER, POS_QUARTER);

                // Write 2x2 output block
                int dst_x = x * 2;
                int dst_y = y * 2;
                result.set_pixel(dst_x, dst_y, e0);
                result.set_pixel(dst_x + 1, dst_y, e1);
                result.set_pixel(dst_x, dst_y + 1, e2);
                result.set_pixel(dst_x + 1, dst_y + 1, e3);
            }
        }

        return result;
    }

    // OmniScale 3x implementation with pattern caching
    template<typename InputImage, typename OutputImage>
    SCALER_HOT
    auto scaleOmniScale3x(const InputImage& src, [[maybe_unused]] int scale_factor = 3)
        -> OutputImage {
        OutputImage result(src.width() * 3, src.height() * 3, src);

        using PixelType = decltype(src.get_pixel(0, 0));
        using namespace omniscale_detail;

        SlidingWindow3x3 <PixelType> window(src.width());
        window.initialize(src, 0);

        // Pre-compute position values for 3x3 grid
        constexpr float positions[9][2] = {
            {1.0f / 6.0f, 1.0f / 6.0f}, {0.5f, 1.0f / 6.0f}, {5.0f / 6.0f, 1.0f / 6.0f},
            {1.0f / 6.0f, 0.5f}, {0.5f, 0.5f}, {5.0f / 6.0f, 0.5f},
            {1.0f / 6.0f, 5.0f / 6.0f}, {0.5f, 5.0f / 6.0f}, {5.0f / 6.0f, 5.0f / 6.0f}
        };

        // Determine which pattern to use for each position
        [[maybe_unused]] constexpr int pattern_index[9] = {
            0, 0, 1, // Top row: no-flip, no-flip, flipX
            0, 0, 1, // Mid row: no-flip, no-flip, flipX
            2, 2, 3 // Bot row: flipY, flipY, flipXY
        };

        for (int y = 0; y < src.height(); y++) {
            if (y > 0) {
                window.advance(src);
            }

            // Cache row pointers
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (int x = 0; x < src.width(); x++) {
                const size_t xp = static_cast<size_t>(x + pad);

                // Load 3x3 neighborhood once
                PixelType n0 = topRow[xp - 1], n1 = topRow[xp], n2 = topRow[xp + 1];
                PixelType n3 = midRow[xp - 1], n4 = midRow[xp], n5 = midRow[xp + 1];
                PixelType n6 = botRow[xp - 1], n7 = botRow[xp], n8 = botRow[xp + 1];

                // Build all patterns at once
                PatternCache <PixelType> cache;
                cache.buildAllPatterns(n0, n1, n2, n3, n4, n5, n6, n7, n8);

                PixelType pixels[9];
                OmniScaleCore <PixelType> core;

                // Process each output pixel using cached patterns
                for (int i = 0; i < 9; i++) {
                    float px = positions[i][0];
                    float py = positions[i][1];

                    // Determine quarter and adjust position
                    bool flipX = px > 0.5f;
                    bool flipY = py > 0.5f;

                    if (flipX) px = 1.0f - px;
                    if (flipY) py = 1.0f - py;

                    // Load with appropriate flipping
                    core.loadNeighborhood(n0, n1, n2, n3, n4, n5, n6, n7, n8, flipX, flipY);

                    // Use pre-computed pattern
                    int pidx = (flipY ? 2 : 0) + (flipX ? 1 : 0);
                    core.setPattern(cache.patterns[pidx]);

                    pixels[i] = core.interpolateCorner(px, py);
                }

                // Write 3x3 output block
                int dst_x = x * 3;
                int dst_y = y * 3;

                for (int dy = 0; dy < 3; dy++) {
                    for (int dx = 0; dx < 3; dx++) {
                        result.set_pixel(dst_x + dx, dst_y + dy, pixels[dy * 3 + dx]);
                    }
                }
            }
        }

        return result;
    }

#undef P
}
