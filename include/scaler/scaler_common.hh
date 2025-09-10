#pragma once
#include <cstdint>
#include <scaler/vec3.hh>
/**
 * Compute if three or more of the given values are equal/identical
 * 
 * @param a First value
 * @param b Second value
 * @param c Third value
 * @param d Fourth value
 * 
 * @return True if three or more are equal, false otherwise
*/
template<typename T>
inline bool threeOrMoreIdentical(T a, T b, T c, T d) noexcept {
    // Count how many pairs are equal
    // If 3+ values are identical, we'll have at least 3 equal pairs
    int equal_pairs = (a == b) + (a == c) + (a == d) + (b == c) + (b == d) + (c == d);
    return equal_pairs >= 3;
}

/**
 * Interpolate a point in the rectangle defined by the given values
 * 
 * @param top_left Value of the top left corner
 * @param top_right Value of the top right corner
 * @param bottom_left Value of the bottom left corner
 * @param bottom_right Value of the bottom right corner
 * @param right_proportion How far the point is proportionally from the left to the right. Range: [0..1]
 * @param bottom_proportion How far the point is proportionally from the top to the bottom. Range: [0..1]
 * 
 * @return The value produced by bilinear interpolation
*/
template<typename T>
inline T bilinearInterpolation(T top_left, T top_right, T bottom_left, T bottom_right,
                               float right_proportion, float bottom_proportion) noexcept {
    T top_interp    = mix(top_left, top_right, right_proportion);
    T bottom_interp = mix(bottom_left, bottom_right, right_proportion);
    return mix(top_interp, bottom_interp, bottom_proportion);
}

inline static uvec3 rgbToYuv(const uvec3& val) noexcept {
    // Use integer arithmetic with fixed-point representation (16-bit precision)
    // Coefficients multiplied by 65536 for precision
    constexpr int Y_R = 19595;  // 0.299 * 65536
    constexpr int Y_G = 38470;  // 0.587 * 65536
    constexpr int Y_B = 7471;   // 0.114 * 65536
    
    constexpr int U_R = -11076; // -0.169 * 65536
    constexpr int U_G = -21692; // -0.331 * 65536
    constexpr int U_B = 32768;  // 0.5 * 65536
    
    constexpr int V_R = 32768;  // 0.5 * 65536
    constexpr int V_G = -27460; // -0.419 * 65536
    constexpr int V_B = -5308;  // -0.081 * 65536
    
    int r = val.x;
    int g = val.y;
    int b = val.z;
    
    unsigned int y = (Y_R * r + Y_G * g + Y_B * b) >> 16;
    unsigned int u = ((U_R * r + U_G * g + U_B * b) >> 16) + 128;
    unsigned int v = ((V_R * r + V_G * g + V_B * b) >> 16) + 128;
    
    return {y, u, v};
}

[[maybe_unused]] inline static uint32_t rgbToYuv(uint32_t val) noexcept {
    // Use same integer coefficients as above
    constexpr int Y_R = 19595, Y_G = 38470, Y_B = 7471;
    constexpr int U_R = -11076, U_G = -21692, U_B = 32768;
    constexpr int V_R = 32768, V_G = -27460, V_B = -5308;
    
    uint32_t r = (val & 0xFF0000) >> 16;
    uint32_t g = (val & 0x00FF00) >> 8;
    uint32_t b = val & 0x0000FF;

    uint32_t y = (Y_R * r + Y_G * g + Y_B * b) >> 16;
    uint32_t u = ((U_R * r + U_G * g + U_B * b) >> 16) + 128;
    uint32_t v = ((V_R * r + V_G * g + V_B * b) >> 16) + 128;
    return (y << 16) | (u << 8) | v;
}
