#pragma once

#include <scaler/image.hh>
#include <scaler/scaler_common.hh>

template<typename T>
SDL_Surface* scaleEagle(const input_image<T>& src) {
    auto result = src.get_output(src.width() * 2, src.height() * 2);

    for (int y = 0; y < src.height(); y++) {
        for (int x = 0; x < src.width(); x++) {
            // Acquire neighbour pixel values
            T top_left, top, top_right;
            top_left = src.safeAccess(x - 1, y - 1, NEAREST),
            top = src.safeAccess(x, y - 1, NEAREST),
            top_right = src.safeAccess(x + 1, y - 1, NEAREST);
            T left, right;
            left = src.safeAccess(x - 1, y, NEAREST), right = src.safeAccess(x + 1, y, NEAREST);
            T bottom_left, bottom, bottom_right;
            bottom_left = src.safeAccess(x - 1, y + 1, NEAREST),
            bottom = src.safeAccess(x, y + 1, NEAREST),
            bottom_right = src.safeAccess(x + 1, y + 1, NEAREST);

            // Initial expanded pixel value assignments
            T original_pixel = src.safeAccess(x, y);
            T one, two, three, four;
            one = two = three = four = original_pixel;

            // Interpolation rules
            if (top_left == top && top == top_right) { one = top_left; }
            if (top == top_right && top_right == right) { two = top_right; }
            if (left == bottom_left && bottom_left == bottom) { three = bottom_left; }
            if (right == bottom_right && bottom_right == bottom) { four = bottom_right; }

            int dst_x = 2 * x;
            int dst_y = 2 * y;
            result.set_pixel(dst_x, dst_y,     one);
            result.set_pixel(dst_x + 1, dst_y, two);
            result.set_pixel(dst_x, dst_y + 1, three);
            result.set_pixel(dst_x + 1, dst_y + 1, four);
        }
    }
    return result.get();
}


