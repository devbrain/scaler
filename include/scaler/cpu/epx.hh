#pragma once

#include <scaler/image_base.hh>
#include <scaler/cpu/scaler_common.hh>
#include <scaler/cpu/sliding_window_buffer.hh>

namespace scaler {
    namespace detail {
        // Implementation template for EPX with any window type
        template<typename InputImage, typename OutputImage, typename WindowType>
        OutputImage scale_epx_impl(const InputImage& src, WindowType& window, size_t scale_factor = 2) {
            OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
            window.initialize(src, 0);

            for (size_t y = 0; y < src.height(); y++) {
                // Advance sliding window for next row
                if (y > 0) {
                    window.advance(src);
                }

                // Get row references once per scanline for better performance
                const auto& topRow = window.get_row(-1);
                const auto& midRow = window.get_row(0);
                const auto& botRow = window.get_row(1);
                const int pad = window.get_padding();

                for (size_t x = 0; x < src.width(); x++) {
                    // Acquire neighbour pixel values from cached row references
                    const int xp = static_cast <int>(x) + pad;
                    auto A = topRow[static_cast <size_t>(xp)]; // top
                    auto B = midRow[static_cast <size_t>(xp + 1)]; // right
                    auto C = midRow[static_cast <size_t>(xp - 1)]; // left
                    auto D = botRow[static_cast <size_t>(xp)]; // bottom
                    auto original_pixel = midRow[static_cast <size_t>(xp)]; // center

                    // Initial expanded pixel value assignments
                    auto one = original_pixel;
                    auto two = original_pixel;
                    auto three = original_pixel;
                    auto four = original_pixel;

                    // Interpolation conditions
                    if (C == A) { one = A; }
                    if (A == B) { two = B; }
                    if (D == C) { three = C; }
                    if (B == D) { four = D; }
                    if (three_or_more_identical(A, B, C, D)) {
                        one = two = three = four = original_pixel;
                    }

                    size_t dst_x = scale_factor * x;
                    size_t dst_y = scale_factor * y;
                    result.set_pixel(dst_x, dst_y, one);
                    result.set_pixel(dst_x + 1, dst_y, two);
                    result.set_pixel(dst_x, dst_y + 1, three);
                    result.set_pixel(dst_x + 1, dst_y + 1, four);
                }
            }
            return result;
        }
    }

    // Generic EPX scaler using CRTP - automatically uses fast path for small images
    template<typename InputImage, typename OutputImage>
    OutputImage scale_epx(const InputImage& src, size_t scale_factor = 2) {
        using PixelType = decltype(src.get_pixel(0, 0));

        // Use fast sliding window for images <= 4096 pixels wide
        if (src.width() <= 4096) {
            fast_sliding_window_3x3 <PixelType, 4096> window(src.width());
            return detail::scale_epx_impl <InputImage, OutputImage>(src, window, scale_factor);
        } else {
            // Fall back to dynamic sliding window for very wide images
            sliding_window_3x3 <PixelType> window(src.width());
            return detail::scale_epx_impl <InputImage, OutputImage>(src, window, scale_factor);
        }
    }

    namespace detail {
        // Implementation template for AdvMAME with any window type
        template<typename InputImage, typename OutputImage, typename WindowType>
        OutputImage scale_adv_mame_impl(const InputImage& src, WindowType& window, size_t scale_factor = 2) {
            OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
            window.initialize(src, 0);

            for (size_t y = 0; y < src.height(); y++) {
                // Advance sliding window for next row
                if (y > 0) {
                    window.advance(src);
                }

                // Get row references once per scanline for better performance
                const auto& topRow = window.get_row(-1);
                const auto& midRow = window.get_row(0);
                const auto& botRow = window.get_row(1);
                const int pad = window.get_padding();

                for (size_t x = 0; x < src.width(); x++) {
                    // Acquire neighbour pixel values from cached row references
                    const int xp = static_cast <int>(x) + pad;
                    auto A = topRow[static_cast <size_t>(xp)]; // top
                    auto B = midRow[static_cast <size_t>(xp + 1)]; // right
                    auto C = midRow[static_cast <size_t>(xp - 1)]; // left
                    auto D = botRow[static_cast <size_t>(xp)]; // bottom
                    auto original_pixel = midRow[static_cast <size_t>(xp)]; // center

                    // Initial expanded pixel value assignments
                    auto one = original_pixel;
                    auto two = original_pixel;
                    auto three = original_pixel;
                    auto four = original_pixel;

                    // Interpolation conditions
                    if (C == A && C != D && A != B) { one = A; }
                    if (A == B && A != C && B != D) { two = B; }
                    if (D == C && D != B && C != A) { three = C; }
                    if (B == D && B != A && D != C) { four = D; }

                    size_t dst_x = scale_factor * x;
                    size_t dst_y = scale_factor * y;
                    result.set_pixel(dst_x, dst_y, one);
                    result.set_pixel(dst_x + 1, dst_y, two);
                    result.set_pixel(dst_x, dst_y + 1, three);
                    result.set_pixel(dst_x + 1, dst_y + 1, four);
                }
            }
            return result;
        }
    }

    // Generic AdvMAME scaler using CRTP - automatically uses fast path for small images
    template<typename InputImage, typename OutputImage>
    OutputImage scale_adv_mame(const InputImage& src, size_t scale_factor = 2) {
        using PixelType = decltype(src.get_pixel(0, 0));

        // Use fast sliding window for images <= 4096 pixels wide
        if (src.width() <= 4096) {
            fast_sliding_window_3x3 <PixelType, 4096> window(src.width());
            return detail::scale_adv_mame_impl <InputImage, OutputImage>(src, window, scale_factor);
        } else {
            // Fall back to dynamic sliding window for very wide images
            sliding_window_3x3 <PixelType> window(src.width());
            return detail::scale_adv_mame_impl <InputImage, OutputImage>(src, window, scale_factor);
        }
    }
}
