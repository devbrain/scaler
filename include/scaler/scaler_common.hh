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
    auto a = static_cast<unsigned int>((0.299f * static_cast <float>(val.x))    + (0.587f * static_cast <float>(val.y))  + (0.114f * static_cast <float>(val.z)));
    auto b = static_cast<unsigned int>(((-0.169f * static_cast <float>(val.x))  + (-0.331 * static_cast <float>(val.y))  + (0.5f * static_cast <float>(val.z))) + 128.0f);
    auto c = static_cast<unsigned int>(((0.5f * static_cast <float>(val.x))     + (-0.419f * static_cast <float>(val.y)) + (-0.081f * static_cast <float>(val.z))) + 128.0f);
    return {a, b, c};
}

[[maybe_unused]] inline static uint32_t rgbToYuv(uint32_t val) noexcept {
    uint32_t r = (val & 0xFF0000) >> 16;
    uint32_t g = (val & 0x00FF00) >> 8;
    uint32_t b = (val) & 0x0000FF;

    auto y = static_cast<unsigned int>((0.299f * static_cast <float>(r)) + (0.587f * static_cast <float>(g)) + (0.114f * static_cast <float>(b)));
    auto u = static_cast<unsigned int>(((-0.169f * static_cast <float>(r)) + (-0.331 * static_cast <float>(g)) + (0.5f * static_cast <float>(b))) + 128.0f);
    auto v = static_cast<unsigned int>(((0.5f * static_cast <float>(r)) + (-0.419f * static_cast <float>(g)) + (-0.081f * static_cast <float>(b))) + 128.0f);
    return (y << 16) + (u << 8) + v;
}
