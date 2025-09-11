#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

namespace scaler {
    // Generic Eagle scaler using CRTP - works with any image implementation
    template<typename InputImage, typename OutputImage>
    auto scaleEagle(const InputImage& src, size_t scale_factor = 2)
        -> OutputImage {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 3x3 neighborhood
        using PixelType = decltype(src.get_pixel(0, 0));
        SlidingWindow3x3<PixelType> window(src.width());
        window.initialize(src, 0);

        for (size_t y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            // Get row references once per scanline for better performance
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (size_t x = 0; x < src.width(); x++) {
                // Acquire neighbour pixel values from cached row references
                const int xp = static_cast<int>(x) + pad;
                auto top_left = topRow[static_cast<size_t>(xp - 1)];
                auto top = topRow[static_cast<size_t>(xp)];
                auto top_right = topRow[static_cast<size_t>(xp + 1)];

                auto left = midRow[static_cast<size_t>(xp - 1)];
                auto original_pixel = midRow[static_cast<size_t>(xp)];
                auto right = midRow[static_cast<size_t>(xp + 1)];

                auto bottom_left = botRow[static_cast<size_t>(xp - 1)];
                auto bottom = botRow[static_cast<size_t>(xp)];
                auto bottom_right = botRow[static_cast<size_t>(xp + 1)];

                // Initial expanded pixel value assignments
                auto one = original_pixel;
                auto two = original_pixel;
                auto three = original_pixel;
                auto four = original_pixel;

                // Interpolation rules
                if (top_left == top && top == top_right) { one = top_left; }
                if (top == top_right && top_right == right) { two = top_right; }
                if (left == bottom_left && bottom_left == bottom) { three = bottom_left; }
                if (right == bottom_right && bottom_right == bottom) { four = bottom_right; }

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