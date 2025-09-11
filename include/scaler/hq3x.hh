#pragma once

#include "compiler_compat.hh"
#include <scaler/vec3.hh>
#include <scaler/image_base.hh>
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

namespace scaler {
    namespace hq3x_detail {
        // YUV thresholds from reference
        constexpr uint32_t THRESHOLD_Y = 0x30;
        constexpr uint32_t THRESHOLD_U = 0x07;
        constexpr uint32_t THRESHOLD_V = 0x06;

        // Specialized blend functions for common weight patterns
        
        // blend2_3_1: 75% first color, 25% second (3:1 ratio) - most common
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend2_3_1(const T& c0, const T& c1) noexcept {
            return T{
                static_cast<typename T::value_type>((c0.x * 3 + c1.x) / 4),
                static_cast<typename T::value_type>((c0.y * 3 + c1.y) / 4),
                static_cast<typename T::value_type>((c0.z * 3 + c1.z) / 4)
            };
        }
        
        // blend2_7_1: 87.5% first color, 12.5% second (7:1 ratio)
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend2_7_1(const T& c0, const T& c1) noexcept {
            return T{
                static_cast<typename T::value_type>((c0.x * 7 + c1.x) / 8),
                static_cast<typename T::value_type>((c0.y * 7 + c1.y) / 8),
                static_cast<typename T::value_type>((c0.z * 7 + c1.z) / 8)
            };
        }
        
        // blend2_1_1: 50% each (1:1 ratio)
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend2_1_1(const T& c0, const T& c1) noexcept {
            return T{
                static_cast<typename T::value_type>((c0.x + c1.x) / 2),
                static_cast<typename T::value_type>((c0.y + c1.y) / 2),
                static_cast<typename T::value_type>((c0.z + c1.z) / 2)
            };
        }
        
        // blend3_2_1_1: 50% first, 25% second, 25% third (2:1:1 ratio)
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend3_2_1_1(const T& c0, const T& c1, const T& c2) noexcept {
            return T{
                static_cast<typename T::value_type>((c0.x * 2 + c1.x + c2.x) / 4),
                static_cast<typename T::value_type>((c0.y * 2 + c1.y + c2.y) / 4),
                static_cast<typename T::value_type>((c0.z * 2 + c1.z + c2.z) / 4)
            };
        }
        
        // blend3_2_7_7: special case for 2:7:7 ratio
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend3_2_7_7(const T& c0, const T& c1, const T& c2) noexcept {
            return T{
                static_cast<typename T::value_type>((c0.x * 2 + c1.x * 7 + c2.x * 7) / 16),
                static_cast<typename T::value_type>((c0.y * 2 + c1.y * 7 + c2.y * 7) / 16),
                static_cast<typename T::value_type>((c0.z * 2 + c1.z * 7 + c2.z * 7) / 16)
            };
        }

        // Generic blend functions (rarely used)
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend2(const T& c0, const T& c1, unsigned w0, unsigned w1) noexcept {
            unsigned total = w0 + w1;
            return T{
                static_cast <typename T::value_type>((c0.x * w0 + c1.x * w1) / total),
                static_cast <typename T::value_type>((c0.y * w0 + c1.y * w1) / total),
                static_cast <typename T::value_type>((c0.z * w0 + c1.z * w1) / total)
            };
        }

        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE T blend3(const T& c0, const T& c1, const T& c2, unsigned w0, unsigned w1, unsigned w2) noexcept {
            unsigned total = w0 + w1 + w2;
            return T{
                static_cast <typename T::value_type>((c0.x * w0 + c1.x * w1 + c2.x * w2) / total),
                static_cast <typename T::value_type>((c0.y * w0 + c1.y * w1 + c2.y * w2) / total),
                static_cast <typename T::value_type>((c0.z * w0 + c1.z * w1 + c2.z * w2) / total)
            };
        }

        // YUV difference check - optimized with integer arithmetic
        template<typename T>
        SCALER_FORCE_INLINE SCALER_PURE bool yuvDifference(const T& lhs, const T& rhs) noexcept {
            if (SCALER_UNLIKELY(lhs == rhs)) return false;

            // Use integer arithmetic with fixed point (scale by 256 instead of dividing by 1000)
            int r1 = static_cast<int>(lhs.x), g1 = static_cast<int>(lhs.y), b1 = static_cast<int>(lhs.z);
            int r2 = static_cast<int>(rhs.x), g2 = static_cast<int>(rhs.y), b2 = static_cast<int>(rhs.z);

            // Y difference (scaled by 256)
            int y_diff = std::abs((77 * (r1 - r2) + 150 * (g1 - g2) + 29 * (b1 - b2)) >> 8);
            if (static_cast<uint32_t>(y_diff) > THRESHOLD_Y) return true;

            // U difference (scaled by 256)  
            int u_diff = std::abs(((-43 * (r1 - r2) - 85 * (g1 - g2) + 128 * (b1 - b2)) >> 8));
            if (static_cast<uint32_t>(u_diff) > THRESHOLD_U) return true;

            // V difference (scaled by 256)
            int v_diff = std::abs(((128 * (r1 - r2) - 107 * (g1 - g2) - 21 * (b1 - b2)) >> 8));
            return static_cast<uint32_t>(v_diff) > THRESHOLD_V;
        }

        // Process pattern with all 256 cases
        template<typename T>
        SCALER_HOT SCALER_FLATTEN void processPattern(const std::array <T, 9>& w, T* SCALER_RESTRICT output, int pattern) noexcept {
            // Default: copy center to all
            for (int i = 0; i < 9; i++) output[i] = w[4];

            switch (pattern) {
                case 0:
                case 1:
                case 4:
                case 32:
                case 128:
                case 5:
                case 132:
                case 160:
                case 33:
                case 129:
                case 36:
                case 133:
                case 164:
                case 161:
                case 37:
                case 165:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 2:
                case 34:
                case 130:
                case 162:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 16:
                case 17:
                case 48:
                case 49:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 64:
                case 65:
                case 68:
                case 69:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 8:
                case 12:
                case 136:
                case 140:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 3:
                case 35:
                case 131:
                case 163:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 6:
                case 38:
                case 134:
                case 166:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 20:
                case 21:
                case 52:
                case 53:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 144:
                case 145:
                case 176:
                case 177:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 192:
                case 193:
                case 196:
                case 197:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 96:
                case 97:
                case 100:
                case 101:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 40:
                case 44:
                case 168:
                case 172:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 9:
                case 13:
                case 137:
                case 141:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 18:
                case 50:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[2]);
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 80:
                case 81:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 72:
                case 76:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[6]);
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 10:
                case 138:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 66:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 24:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 7:
                case 39:
                case 135:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 148:
                case 149:
                case 180:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 224:
                case 228:
                case 225:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 41:
                case 169:
                case 45:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 22:
                case 54:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 208:
                case 209:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 104:
                case 108:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 11:
                case 139:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 19:
                case 51:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[0] = blend2_3_1(w[4], w[3]);
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[2]);
                        output[5] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 146:
                case 178:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[2]);
                        output[5] = w[4];
                        output[8] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    break;

                case 84:
                case 85:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[2] = blend2_3_1(w[4], w[1]);
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[7] = blend2_3_1(w[4], w[7]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    break;

                case 112:
                case 113:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[6] = blend2_3_1(w[4], w[3]);
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[5] = blend2_3_1(w[4], w[5]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    break;

                case 200:
                case 204:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[6]);
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[5]);
                    } else {
                        output[3] = blend2_3_1(w[4], w[3]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    break;

                case 73:
                case 77:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[0] = blend2_3_1(w[4], w[1]);
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[6]);
                        output[7] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[4], w[7]);
                    }
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 42:
                case 170:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                        output[1] = w[4];
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 14:
                case 142:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[5]);
                        output[3] = w[4];
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[3] = blend2_3_1(w[4], w[3]);
                    }
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 67:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 70:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 28:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 152:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 194:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 98:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 56:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 25:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 26:
                case 31:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 82:
                case 214:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 88:
                case 248:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 74:
                case 107:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 27:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 86:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 216:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 106:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 30:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 210:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 120:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 75:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 29:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 198:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 184:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 99:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 57:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 71:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 156:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 226:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 60:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 195:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 102:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 153:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 58:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 83:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 92:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 202:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 78:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 154:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 114:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 89:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 90:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 55:
                case 23:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[0] = blend2_3_1(w[4], w[3]);
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 182:
                case 150:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                        output[8] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    break;

                case 213:
                case 212:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[2] = blend2_3_1(w[4], w[1]);
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[7] = blend2_3_1(w[4], w[7]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    break;

                case 241:
                case 240:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[6] = blend2_3_1(w[4], w[3]);
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_3_1(w[4], w[5]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    break;

                case 236:
                case 232:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[5]);
                    } else {
                        output[3] = blend2_3_1(w[4], w[3]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    break;

                case 109:
                case 105:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[0] = blend2_3_1(w[4], w[1]);
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[4], w[7]);
                    }
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 171:
                case 43:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 143:
                case 15:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[5]);
                        output[3] = w[4];
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[3] = blend2_3_1(w[4], w[3]);
                    }
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 124:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 203:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 62:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 211:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 118:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 217:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 110:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 155:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 188:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 185:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 61:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 157:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 103:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 227:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 230:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 199:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 220:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 158:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 234:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 242:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 59:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 121:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 87:
                    output[0] = blend2_3_1(w[4], w[3]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 79:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 122:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 94:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 218:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 91:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 229:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 167:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 173:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 181:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 186:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 115:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 93:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 206:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 205:
                case 201:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = blend2_3_1(w[4], w[6]);
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 174:
                case 46:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = blend2_3_1(w[4], w[0]);
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 179:
                case 147:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = blend2_3_1(w[4], w[2]);
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 117:
                case 116:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = blend2_3_1(w[4], w[8]);
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 189:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 231:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 126:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 219:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 125:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[0] = blend2_3_1(w[4], w[1]);
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[4], w[7]);
                    }
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 221:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[2] = blend2_3_1(w[4], w[1]);
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[7] = blend2_3_1(w[4], w[7]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    break;

                case 207:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[2] = blend2_3_1(w[4], w[5]);
                        output[3] = w[4];
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[3] = blend2_3_1(w[4], w[3]);
                    }
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 238:
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                        output[8] = blend2_3_1(w[4], w[5]);
                    } else {
                        output[3] = blend2_3_1(w[4], w[3]);
                        output[6] = blend2_1_1(w[3], w[7]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    break;

                case 190:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                        output[8] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[5], w[4]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    break;

                case 187:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                        output[6] = blend2_3_1(w[4], w[7]);
                    } else {
                        output[0] = blend2_1_1(w[3], w[1]);
                        output[1] = blend2_3_1(w[4], w[1]);
                        output[3] = blend2_3_1(w[3], w[4]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 243:
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[6] = blend2_3_1(w[4], w[3]);
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_3_1(w[4], w[5]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                        output[7] = blend2_3_1(w[7], w[4]);
                        output[8] = blend2_1_1(w[5], w[7]);
                    }
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    break;

                case 119:
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[0] = blend2_3_1(w[4], w[3]);
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[1] = blend2_3_1(w[1], w[4]);
                        output[2] = blend2_1_1(w[1], w[5]);
                        output[5] = blend2_3_1(w[4], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 237:
                case 233:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 175:
                case 47:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    break;

                case 183:
                case 151:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 245:
                case 244:
                    output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 250:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 123:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 95:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 222:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 252:
                    output[0] = blend2_3_1(w[4], w[0]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 249:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 235:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 111:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 63:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 159:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 215:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 246:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 254:
                    output[0] = blend2_3_1(w[4], w[0]);
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                    }
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 253:
                    output[0] = blend2_3_1(w[4], w[1]);
                    output[1] = blend2_3_1(w[4], w[1]);
                    output[2] = blend2_3_1(w[4], w[1]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 251:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                    }
                    output[2] = blend2_3_1(w[4], w[2]);
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[3] = w[4];
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[3] = blend2_7_1(w[4], w[3]);
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[5] = w[4];
                        output[8] = w[4];
                    } else {
                        output[5] = blend2_7_1(w[4], w[5]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 239:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    output[2] = blend2_3_1(w[4], w[5]);
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = blend2_3_1(w[4], w[5]);
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    output[8] = blend2_3_1(w[4], w[5]);
                    break;

                case 127:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[1] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[2] = blend3_2_7_7(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[4] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                        output[7] = w[4];
                    } else {
                        output[6] = blend3_2_7_7(w[4], w[3], w[7]);
                        output[7] = blend2_7_1(w[4], w[7]);
                    }
                    output[8] = blend2_3_1(w[4], w[8]);
                    break;

                case 191:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[7]);
                    output[7] = blend2_3_1(w[4], w[7]);
                    output[8] = blend2_3_1(w[4], w[7]);
                    break;

                case 223:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                        output[3] = w[4];
                    } else {
                        output[0] = blend3_2_7_7(w[4], w[3], w[1]);
                        output[3] = blend2_7_1(w[4], w[3]);
                    }
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[1] = w[4];
                        output[2] = w[4];
                        output[5] = w[4];
                    } else {
                        output[1] = blend2_7_1(w[4], w[1]);
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                        output[5] = blend2_7_1(w[4], w[5]);
                    }
                    output[4] = w[4];
                    output[6] = blend2_3_1(w[4], w[6]);
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[7] = w[4];
                        output[8] = w[4];
                    } else {
                        output[7] = blend2_7_1(w[4], w[7]);
                        output[8] = blend3_2_7_7(w[4], w[5], w[7]);
                    }
                    break;

                case 247:
                    output[0] = blend2_3_1(w[4], w[3]);
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = blend2_3_1(w[4], w[3]);
                    output[4] = w[4];
                    output[5] = w[4];
                    output[6] = blend2_3_1(w[4], w[3]);
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;

                case 255:
                    if (hq3x_detail::yuvDifference(w[3], w[1])) {
                        output[0] = w[4];
                    } else {
                        output[0] = blend3_2_1_1(w[4], w[3], w[1]);
                    }
                    output[1] = w[4];
                    if (hq3x_detail::yuvDifference(w[1], w[5])) {
                        output[2] = w[4];
                    } else {
                        output[2] = blend3_2_1_1(w[4], w[1], w[5]);
                    }
                    output[3] = w[4];
                    output[4] = w[4];
                    output[5] = w[4];
                    if (hq3x_detail::yuvDifference(w[7], w[3])) {
                        output[6] = w[4];
                    } else {
                        output[6] = blend3_2_1_1(w[4], w[7], w[3]);
                    }
                    output[7] = w[4];
                    if (hq3x_detail::yuvDifference(w[5], w[7])) {
                        output[8] = w[4];
                    } else {
                        output[8] = blend3_2_1_1(w[4], w[5], w[7]);
                    }
                    break;
            }
        }
    } // namespace hq3x_detail

    // Main HQ3x function - optimized with row caching
    template<typename InputImage, typename OutputImage>
    SCALER_HOT auto scaleHq3x(const InputImage& src) -> OutputImage {
        const int src_width = src.width();
        const int src_height = src.height();

        OutputImage result(src_width * 3, src_height * 3, src);

        if (SCALER_UNLIKELY(src_width == 0 || src_height == 0)) {
            return result;
        }

        using PixelType = decltype(src.get_pixel(0, 0));
        
        // Pre-allocate row buffers for sliding window
        std::vector<PixelType> prev_row;
        std::vector<PixelType> curr_row;
        std::vector<PixelType> next_row;
        
        prev_row.reserve(src_width + 2);
        curr_row.reserve(src_width + 2);
        next_row.reserve(src_width + 2);

        for (int y = 0; y < src_height; ++y) {
            // Load rows with padding for edges
            prev_row.clear();
            curr_row.clear();
            next_row.clear();
            
            // Load previous row (or edge)
            for (int x = -1; x <= src_width; ++x) {
                prev_row.push_back(src.safeAccess(x, y - 1));
            }
            
            // Load current row
            for (int x = -1; x <= src_width; ++x) {
                if (x == -1 || x == src_width) {
                    curr_row.push_back(src.safeAccess(x, y));
                } else {
                    curr_row.push_back(src.get_pixel(x, y));
                }
            }
            
            // Load next row (or edge)
            for (int x = -1; x <= src_width; ++x) {
                next_row.push_back(src.safeAccess(x, y + 1));
            }
            
            for (int x = 0; x < src_width; ++x) {
                // Get 3x3 window from cached rows (index offset by 1 for padding)
                std::array <PixelType, 9> w;
                w[0] = prev_row[x];      // x-1, y-1
                w[1] = prev_row[x + 1];  // x,   y-1
                w[2] = prev_row[x + 2];  // x+1, y-1
                w[3] = curr_row[x];      // x-1, y
                w[4] = curr_row[x + 1];  // x,   y  (center)
                w[5] = curr_row[x + 2];  // x+1, y
                w[6] = next_row[x];      // x-1, y+1
                w[7] = next_row[x + 1];  // x,   y+1
                w[8] = next_row[x + 2];  // x+1, y+1

                // Compute pattern - unrolled for better performance
                int pattern = 0;
                const PixelType& center = w[4];
                
                // Check each neighbor and set corresponding bit if different
                if (w[0] != center && hq3x_detail::yuvDifference(center, w[0])) pattern |= 1;
                if (w[1] != center && hq3x_detail::yuvDifference(center, w[1])) pattern |= 2;
                if (w[2] != center && hq3x_detail::yuvDifference(center, w[2])) pattern |= 4;
                if (w[3] != center && hq3x_detail::yuvDifference(center, w[3])) pattern |= 8;
                // w[4] is center, skip
                if (w[5] != center && hq3x_detail::yuvDifference(center, w[5])) pattern |= 32;
                if (w[6] != center && hq3x_detail::yuvDifference(center, w[6])) pattern |= 64;
                if (w[7] != center && hq3x_detail::yuvDifference(center, w[7])) pattern |= 128;
                if (w[8] != center && hq3x_detail::yuvDifference(center, w[8])) pattern |= 256;

                // Process pattern
                std::array <PixelType, 9> output;
                hq3x_detail::processPattern(w, output.data(), pattern);

                // Write 3x3 block
                int out_x = x * 3;
                int out_y = y * 3;

                result.set_pixel(out_x, out_y, output[0]);
                result.set_pixel(out_x + 1, out_y, output[1]);
                result.set_pixel(out_x + 2, out_y, output[2]);
                result.set_pixel(out_x, out_y + 1, output[3]);
                result.set_pixel(out_x + 1, out_y + 1, output[4]);
                result.set_pixel(out_x + 2, out_y + 1, output[5]);
                result.set_pixel(out_x, out_y + 2, output[6]);
                result.set_pixel(out_x + 1, out_y + 2, output[7]);
                result.set_pixel(out_x + 2, out_y + 2, output[8]);
            }
        }

        return result;
    }
} // namespace scaler
