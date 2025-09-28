#pragma once
#include <cstdint>
#include <scaler/vec3.hh>

namespace scaler {
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
    bool three_or_more_identical(T a, T b, T c, T d) noexcept {
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
    T bilinear_interpolation(T top_left, T top_right, T bottom_left, T bottom_right,
                             float right_proportion, float bottom_proportion) noexcept {
        T top_interp = mix(top_left, top_right, right_proportion);
        T bottom_interp = mix(bottom_left, bottom_right, right_proportion);
        return mix(top_interp, bottom_interp, bottom_proportion);
    }

    inline static uvec3 rgb_to_yuv(const uvec3& val) noexcept {
        // Use integer arithmetic with fixed-point representation (16-bit precision)
        // Coefficients multiplied by 65536 for precision
        constexpr int Y_R = 19595; // 0.299 * 65536
        constexpr int Y_G = 38470; // 0.587 * 65536
        constexpr int Y_B = 7471; // 0.114 * 65536

        constexpr int U_R = -11076; // -0.169 * 65536
        constexpr int U_G = -21692; // -0.331 * 65536
        constexpr int U_B = 32768; // 0.5 * 65536

        constexpr int V_R = 32768; // 0.5 * 65536
        constexpr int V_G = -27460; // -0.419 * 65536
        constexpr int V_B = -5308; // -0.081 * 65536

        int r = static_cast <int>(val.x);
        int g = static_cast <int>(val.y);
        int b = static_cast <int>(val.z);

        auto y = static_cast <unsigned int>((Y_R * r + Y_G * g + Y_B * b) >> 16);
        auto u = static_cast <unsigned int>(((U_R * r + U_G * g + U_B * b) >> 16) + 128);
        auto v = static_cast <unsigned int>(((V_R * r + V_G * g + V_B * b) >> 16) + 128);

        return {y, u, v};
    }

    [[maybe_unused]] static uint32_t rgb_to_yuv(uint32_t val) noexcept {
        // Use same integer coefficients as above
        constexpr int Y_R = 19595, Y_G = 38470, Y_B = 7471;
        constexpr int U_R = -11076, U_G = -21692, U_B = 32768;
        constexpr int V_R = 32768, V_G = -27460, V_B = -5308;

        const auto r = static_cast <int32_t>((val & 0xFF0000) >> 16);
        const auto g = static_cast <int32_t>((val & 0x00FF00) >> 8);
        const auto b = static_cast <int32_t>(val & 0x0000FF);

        const auto y = static_cast <uint32_t>((Y_R * r + Y_G * g + Y_B * b) >> 16);
        const auto u = static_cast <uint32_t>(((U_R * r + U_G * g + U_B * b) >> 16) + 128);
        const auto v = static_cast <uint32_t>(((V_R * r + V_G * g + V_B * b) >> 16) + 128);
        return (y << 16) | (u << 8) | v;
    }
}
