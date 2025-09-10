#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/vec3.hh>
#include <array>
#include <scaler/sliding_window_buffer.hh>

#define P(mask, des_res) ((diffs & (mask)) == (des_res))
#define WDIFF(c1, c2) yuvDifference(c1, c2)

constexpr uint8_t Y_THRESHOLD = 0x30;
constexpr uint8_t U_THRESHOLD = 0x07;
constexpr uint8_t V_THRESHOLD = 0x06;

template<typename T>
inline static bool yuvDifference(const T& lhs, const T& rhs) noexcept {
    auto lhs_yuv = rgbToYuv(lhs);
    auto rhs_yuv = rgbToYuv(rhs);
    // Use unsigned arithmetic to avoid abs() function call
    auto dy = (lhs_yuv.x > rhs_yuv.x) ? (lhs_yuv.x - rhs_yuv.x) : (rhs_yuv.x - lhs_yuv.x);
    auto du = (lhs_yuv.y > rhs_yuv.y) ? (lhs_yuv.y - rhs_yuv.y) : (rhs_yuv.y - lhs_yuv.y);
    auto dv = (lhs_yuv.z > rhs_yuv.z) ? (lhs_yuv.z - rhs_yuv.z) : (rhs_yuv.z - lhs_yuv.z);
    return (dy > Y_THRESHOLD || du > U_THRESHOLD || dv > V_THRESHOLD);
}

template<typename T>
inline static T interpolate2Pixels(T c1, int32_t w1, T c2, int32_t w2, int32_t s) noexcept {
    // Early exit for identical colors
    if (c1 == c2) { return c1; }
    
    // Fast path for (w[4], 5, w[x], 3, 3) - most common case (16 instances)
    // This gives us (5*c1 + 3*c2) >> 3
    if (w1 == 5 && w2 == 3 && s == 3) {
        return T{
            static_cast<unsigned int>((c1.x * 5 + c2.x * 3) >> 3),
            static_cast<unsigned int>((c1.y * 5 + c2.y * 3) >> 3),
            static_cast<unsigned int>((c1.z * 5 + c2.z * 3) >> 3)
        };
    }
    
    // Fast path for (w[4], 7, w[x], 1, 3) - second most common (10 instances)
    // This gives us (7*c1 + c2) >> 3
    if (w1 == 7 && w2 == 1 && s == 3) {
        return T{
            static_cast<unsigned int>((c1.x * 7 + c2.x) >> 3),
            static_cast<unsigned int>((c1.y * 7 + c2.y) >> 3),
            static_cast<unsigned int>((c1.z * 7 + c2.z) >> 3)
        };
    }
    
    // Fast path for (w[4], 3, w[x], 1, 2) - (4 instances)
    // This gives us (3*c1 + c2) >> 2
    if (w1 == 3 && w2 == 1 && s == 2) {
        return T{
            static_cast<unsigned int>((c1.x * 3 + c2.x) >> 2),
            static_cast<unsigned int>((c1.y * 3 + c2.y) >> 2),
            static_cast<unsigned int>((c1.z * 3 + c2.z) >> 2)
        };
    }
    
    // Fast path for (w[x], 1, w[y], 1, 1) - (3 instances)
    // This gives us (c1 + c2) >> 1 (simple average)
    if (w1 == 1 && w2 == 1 && s == 1) {
        return T{
            static_cast<unsigned int>((c1.x + c2.x) >> 1),
            static_cast<unsigned int>((c1.y + c2.y) >> 1),
            static_cast<unsigned int>((c1.z + c2.z) >> 1)
        };
    }
    
    // General case for any other combination
    return T{
        static_cast<unsigned int>(((c1.x * w1) + (c2.x * w2)) >> s),
        static_cast<unsigned int>(((c1.y * w1) + (c2.y * w2)) >> s),
        static_cast<unsigned int>(((c1.z * w1) + (c2.z * w2)) >> s)
    };
}

template<typename T>
inline static T interpolate3Pixels(T c1, int32_t w1, T c2, int32_t w2, T c3, int32_t w3, int32_t s) noexcept {
    // Fast path for the common case: (c1, 2, c2, 1, c3, 1, 2)
    // This accounts for 100% of calls in HQ2x
    if (w1 == 2 && w2 == 1 && w3 == 1 && s == 2) {
        // (2*c1 + c2 + c3) >> 2 = (c1 + c1 + c2 + c3) >> 2
        return T{
            static_cast<unsigned int>((c1.x + c1.x + c2.x + c3.x) >> 2),
            static_cast<unsigned int>((c1.y + c1.y + c2.y + c3.y) >> 2),
            static_cast<unsigned int>((c1.z + c1.z + c2.z + c3.z) >> 2)
        };
    }
    
    return T{
        static_cast<unsigned int>(((c1.x * w1) + (c2.x * w2) + (c3.x * w3)) >> s),
        static_cast<unsigned int>(((c1.y * w1) + (c2.y * w2) + (c3.y * w3)) >> s),
        static_cast<unsigned int>(((c1.z * w1) + (c2.z * w2) + (c3.z * w3)) >> s)
    };
}

template<typename T>
static uint8_t compute_differences(const std::array<T, 9>& w) {
    const bool w1_diff = yuvDifference(w[4], w[1]);
    const bool w2_diff = yuvDifference(w[4], w[2]);
    const bool w3_diff = yuvDifference(w[4], w[3]);
    const bool w4_diff = yuvDifference(w[4], w[5]);
    const bool w5_diff = yuvDifference(w[4], w[6]);
    const bool w6_diff = yuvDifference(w[4], w[7]);
    const bool w7_diff = yuvDifference(w[4], w[8]);
    const bool w8_diff = yuvDifference(w[4], w[0]);

    return (w1_diff << 0) | (w2_diff << 1) | (w3_diff << 2) | (w4_diff << 3) |
           (w5_diff << 4) | (w6_diff << 5) | (w7_diff << 6) | (w8_diff << 7);
}

// Generic HQ2x scaler using CRTP - works with any image implementation
template<typename InputImage, typename OutputImage>
auto scaleHq2x(const InputImage& src, int scale_factor = 2) 
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
            std::array<PixelType, 9> w;
            const int xp = x + pad;
            w[0] = topRow[xp - 1];  // top-left
            w[1] = topRow[xp];      // top
            w[2] = topRow[xp + 1];  // top-right
            w[3] = midRow[xp - 1];  // left
            w[4] = midRow[xp];      // center
            w[5] = midRow[xp + 1];  // right
            w[6] = botRow[xp - 1];  // bottom-left
            w[7] = botRow[xp];      // bottom
            w[8] = botRow[xp + 1];  // bottom-right

            // Compute conditions corresponding to each set of 2x2 interpolation rules
            uint8_t diffs = compute_differences(w);
            const bool cond00 = (P(0xbf, 0x37) || P(0xdb, 0x13)) && WDIFF(w[1], w[5]);
            const bool cond01 = (P(0xdb, 0x49) || P(0xef, 0x6d)) && WDIFF(w[7], w[3]);
            const bool cond02 = (P(0x6f, 0x2a) || P(0x5b, 0x0a) || P(0xbf, 0x3a) ||
                                P(0xdf, 0x5a) || P(0x9f, 0x8a) || P(0xcf, 0x8a) ||
                                P(0xef, 0x4e) || P(0x3f, 0x0e) || P(0xfb, 0x5a) ||
                                P(0xbb, 0x8a) || P(0x7f, 0x5a) || P(0xaf, 0x8a) ||
                                P(0xeb, 0x8a)) && WDIFF(w[3], w[1]);
            const bool cond03 = P(0xdb, 0x49) || P(0xef, 0x6d);
            const bool cond04 = P(0xbf, 0x37) || P(0xdb, 0x13);
            const bool cond05 = P(0x1b, 0x03) || P(0x4f, 0x43) || P(0x8b, 0x83) || P(0x6b, 0x43);
            const bool cond06 = P(0x4b, 0x09) || P(0x8b, 0x89) || P(0x1f, 0x19) || P(0x3b, 0x19);
            const bool cond07 = P(0x0b, 0x08) || P(0xf9, 0x68) || P(0xf3, 0x62) ||
                               P(0x6d, 0x6c) || P(0x67, 0x66) || P(0x3d, 0x3c) ||
                               P(0x37, 0x36) || P(0xf9, 0xf8) || P(0xdd, 0xdc) ||
                               P(0xf3, 0xf2) || P(0xd7, 0xd6) || P(0xdd, 0x1c) ||
                               P(0xd7, 0x16) || P(0x0b, 0x02);
            const bool cond08 = (P(0x0f, 0x0b) || P(0x2b, 0x0b) || P(0xfe, 0x4a) ||
                                P(0xfe, 0x1a)) && WDIFF(w[3], w[1]);
            const bool cond09 = P(0x2f, 0x2f);
            const bool cond10 = P(0x0a, 0x00);
            const bool cond11 = P(0x0b, 0x09);
            const bool cond12 = P(0x7e, 0x2a) || P(0xef, 0xab);
            const bool cond13 = P(0xbf, 0x8f) || P(0x7e, 0x0e);
            const bool cond14 = P(0x4f, 0x4b) || P(0x9f, 0x1b) || P(0x2f, 0x0b) ||
                               P(0xbe, 0x0a) || P(0xee, 0x0a) || P(0x7e, 0x0a) ||
                               P(0xeb, 0x4b) || P(0x3b, 0x1b);
            const bool cond15 = P(0x0b, 0x03);

            // Assign destination pixel values corresponding to the various conditions
            auto dst00 = w[4];
            auto dst01 = w[4];
            auto dst10 = w[4];
            auto dst11 = w[4];

            // Top-left pixel
            if (cond00)
                dst00 = interpolate2Pixels(w[4], 5, w[3], 3, 3);
            else if (cond01)
                dst00 = interpolate2Pixels(w[4], 5, w[1], 3, 3);
            else if ((P(0x0b, 0x0b) || P(0xfe, 0x4a) || P(0xfe, 0x1a)) && WDIFF(w[3], w[1]))
                dst00 = w[4];
            else if (cond02)
                dst00 = interpolate2Pixels(w[4], 5, w[0], 3, 3);
            else if (cond03)
                dst00 = interpolate2Pixels(w[4], 3, w[3], 1, 2);
            else if (cond04)
                dst00 = interpolate2Pixels(w[4], 3, w[1], 1, 2);
            else if (cond05)
                dst00 = interpolate2Pixels(w[4], 5, w[3], 3, 3);
            else if (cond06)
                dst00 = interpolate2Pixels(w[4], 5, w[1], 3, 3);
            else if (P(0x0f, 0x0b) || P(0x5e, 0x0a) || P(0x2b, 0x0b) || P(0xbe, 0x0a) ||
                    P(0x7a, 0x0a) || P(0xee, 0x0a))
                dst00 = interpolate2Pixels(w[1], 1, w[3], 1, 1);
            else if (cond07)
                dst00 = interpolate2Pixels(w[4], 5, w[0], 3, 3);
            else
                dst00 = interpolate3Pixels(w[4], 2, w[1], 1, w[3], 1, 2);

            // Top-right pixel
            if (cond00)
                dst01 = interpolate2Pixels(w[4], 7, w[5], 1, 3);
            else if (cond01)
                dst01 = interpolate2Pixels(w[4], 5, w[2], 3, 3);
            else if (cond08)
                dst01 = w[4];
            else if (cond02)
                dst01 = interpolate2Pixels(w[4], 7, w[1], 1, 3);
            else if (cond03)
                dst01 = interpolate2Pixels(w[4], 5, w[2], 3, 3);
            else if (cond04)
                dst01 = interpolate2Pixels(w[4], 3, w[1], 1, 2);
            else if (cond05)
                dst01 = interpolate2Pixels(w[4], 7, w[1], 1, 3);
            else if (cond06)
                dst01 = interpolate2Pixels(w[4], 5, w[1], 3, 3);
            else if (cond09)
                dst01 = w[4];
            else if (cond10)
                dst01 = interpolate2Pixels(w[1], 1, w[5], 1, 1);
            else if (cond11)
                dst01 = interpolate2Pixels(w[4], 5, w[2], 3, 3);
            else if (cond07)
                dst01 = interpolate2Pixels(w[4], 7, w[5], 1, 3);
            else
                dst01 = interpolate3Pixels(w[4], 2, w[1], 1, w[5], 1, 2);

            // Bottom-left pixel
            if (cond00)
                dst10 = interpolate2Pixels(w[4], 5, w[3], 3, 3);
            else if (cond01)
                dst10 = interpolate2Pixels(w[4], 7, w[7], 1, 3);
            else if (cond08)
                dst10 = interpolate2Pixels(w[4], 7, w[3], 1, 3);
            else if (cond02)
                dst10 = w[4];
            else if (cond03)
                dst10 = interpolate2Pixels(w[4], 3, w[3], 1, 2);
            else if (cond04)
                dst10 = interpolate2Pixels(w[4], 5, w[6], 3, 3);
            else if (cond05)
                dst10 = interpolate2Pixels(w[4], 5, w[3], 3, 3);
            else if (cond06)
                dst10 = interpolate2Pixels(w[4], 7, w[3], 1, 3);
            else if (cond12)
                dst10 = interpolate2Pixels(w[3], 1, w[7], 1, 1);
            else if (cond13)
                dst10 = interpolate2Pixels(w[4], 5, w[6], 3, 3);
            else if (cond14)
                dst10 = w[4];
            else if (cond07)
                dst10 = interpolate2Pixels(w[4], 7, w[7], 1, 3);
            else
                dst10 = interpolate3Pixels(w[4], 2, w[3], 1, w[7], 1, 2);

            // Bottom-right pixel
            if (cond00)
                dst11 = interpolate2Pixels(w[4], 7, w[5], 1, 3);
            else if (cond01)
                dst11 = interpolate2Pixels(w[4], 5, w[8], 3, 3);
            else if (cond08)
                dst11 = interpolate2Pixels(w[4], 7, w[5], 1, 3);
            else if (cond02)
                dst11 = interpolate2Pixels(w[4], 7, w[7], 1, 3);
            else if (cond03)
                dst11 = interpolate2Pixels(w[4], 5, w[8], 3, 3);
            else if (cond04)
                dst11 = interpolate2Pixels(w[4], 3, w[7], 1, 2);
            else if (cond05)
                dst11 = interpolate2Pixels(w[4], 7, w[7], 1, 3);
            else if (cond06)
                dst11 = interpolate2Pixels(w[4], 7, w[5], 1, 3);
            else if (cond15)
                dst11 = w[4];
            else if (P(0xf7, 0xf6) || P(0x37, 0x36) || P(0x37, 0x16) || P(0xdb, 0xd2) ||
                    P(0xf3, 0xf2) || P(0xf9, 0xf8) || P(0x6d, 0x6c) || P(0xf3, 0xf0))
                dst11 = interpolate2Pixels(w[4], 5, w[8], 3, 3);
            else if (P(0xf7, 0xf7) || P(0xff, 0xff) || P(0xfc, 0xf4) || P(0xfb, 0xf3) ||
                    P(0xfb, 0xfb) || P(0xfd, 0xfd) || P(0xfe, 0xf6) || P(0xf7, 0xf3) ||
                    P(0xfd, 0xf5))
                dst11 = interpolate2Pixels(w[5], 1, w[7], 1, 1);
            else if (cond07)
                dst11 = interpolate2Pixels(w[4], 5, w[8], 3, 3);
            else
                dst11 = interpolate3Pixels(w[4], 2, w[5], 1, w[7], 1, 2);

            int dst_x = scale_factor * x;
            int dst_y = scale_factor * y;
            result.set_pixel(dst_x, dst_y, dst00);
            result.set_pixel(dst_x + 1, dst_y, dst01);
            result.set_pixel(dst_x, dst_y + 1, dst10);
            result.set_pixel(dst_x + 1, dst_y + 1, dst11);
        }
    }
    return result;
}

#undef P
#undef WDIFF