#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

namespace scaler {
    // Generic EPX scaler using CRTP - works with any image implementation
    template<typename InputImage, typename OutputImage>
    auto scaleEpx(const InputImage& src, int scale_factor = 2)
        -> OutputImage {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 3x3 neighborhood
        using PixelType = decltype(src.get_pixel(0, 0));
        SlidingWindow3x3<PixelType> window(src.width());
        window.initialize(src, 0);

        for (int y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            // Get row references once per scanline for better performance
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (int x = 0; x < src.width(); x++) {
                // Acquire neighbour pixel values from cached row references
                const size_t xp = static_cast<size_t>(x + pad);
                auto A = topRow[xp];      // top
                auto B = midRow[xp + 1];   // right
                auto C = midRow[xp - 1];   // left
                auto D = botRow[xp];       // bottom
                auto original_pixel = midRow[xp];  // center

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
                if (threeOrMoreIdentical(A, B, C, D)) {
                    one = two = three = four = original_pixel;
                }

                int dst_x = scale_factor * x;
                int dst_y = scale_factor * y;
                result.set_pixel(dst_x, dst_y, one);
                result.set_pixel(dst_x + 1, dst_y, two);
                result.set_pixel(dst_x, dst_y + 1, three);
                result.set_pixel(dst_x + 1, dst_y + 1, four);
            }
        }
        return result;
    }

    // Generic AdvMAME scaler using CRTP
    template<typename InputImage, typename OutputImage>
    auto scaleAdvMame(const InputImage& src, int scale_factor = 2)
        -> OutputImage {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 3x3 neighborhood
        using PixelType = decltype(src.get_pixel(0, 0));
        SlidingWindow3x3<PixelType> window(src.width());
        window.initialize(src, 0);

        for (int y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            // Get row references once per scanline for better performance
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (int x = 0; x < src.width(); x++) {
                // Acquire neighbour pixel values from cached row references
                const size_t xp = static_cast<size_t>(x + pad);
                auto A = topRow[xp];      // top
                auto B = midRow[xp + 1];   // right
                auto C = midRow[xp - 1];   // left
                auto D = botRow[xp];       // bottom
                auto original_pixel = midRow[xp];  // center

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

                int dst_x = scale_factor * x;
                int dst_y = scale_factor * y;
                result.set_pixel(dst_x, dst_y, one);
                result.set_pixel(dst_x + 1, dst_y, two);
                result.set_pixel(dst_x, dst_y + 1, three);
                result.set_pixel(dst_x + 1, dst_y + 1, four);
            }
        }
        return result;
    }
}