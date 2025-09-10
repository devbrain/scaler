#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>

// Generic EPX scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleEpx(const InputImage& src, int scale_factor = 2) 
    -> OutputImage {
    OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

    for (int y = 0; y < src.height(); y++) {
        for (int x = 0; x < src.width(); x++) {
            // Acquire neighbour pixel values
            auto A = src.safeAccess(x, y - 1);
            auto B = src.safeAccess(x + 1, y);
            auto C = src.safeAccess(x - 1, y);
            auto D = src.safeAccess(x, y + 1);

            // Initial expanded pixel value assignments
            auto original_pixel = src.safeAccess(x, y);
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

    for (int y = 0; y < src.height(); y++) {
        for (int x = 0; x < src.width(); x++) {
            // Acquire neighbour pixel values
            auto A = src.safeAccess(x, y - 1);
            auto B = src.safeAccess(x + 1, y);
            auto C = src.safeAccess(x - 1, y);
            auto D = src.safeAccess(x, y + 1);

            // Initial expanded pixel value assignments
            auto original_pixel = src.safeAccess(x, y);
            auto one = original_pixel;
            auto two = original_pixel;
            auto three = original_pixel;
            auto four = original_pixel;

            // Interpolation conditions
            if (C == A && legacyNotEqual(C, D) && legacyNotEqual(A, B)) { one = A; }
            if (A == B && legacyNotEqual(A, C) && legacyNotEqual(B, D)) { two = B; }
            if (D == C && legacyNotEqual(D, B) && legacyNotEqual(C, A)) { three = C; }
            if (B == D && legacyNotEqual(B, A) && legacyNotEqual(D, C)) { four = D; }

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