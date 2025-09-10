#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>

// Generic Eagle scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleEagle(const InputImage& src, int scale_factor = 2) 
    -> OutputImage {
    OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

    for (int y = 0; y < src.height(); y++) {
        for (int x = 0; x < src.width(); x++) {
            // Acquire neighbour pixel values
            auto top_left = src.safeAccess(x - 1, y - 1, NEAREST);
            auto top = src.safeAccess(x, y - 1, NEAREST);
            auto top_right = src.safeAccess(x + 1, y - 1, NEAREST);
            
            auto left = src.safeAccess(x - 1, y, NEAREST);
            auto right = src.safeAccess(x + 1, y, NEAREST);
            
            auto bottom_left = src.safeAccess(x - 1, y + 1, NEAREST);
            auto bottom = src.safeAccess(x, y + 1, NEAREST);
            auto bottom_right = src.safeAccess(x + 1, y + 1, NEAREST);

            // Initial expanded pixel value assignments
            auto original_pixel = src.safeAccess(x, y);
            auto one = original_pixel;
            auto two = original_pixel;
            auto three = original_pixel;
            auto four = original_pixel;

            // Interpolation rules
            if (top_left == top && top == top_right) { one = top_left; }
            if (top == top_right && top_right == right) { two = top_right; }
            if (left == bottom_left && bottom_left == bottom) { three = bottom_left; }
            if (right == bottom_right && bottom_right == bottom) { four = bottom_right; }

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