#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>

constexpr uint8_t Y_COEFF = 0x30;
constexpr uint8_t U_COEFF = 0x07;
constexpr uint8_t V_COEFF = 0x06;

template<typename T>
static uint32_t dist(T A, T B) {
    auto A_yuv = rgbToYuv(A);
    auto B_yuv = rgbToYuv(B);
    auto diff = uvec3{
        static_cast<unsigned int>(abs(static_cast<int>(A_yuv.x) - static_cast<int>(B_yuv.x))),
        static_cast<unsigned int>(abs(static_cast<int>(A_yuv.y) - static_cast<int>(B_yuv.y))),
        static_cast<unsigned int>(abs(static_cast<int>(A_yuv.z) - static_cast<int>(B_yuv.z)))
    };
    return (diff.x * Y_COEFF) + (diff.y * U_COEFF) + (diff.z * V_COEFF);
}

// Generic XBR scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleXbr(const InputImage& src, int scale_factor = 2) 
    -> OutputImage {
    OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

    for (int y = 0; y < src.height(); y++) {
        for (int x = 0; x < src.width(); x++) {
            // Acquire original pixel grid values (row by row)
            auto A1 = src.safeAccess(x - 1, y - 2, NEAREST);
            auto B1 = src.safeAccess(x, y - 2, NEAREST);
            auto C1 = src.safeAccess(x + 1, y - 2, NEAREST);
            
            auto A0 = src.safeAccess(x - 2, y - 1, NEAREST);
            auto A = src.safeAccess(x - 1, y - 1, NEAREST);
            auto B = src.safeAccess(x, y - 1, NEAREST);
            auto C = src.safeAccess(x + 1, y - 1, NEAREST);
            auto C4 = src.safeAccess(x + 2, y - 1, NEAREST);
            
            auto D0 = src.safeAccess(x - 2, y, NEAREST);
            auto D = src.safeAccess(x - 1, y, NEAREST);
            auto E = src.safeAccess(x, y, NEAREST);
            auto F = src.safeAccess(x + 1, y, NEAREST);
            auto F4 = src.safeAccess(x + 2, y, NEAREST);
            
            auto G0 = src.safeAccess(x - 2, y + 1, NEAREST);
            auto G = src.safeAccess(x - 1, y + 1, NEAREST);
            auto H = src.safeAccess(x, y + 1, NEAREST);
            auto I = src.safeAccess(x + 1, y + 1, NEAREST);
            auto I4 = src.safeAccess(x + 2, y + 1, NEAREST);
            
            auto G5 = src.safeAccess(x - 1, y + 2, NEAREST);
            auto H5 = src.safeAccess(x, y + 2, NEAREST);
            auto I5 = src.safeAccess(x + 1, y + 2, NEAREST);

            // Detect diagonal edges in the four possible directions
            uint32_t bot_right_perpendicular_dist = dist(E, C) + dist(E, G) + dist(I, F4) + dist(I, H5) + 4 * dist(H, F);
            uint32_t bot_right_parallel_dist = dist(H, D) + dist(H, I5) + dist(F, I4) + dist(F, B) + 4 * dist(E, I);
            bool edr_bot_right = bot_right_perpendicular_dist < bot_right_parallel_dist;
            
            uint32_t bot_left_perpendicular_dist = dist(A, E) + dist(E, I) + dist(D0, G) + dist(G, H5) + 4 * dist(D, H);
            uint32_t bot_left_parallel_dist = dist(B, D) + dist(F, H) + dist(D, G0) + dist(H, G5) + 4 * dist(E, G);
            bool edr_bot_left = bot_left_perpendicular_dist < bot_left_parallel_dist;
            
            uint32_t top_left_perpendicular_dist = dist(G, E) + dist(E, C) + dist(D0, A) + dist(A, B1) + 4 * dist(D, B);
            uint32_t top_left_parallel_dist = dist(H, D) + dist(D, A0) + dist(F, B) + dist(B, A1) + 4 * dist(E, A);
            bool edr_top_left = top_left_perpendicular_dist < top_left_parallel_dist;
            
            uint32_t top_right_perpendicular_dist = dist(A, E) + dist(E, I) + dist(B1, C) + dist(C, F4) + 4 * dist(B, F);
            uint32_t top_right_parallel_dist = dist(D, B) + dist(B, C1) + dist(H, F) + dist(F, C4) + 4 * dist(E, C);
            bool edr_top_right = top_right_perpendicular_dist < top_right_parallel_dist;

            // Pixel weighting constants
            constexpr int LEFT_UP_WEIGHT = 5;
            constexpr int EDGE_ANTI_ALIAS_WEIGHT = 2;
            constexpr int RIGHT_DOWN_WEIGHT = 5;
            constexpr int CENTER_WEIGHT = 6;

            // Determine edge weight deltas
            int left_weight = edr_top_left && !edr_bot_left ? LEFT_UP_WEIGHT : 0;
            int top_weight = edr_top_right && !edr_top_left ? LEFT_UP_WEIGHT : 0;
            int right_weight = edr_bot_right && !edr_top_right ? RIGHT_DOWN_WEIGHT : 0;
            int bottom_weight = edr_bot_left && !edr_bot_right ? RIGHT_DOWN_WEIGHT : 0;

            // Blend with anti-aliasing based on detected edges
            auto top_left_pixel = E;
            auto top_right_pixel = E;
            auto bot_left_pixel = E;
            auto bot_right_pixel = E;

            if (top_weight > 0) {
                if (dist(B, D) > dist(B, F)) top_right_pixel = B;
            }
            if (bottom_weight > 0) {
                if (dist(H, D) > dist(H, F)) bot_left_pixel = H;
            }
            if (left_weight > 0) {
                if (dist(D, B) > dist(D, H)) top_left_pixel = D;
            }
            if (right_weight > 0) {
                if (dist(F, B) > dist(F, H)) bot_right_pixel = F;
            }

            // Anti-aliasing for diagonal edges
            if (edr_top_left) {
                auto interp_weight = (dist(E, C) <= dist(E, G)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && A != E && B != E && C != E && D != E) {
                    top_left_pixel = mix(top_left_pixel, A, 0.25f);
                }
            }
            if (edr_top_right) {
                auto interp_weight = (dist(E, G) <= dist(E, C)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && B != E && C != E && A != E && F != E) {
                    top_right_pixel = mix(top_right_pixel, C, 0.25f);
                }
            }
            if (edr_bot_left) {
                auto interp_weight = (dist(E, C) <= dist(E, I)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && D != E && G != E && H != E && A != E) {
                    bot_left_pixel = mix(bot_left_pixel, G, 0.25f);
                }
            }
            if (edr_bot_right) {
                auto interp_weight = (dist(E, A) <= dist(E, I)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && F != E && H != E && I != E && C != E) {
                    bot_right_pixel = mix(bot_right_pixel, I, 0.25f);
                }
            }

            int dst_x = scale_factor * x;
            int dst_y = scale_factor * y;
            result.set_pixel(dst_x, dst_y, top_left_pixel);
            result.set_pixel(dst_x + 1, dst_y, top_right_pixel);
            result.set_pixel(dst_x, dst_y + 1, bot_left_pixel);
            result.set_pixel(dst_x + 1, dst_y + 1, bot_right_pixel);
        }
    }
    return result;
}