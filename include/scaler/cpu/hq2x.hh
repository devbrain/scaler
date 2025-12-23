#pragma once

#include <scaler/image_base.hh>
#include <scaler/cpu/scaler_common.hh>
#include <scaler/vec3.hh>
#include <scaler/cpu/buffer_policy.hh>
#include <array>
#include <scaler/cpu/sliding_window_buffer.hh>

namespace scaler {
    namespace detail {
        // Inline function replacements for macros - better type safety and debugging
        constexpr bool pattern_match(uint32_t diffs, uint32_t mask, uint32_t desired_result) noexcept {
            return (diffs & mask) == desired_result;
        }

        constexpr uint8_t Y_THRESHOLD = 0x30;
        constexpr uint8_t U_THRESHOLD = 0x07;
        constexpr uint8_t V_THRESHOLD = 0x06;

        template<typename T>
        static bool yuv_difference(const T& lhs, const T& rhs) noexcept {
            auto lhs_yuv = rgb_to_yuv(lhs);
            auto rhs_yuv = rgb_to_yuv(rhs);
            // Use unsigned arithmetic to avoid abs() function call
            auto dy = (lhs_yuv.x > rhs_yuv.x) ? (lhs_yuv.x - rhs_yuv.x) : (rhs_yuv.x - lhs_yuv.x);
            auto du = (lhs_yuv.y > rhs_yuv.y) ? (lhs_yuv.y - rhs_yuv.y) : (rhs_yuv.y - lhs_yuv.y);
            auto dv = (lhs_yuv.z > rhs_yuv.z) ? (lhs_yuv.z - rhs_yuv.z) : (rhs_yuv.z - lhs_yuv.z);
            return (dy > Y_THRESHOLD || du > U_THRESHOLD || dv > V_THRESHOLD);
        }

        template<typename T>
        static T interpolate2_pixels(T c1, int32_t w1, T c2, int32_t w2, int32_t s) noexcept {
            // Early exit for identical colors
            if (c1 == c2) { return c1; }

            // Fast path for (w[4], 5, w[x], 3, 3) - most common case (16 instances)
            // This gives us (5*c1 + 3*c2) >> 3
            if (w1 == 5 && w2 == 3 && s == 3) {
                return T{
                    static_cast<typename T::value_type>((c1.x * 5 + c2.x * 3) >> 3),
                    static_cast<typename T::value_type>((c1.y * 5 + c2.y * 3) >> 3),
                    static_cast<typename T::value_type>((c1.z * 5 + c2.z * 3) >> 3)
                };
            }

            // Fast path for (w[4], 7, w[x], 1, 3) - second most common (10 instances)
            // This gives us (7*c1 + c2) >> 3
            if (w1 == 7 && w2 == 1 && s == 3) {
                return T{
                    static_cast<typename T::value_type>((c1.x * 7 + c2.x) >> 3),
                    static_cast<typename T::value_type>((c1.y * 7 + c2.y) >> 3),
                    static_cast<typename T::value_type>((c1.z * 7 + c2.z) >> 3)
                };
            }

            // Fast path for (w[4], 3, w[x], 1, 2) - (4 instances)
            // This gives us (3*c1 + c2) >> 2
            if (w1 == 3 && w2 == 1 && s == 2) {
                return T{
                    static_cast<typename T::value_type>((c1.x * 3 + c2.x) >> 2),
                    static_cast<typename T::value_type>((c1.y * 3 + c2.y) >> 2),
                    static_cast<typename T::value_type>((c1.z * 3 + c2.z) >> 2)
                };
            }

            // Fast path for (w[x], 1, w[y], 1, 1) - (3 instances)
            // This gives us (c1 + c2) >> 1 (simple average)
            if (w1 == 1 && w2 == 1 && s == 1) {
                return T{
                    static_cast<typename T::value_type>((c1.x + c2.x) >> 1),
                    static_cast<typename T::value_type>((c1.y + c2.y) >> 1),
                    static_cast<typename T::value_type>((c1.z + c2.z) >> 1)
                };
            }

            // General case for any other combination
            return T{
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.x) * w1 + static_cast <int32_t>(c2.x) * w2) >> s),
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.y) * w1 + static_cast <int32_t>(c2.y) * w2) >> s),
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.z) * w1 + static_cast <int32_t>(c2.z) * w2) >> s)
            };
        }

        template<typename T>
        static T interpolate_3pixels(T c1, int32_t w1, T c2, int32_t w2, T c3, int32_t w3, int32_t s) noexcept {
            // Fast path for the common case: (c1, 2, c2, 1, c3, 1, 2)
            // This accounts for 100% of calls in HQ2x
            if (w1 == 2 && w2 == 1 && w3 == 1 && s == 2) {
                // (2*c1 + c2 + c3) >> 2 = (c1 + c1 + c2 + c3) >> 2
                return T{
                    static_cast<typename T::value_type>((c1.x + c1.x + c2.x + c3.x) >> 2),
                    static_cast<typename T::value_type>((c1.y + c1.y + c2.y + c3.y) >> 2),
                    static_cast<typename T::value_type>((c1.z + c1.z + c2.z + c3.z) >> 2)
                };
            }

            return T{
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.x) * w1 + static_cast <int32_t>(c2.x) * w2 +
                                            static_cast <int32_t>(c3.x) * w3) >> s),
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.y) * w1 + static_cast <int32_t>(c2.y) * w2 +
                                            static_cast <int32_t>(c3.y) * w3) >> s),
                static_cast<typename T::value_type>((static_cast <int32_t>(c1.z) * w1 + static_cast <int32_t>(c2.z) * w2 +
                                            static_cast <int32_t>(c3.z) * w3) >> s)
            };
        }

        template<typename T>
        static uint8_t compute_differences(const std::array <T, 9>& w) {
            const bool w1_diff = yuv_difference(w[4], w[1]);
            const bool w2_diff = yuv_difference(w[4], w[2]);
            const bool w3_diff = yuv_difference(w[4], w[3]);
            const bool w4_diff = yuv_difference(w[4], w[5]);
            const bool w5_diff = yuv_difference(w[4], w[6]);
            const bool w6_diff = yuv_difference(w[4], w[7]);
            const bool w7_diff = yuv_difference(w[4], w[8]);
            const bool w8_diff = yuv_difference(w[4], w[0]);

            return static_cast<uint8_t>(
                   (w1_diff << 0) | (w2_diff << 1) | (w3_diff << 2) | (w4_diff << 3) |
                   (w5_diff << 4) | (w6_diff << 5) | (w7_diff << 6) | (w8_diff << 7));
        }

        // Generic HQ2x scaler with buffer policy
        template<typename InputImage, typename OutputImage, typename BufferPolicy>
        void scale_hq2x_with_policy(const InputImage& src, OutputImage& result, size_t scale_factor = 2) {

            using PixelType = decltype(src.get_pixel(0, 0));
            row_buffer_manager <PixelType, BufferPolicy> buffers(src.width());

            // Initialize first rows
            buffers.initialize_rows(src, 0);

            for (size_t y = 0; y < src.height(); y++) {
                // Load next row
                buffers.load_next_row(src, static_cast <int>(y));

                for (size_t x = 0; x < src.width(); x++) {
                    // Get 3x3 neighborhood
                    std::array <PixelType, 9> w;
                    buffers.get_neighborhood(static_cast <int>(x), w.data());

                    // Compute conditions corresponding to each set of 2x2 interpolation rules
                    uint8_t diffs = compute_differences(w);
                    const bool cond00 = (pattern_match(diffs, 0xbf, 0x37) || pattern_match(diffs, 0xdb, 0x13)) &&
                                        yuv_difference(w[1], w[5]);
                    const bool cond01 = (pattern_match(diffs, 0xdb, 0x49) || pattern_match(diffs, 0xef, 0x6d)) &&
                                        yuv_difference(w[7], w[3]);
                    const bool cond02 = (pattern_match(diffs, 0x6f, 0x2a) || pattern_match(diffs, 0x5b, 0x0a) ||
                                         pattern_match(diffs, 0xbf, 0x3a) ||
                                         pattern_match(diffs, 0xdf, 0x5a) || pattern_match(diffs, 0x9f, 0x8a) ||
                                         pattern_match(diffs, 0xcf, 0x8a) ||
                                         pattern_match(diffs, 0xef, 0x4e) || pattern_match(diffs, 0x3f, 0x0e) ||
                                         pattern_match(diffs, 0xfb, 0x5a) ||
                                         pattern_match(diffs, 0xbb, 0x8a) || pattern_match(diffs, 0x7f, 0x5a) ||
                                         pattern_match(diffs, 0xaf, 0x8a) ||
                                         pattern_match(diffs, 0xeb, 0x8a)) && yuv_difference(w[3], w[1]);
                    const bool cond03 = pattern_match(diffs, 0xdb, 0x49) || pattern_match(diffs, 0xef, 0x6d);
                    const bool cond04 = pattern_match(diffs, 0xbf, 0x37) || pattern_match(diffs, 0xdb, 0x13);
                    const bool cond05 = pattern_match(diffs, 0x1b, 0x03) || pattern_match(diffs, 0x4f, 0x43) ||
                                        pattern_match(diffs, 0x8b, 0x83) || pattern_match(diffs, 0x6b, 0x43);
                    const bool cond06 = pattern_match(diffs, 0x4b, 0x09) || pattern_match(diffs, 0x8b, 0x89) ||
                                        pattern_match(diffs, 0x1f, 0x19) || pattern_match(diffs, 0x3b, 0x19);
                    const bool cond07 = pattern_match(diffs, 0x0b, 0x08) || pattern_match(diffs, 0xf9, 0x68) ||
                                        pattern_match(diffs, 0xf3, 0x62) ||
                                        pattern_match(diffs, 0x6d, 0x6c) || pattern_match(diffs, 0x67, 0x66) ||
                                        pattern_match(diffs, 0x3d, 0x3c) ||
                                        pattern_match(diffs, 0x37, 0x36) || pattern_match(diffs, 0xf9, 0xf8) ||
                                        pattern_match(diffs, 0xdd, 0xdc) ||
                                        pattern_match(diffs, 0xf3, 0xf2) || pattern_match(diffs, 0xd7, 0xd6) ||
                                        pattern_match(diffs, 0xdd, 0x1c) ||
                                        pattern_match(diffs, 0xd7, 0x16) || pattern_match(diffs, 0x0b, 0x02);
                    const bool cond08 = (pattern_match(diffs, 0x0f, 0x0b) || pattern_match(diffs, 0x2b, 0x0b) ||
                                         pattern_match(diffs, 0xfe, 0x4a) ||
                                         pattern_match(diffs, 0xfe, 0x1a)) && yuv_difference(w[3], w[1]);
                    const bool cond09 = pattern_match(diffs, 0x2f, 0x2f);
                    const bool cond10 = pattern_match(diffs, 0x0a, 0x00);
                    const bool cond11 = pattern_match(diffs, 0x0b, 0x09);
                    const bool cond12 = pattern_match(diffs, 0x7e, 0x2a) || pattern_match(diffs, 0xef, 0xab);
                    const bool cond13 = pattern_match(diffs, 0xbf, 0x8f) || pattern_match(diffs, 0x7e, 0x0e);
                    const bool cond14 = pattern_match(diffs, 0x4f, 0x4b) || pattern_match(diffs, 0x9f, 0x1b) ||
                                        pattern_match(diffs, 0x2f, 0x0b) ||
                                        pattern_match(diffs, 0xbe, 0x0a) || pattern_match(diffs, 0xee, 0x0a) ||
                                        pattern_match(diffs, 0x7e, 0x0a) ||
                                        pattern_match(diffs, 0xeb, 0x4b) || pattern_match(diffs, 0x3b, 0x1b);
                    const bool cond15 = pattern_match(diffs, 0x0b, 0x03);

                    // Assign destination pixel values corresponding to the various conditions
                    auto dst00 = w[4];
                    auto dst01 = w[4];
                    auto dst10 = w[4];
                    auto dst11 = w[4];

                    // Top-left pixel
                    if (cond00)
                        dst00 = interpolate2_pixels(w[4], 5, w[3], 3, 3);
                    else if (cond01)
                        dst00 = interpolate2_pixels(w[4], 5, w[1], 3, 3);
                    else if ((pattern_match(diffs, 0x0b, 0x0b) || pattern_match(diffs, 0xfe, 0x4a) ||
                              pattern_match(diffs, 0xfe, 0x1a)) && yuv_difference(w[3], w[1]))
                        dst00 = w[4];
                    else if (cond02)
                        dst00 = interpolate2_pixels(w[4], 5, w[0], 3, 3);
                    else if (cond03)
                        dst00 = interpolate2_pixels(w[4], 3, w[3], 1, 2);
                    else if (cond04)
                        dst00 = interpolate2_pixels(w[4], 3, w[1], 1, 2);
                    else if (cond05)
                        dst00 = interpolate2_pixels(w[4], 5, w[3], 3, 3);
                    else if (cond06)
                        dst00 = interpolate2_pixels(w[4], 5, w[1], 3, 3);
                    else if (pattern_match(diffs, 0x0f, 0x0b) || pattern_match(diffs, 0x5e, 0x0a) ||
                             pattern_match(diffs, 0x2b, 0x0b) || pattern_match(diffs, 0xbe, 0x0a) ||
                             pattern_match(diffs, 0x7a, 0x0a) || pattern_match(diffs, 0xee, 0x0a))
                        dst00 = interpolate2_pixels(w[1], 1, w[3], 1, 1);
                    else if (cond07)
                        dst00 = interpolate2_pixels(w[4], 5, w[0], 3, 3);
                    else
                        dst00 = interpolate_3pixels(w[4], 2, w[1], 1, w[3], 1, 2);

                    // Top-right pixel
                    if (cond00)
                        dst01 = interpolate2_pixels(w[4], 7, w[5], 1, 3);
                    else if (cond01)
                        dst01 = interpolate2_pixels(w[4], 5, w[2], 3, 3);
                    else if (cond08)
                        dst01 = w[4];
                    else if (cond02)
                        dst01 = interpolate2_pixels(w[4], 7, w[1], 1, 3);
                    else if (cond03)
                        dst01 = interpolate2_pixels(w[4], 5, w[2], 3, 3);
                    else if (cond04)
                        dst01 = interpolate2_pixels(w[4], 3, w[1], 1, 2);
                    else if (cond05)
                        dst01 = interpolate2_pixels(w[4], 7, w[1], 1, 3);
                    else if (cond06)
                        dst01 = interpolate2_pixels(w[4], 5, w[1], 3, 3);
                    else if (cond09)
                        dst01 = w[4];
                    else if (cond10)
                        dst01 = interpolate2_pixels(w[1], 1, w[5], 1, 1);
                    else if (cond11)
                        dst01 = interpolate2_pixels(w[4], 5, w[2], 3, 3);
                    else if (cond07)
                        dst01 = interpolate2_pixels(w[4], 7, w[5], 1, 3);
                    else
                        dst01 = interpolate_3pixels(w[4], 2, w[1], 1, w[5], 1, 2);

                    // Bottom-left pixel
                    if (cond00)
                        dst10 = interpolate2_pixels(w[4], 5, w[3], 3, 3);
                    else if (cond01)
                        dst10 = interpolate2_pixels(w[4], 7, w[7], 1, 3);
                    else if (cond08)
                        dst10 = interpolate2_pixels(w[4], 7, w[3], 1, 3);
                    else if (cond02)
                        dst10 = w[4];
                    else if (cond03)
                        dst10 = interpolate2_pixels(w[4], 3, w[3], 1, 2);
                    else if (cond04)
                        dst10 = interpolate2_pixels(w[4], 5, w[6], 3, 3);
                    else if (cond05)
                        dst10 = interpolate2_pixels(w[4], 5, w[3], 3, 3);
                    else if (cond06)
                        dst10 = interpolate2_pixels(w[4], 7, w[3], 1, 3);
                    else if (cond12)
                        dst10 = interpolate2_pixels(w[3], 1, w[7], 1, 1);
                    else if (cond13)
                        dst10 = interpolate2_pixels(w[4], 5, w[6], 3, 3);
                    else if (cond14)
                        dst10 = w[4];
                    else if (cond07)
                        dst10 = interpolate2_pixels(w[4], 7, w[7], 1, 3);
                    else
                        dst10 = interpolate_3pixels(w[4], 2, w[3], 1, w[7], 1, 2);

                    // Bottom-right pixel
                    if (cond00)
                        dst11 = interpolate2_pixels(w[4], 7, w[5], 1, 3);
                    else if (cond01)
                        dst11 = interpolate2_pixels(w[4], 5, w[8], 3, 3);
                    else if (cond08)
                        dst11 = interpolate2_pixels(w[4], 7, w[5], 1, 3);
                    else if (cond02)
                        dst11 = interpolate2_pixels(w[4], 7, w[7], 1, 3);
                    else if (cond03)
                        dst11 = interpolate2_pixels(w[4], 5, w[8], 3, 3);
                    else if (cond04)
                        dst11 = interpolate2_pixels(w[4], 3, w[7], 1, 2);
                    else if (cond05)
                        dst11 = interpolate2_pixels(w[4], 7, w[7], 1, 3);
                    else if (cond06)
                        dst11 = interpolate2_pixels(w[4], 7, w[5], 1, 3);
                    else if (cond15)
                        dst11 = w[4];
                    else if (pattern_match(diffs, 0xf7, 0xf6) || pattern_match(diffs, 0x37, 0x36) ||
                             pattern_match(diffs, 0x37, 0x16) || pattern_match(diffs, 0xdb, 0xd2) ||
                             pattern_match(diffs, 0xf3, 0xf2) || pattern_match(diffs, 0xf9, 0xf8) ||
                             pattern_match(diffs, 0x6d, 0x6c) || pattern_match(diffs, 0xf3, 0xf0))
                        dst11 = interpolate2_pixels(w[4], 5, w[8], 3, 3);
                    else if (pattern_match(diffs, 0xf7, 0xf7) || pattern_match(diffs, 0xff, 0xff) ||
                             pattern_match(diffs, 0xfc, 0xf4) || pattern_match(diffs, 0xfb, 0xf3) ||
                             pattern_match(diffs, 0xfb, 0xfb) || pattern_match(diffs, 0xfd, 0xfd) ||
                             pattern_match(diffs, 0xfe, 0xf6) || pattern_match(diffs, 0xf7, 0xf3) ||
                             pattern_match(diffs, 0xfd, 0xf5))
                        dst11 = interpolate2_pixels(w[5], 1, w[7], 1, 1);
                    else if (cond07)
                        dst11 = interpolate2_pixels(w[4], 5, w[8], 3, 3);
                    else
                        dst11 = interpolate_3pixels(w[4], 2, w[5], 1, w[7], 1, 2);

                    size_t dst_x = scale_factor * x;
                    size_t dst_y = scale_factor * y;
                    result.set_pixel(dst_x, dst_y, dst00);
                    result.set_pixel(dst_x + 1, dst_y, dst01);
                    result.set_pixel(dst_x, dst_y + 1, dst10);
                    result.set_pixel(dst_x + 1, dst_y + 1, dst11);
                }

                // Rotate rows for next iteration
                buffers.rotate_rows();
            }
        }
    }

    // Main HQ2x scaler - writes directly to output
    template<typename InputImage, typename OutputImage>
    void scale_hq2x(const InputImage& src, OutputImage& result, size_t scale_factor = 2) {
        using PixelType = decltype(src.get_pixel(0, 0));

        // Use fixed buffer for images up to 4096 pixels wide
        if (src.width() <= 4096) {
            using Policy = fixed_buffer_policy <PixelType, 4096>;
            detail::scale_hq2x_with_policy <InputImage, OutputImage, Policy>(src, result, scale_factor);
        } else {
            // Fall back to dynamic buffer for very wide images
            using Policy = dynamic_buffer_policy <PixelType>;
            detail::scale_hq2x_with_policy <InputImage, OutputImage, Policy>(src, result, scale_factor);
        }
    }

    // Legacy wrapper that creates output (for backward compatibility)
    template<typename InputImage, typename OutputImage>
    OutputImage scale_hq2x(const InputImage& src, size_t scale_factor = 2) {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
        scale_hq2x(src, result, scale_factor);
        return result;
    }

    // Fast version explicitly uses fixed buffers - writes directly to output
    template<typename InputImage, typename OutputImage>
    void scale_hq2x_fast(const InputImage& src, OutputImage& result, size_t scale_factor = 2) {
        using PixelType = decltype(src.get_pixel(0, 0));
        using Policy = fixed_buffer_policy <PixelType, 4096>;
        detail::scale_hq2x_with_policy <InputImage, OutputImage, Policy>(src, result, scale_factor);
    }

    // Legacy wrapper for fast version
    template<typename InputImage, typename OutputImage>
    OutputImage scale_hq2x_fast(const InputImage& src, size_t scale_factor = 2) {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
        scale_hq2x_fast(src, result, scale_factor);
        return result;
    }
}
