#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

// Generic Eagle scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleEagle(const InputImage& src, int scale_factor = 2) 
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
        
        for (int x = 0; x < src.width(); x++) {
            // Acquire neighbour pixel values from cache-friendly buffer
            auto top_left = window.getTopLeft(x);
            auto top = window.getTop(x);
            auto top_right = window.getTopRight(x);
            
            auto left = window.getLeft(x);
            auto original_pixel = window.getCenter(x);
            auto right = window.getRight(x);
            
            auto bottom_left = window.getBottomLeft(x);
            auto bottom = window.getBottom(x);
            auto bottom_right = window.getBottomRight(x);

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