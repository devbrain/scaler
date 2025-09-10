#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

constexpr uint8_t Y_COEFF = 0x30;
constexpr uint8_t U_COEFF = 0x07;
constexpr uint8_t V_COEFF = 0x06;

// Helper function to compute absolute difference efficiently
template<typename T>
inline static T abs_diff(T a, T b) noexcept {
    return (a > b) ? (a - b) : (b - a);
}

// Distance function for already-converted YUV values (for caching)
template<typename T>
inline static uint32_t dist_yuv(const T& A_yuv, const T& B_yuv) noexcept {
    // Early exit for identical pixels
    if (A_yuv.x == B_yuv.x && A_yuv.y == B_yuv.y && A_yuv.z == B_yuv.z) return 0;
    
    auto dy = abs_diff(A_yuv.x, B_yuv.x);
    auto du = abs_diff(A_yuv.y, B_yuv.y);
    auto dv = abs_diff(A_yuv.z, B_yuv.z);
    
    return (dy * Y_COEFF) + (du * U_COEFF) + (dv * V_COEFF);
}

template<typename T>
inline static uint32_t dist(T A, T B) noexcept {
    // Early exit for identical pixels
    if (A == B) return 0;
    
    auto A_yuv = rgbToYuv(A);
    auto B_yuv = rgbToYuv(B);
    
    return dist_yuv(A_yuv, B_yuv);
}

// Generic XBR scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleXbr(const InputImage& src, int scale_factor = 2) 
    -> OutputImage {
    OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
    
    // Use cache-friendly sliding window buffer for 5x5 neighborhood
    using PixelType = decltype(src.get_pixel(0, 0));
    SlidingWindow5x5<PixelType> window(src.width());
    window.initialize(src, 0);

    for (int y = 0; y < src.height(); y++) {
        // Advance sliding window for next row
        if (y > 0) {
            window.advance(src);
        }
        
        for (int x = 0; x < src.width(); x++) {
            // Get 5x5 neighborhood from cache-friendly buffer
            PixelType neighborhood[5][5];
            window.getNeighborhood(x, neighborhood);
            
            // Map to original variable names
            // Row y-2
            auto A1 = neighborhood[0][1];
            auto B1 = neighborhood[0][2];
            auto C1 = neighborhood[0][3];
            
            // Row y-1
            auto A0 = neighborhood[1][0];
            auto A = neighborhood[1][1];
            auto B = neighborhood[1][2];
            auto C = neighborhood[1][3];
            auto C4 = neighborhood[1][4];
            
            // Row y
            auto D0 = neighborhood[2][0];
            auto D = neighborhood[2][1];
            auto E = neighborhood[2][2];
            auto F = neighborhood[2][3];
            auto F4 = neighborhood[2][4];
            
            // Row y+1
            auto G0 = neighborhood[3][0];
            auto G = neighborhood[3][1];
            auto H = neighborhood[3][2];
            auto I = neighborhood[3][3];
            auto I4 = neighborhood[3][4];
            
            // Row y+2
            auto G5 = neighborhood[4][1];
            auto H5 = neighborhood[4][2];
            auto I5 = neighborhood[4][3];

            // Pre-convert frequently used pixels to YUV to avoid redundant conversions
            // E is used 8 times, so caching it saves 7 conversions
            auto E_yuv = rgbToYuv(E);
            // These pixels are used 3-4 times each
            auto A_yuv = rgbToYuv(A);
            auto B_yuv = rgbToYuv(B);
            auto C_yuv = rgbToYuv(C);
            auto D_yuv = rgbToYuv(D);
            auto F_yuv = rgbToYuv(F);
            auto G_yuv = rgbToYuv(G);
            auto H_yuv = rgbToYuv(H);
            auto I_yuv = rgbToYuv(I);

            // Detect diagonal edges in the four possible directions
            // Use cached YUV values for frequently used pixels
            uint32_t bot_right_perpendicular_dist = dist_yuv(E_yuv, C_yuv) + dist_yuv(E_yuv, G_yuv) + dist(I, F4) + dist(I, H5) + 4 * dist_yuv(H_yuv, F_yuv);
            uint32_t bot_right_parallel_dist = dist_yuv(H_yuv, D_yuv) + dist(H, I5) + dist(F, I4) + dist_yuv(F_yuv, B_yuv) + 4 * dist_yuv(E_yuv, I_yuv);
            bool edr_bot_right = bot_right_perpendicular_dist < bot_right_parallel_dist;
            
            uint32_t bot_left_perpendicular_dist = dist_yuv(A_yuv, E_yuv) + dist_yuv(E_yuv, I_yuv) + dist(D0, G) + dist(G, H5) + 4 * dist_yuv(D_yuv, H_yuv);
            uint32_t bot_left_parallel_dist = dist_yuv(B_yuv, D_yuv) + dist_yuv(F_yuv, H_yuv) + dist(D, G0) + dist(H, G5) + 4 * dist_yuv(E_yuv, G_yuv);
            bool edr_bot_left = bot_left_perpendicular_dist < bot_left_parallel_dist;
            
            uint32_t top_left_perpendicular_dist = dist_yuv(G_yuv, E_yuv) + dist_yuv(E_yuv, C_yuv) + dist(D0, A) + dist(A, B1) + 4 * dist_yuv(D_yuv, B_yuv);
            uint32_t top_left_parallel_dist = dist_yuv(H_yuv, D_yuv) + dist(D, A0) + dist_yuv(F_yuv, B_yuv) + dist(B, A1) + 4 * dist_yuv(E_yuv, A_yuv);
            bool edr_top_left = top_left_perpendicular_dist < top_left_parallel_dist;
            
            uint32_t top_right_perpendicular_dist = dist_yuv(A_yuv, E_yuv) + dist_yuv(E_yuv, I_yuv) + dist(B1, C) + dist(C, F4) + 4 * dist_yuv(B_yuv, F_yuv);
            uint32_t top_right_parallel_dist = dist_yuv(D_yuv, B_yuv) + dist(B, C1) + dist_yuv(H_yuv, F_yuv) + dist(F, C4) + 4 * dist_yuv(E_yuv, C_yuv);
            bool edr_top_right = top_right_perpendicular_dist < top_right_parallel_dist;

            // Pixel weighting constants
            constexpr int LEFT_UP_WEIGHT = 5;
            constexpr int EDGE_ANTI_ALIAS_WEIGHT = 2;
            constexpr int RIGHT_DOWN_WEIGHT = 5;

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
                if (dist_yuv(B_yuv, D_yuv) > dist_yuv(B_yuv, F_yuv)) top_right_pixel = B;
            }
            if (bottom_weight > 0) {
                if (dist_yuv(H_yuv, D_yuv) > dist_yuv(H_yuv, F_yuv)) bot_left_pixel = H;
            }
            if (left_weight > 0) {
                if (dist_yuv(D_yuv, B_yuv) > dist_yuv(D_yuv, H_yuv)) top_left_pixel = D;
            }
            if (right_weight > 0) {
                if (dist_yuv(F_yuv, B_yuv) > dist_yuv(F_yuv, H_yuv)) bot_right_pixel = F;
            }

            // Anti-aliasing for diagonal edges
            if (edr_top_left) {
                auto interp_weight = (dist_yuv(E_yuv, C_yuv) <= dist_yuv(E_yuv, G_yuv)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && A != E && B != E && C != E && D != E) {
                    top_left_pixel = mix(top_left_pixel, A, 0.25f);
                }
            }
            if (edr_top_right) {
                auto interp_weight = (dist_yuv(E_yuv, G_yuv) <= dist_yuv(E_yuv, C_yuv)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && B != E && C != E && A != E && F != E) {
                    top_right_pixel = mix(top_right_pixel, C, 0.25f);
                }
            }
            if (edr_bot_left) {
                auto interp_weight = (dist_yuv(E_yuv, C_yuv) <= dist_yuv(E_yuv, I_yuv)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
                if (interp_weight > 0 && D != E && G != E && H != E && A != E) {
                    bot_left_pixel = mix(bot_left_pixel, G, 0.25f);
                }
            }
            if (edr_bot_right) {
                auto interp_weight = (dist_yuv(E_yuv, A_yuv) <= dist_yuv(E_yuv, I_yuv)) ? EDGE_ANTI_ALIAS_WEIGHT : 0;
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