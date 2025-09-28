#pragma once

#include <scaler/image_base.hh>
#include <scaler/types.hh>
#include <scaler/warning_macros.hh>
#include <algorithm>
#include <cmath>

namespace scaler {
    /**
     * Bilinear interpolation scaler - arbitrary scale factors
     * Smooth but can be blurry, good for photos and continuous-tone images
     */
    template<typename InputImage, typename OutputImage>
    auto scale_bilinear(const InputImage& src, float scale_factor)
        -> OutputImage {
        const dimension_t src_width = src.width();
        const dimension_t src_height = src.height();
        const dimension_t dst_width = static_cast<dimension_t>(SCALER_SIZE_TO_FLOAT(src_width) * scale_factor);
        const dimension_t dst_height = static_cast<dimension_t>(SCALER_SIZE_TO_FLOAT(src_height) * scale_factor);

        OutputImage result(dst_width, dst_height, src);

        // Handle edge case of empty or single pixel images
        if (src_width == 0 || src_height == 0) {
            return result;
        }

        if (src_width == 1 && src_height == 1) {
            auto pixel = src.get_pixel(0, 0);
            for (index_t y = 0; y < dst_height; ++y) {
                for (index_t x = 0; x < dst_width; ++x) {
                    result.set_pixel(x, y, pixel);
                }
            }
            return result;
        }

        // Inverse scale for mapping destination to source
        const float inv_scale = 1.0f / scale_factor;

        for (index_t dst_y = 0; dst_y < dst_height; ++dst_y) {
            // Map destination y to source space
            const float src_y = (SCALER_SIZE_TO_FLOAT(dst_y) + 0.5f) * inv_scale - 0.5f;
            const index_t y0 = src_y >= 0 ? static_cast<index_t>(src_y) : 0;
            const index_t y1 = std::min(y0 + 1, src_height - 1);
            const float fy = src_y >= 0 ? src_y - static_cast<float>(y0) : 0.0f;

            for (index_t dst_x = 0; dst_x < dst_width; ++dst_x) {
                // Map destination x to source space
                const float src_x = (SCALER_SIZE_TO_FLOAT(dst_x) + 0.5f) * inv_scale - 0.5f;
                const index_t x0 = src_x >= 0 ? static_cast<index_t>(src_x) : 0;
                const index_t x1 = std::min(x0 + 1, src_width - 1);
                const float fx = src_x >= 0 ? src_x - static_cast<float>(x0) : 0.0f;

                // Get the four neighboring pixels
                const auto p00 = src.get_pixel(x0, y0);
                const auto p10 = src.get_pixel(x1, y0);
                const auto p01 = src.get_pixel(x0, y1);
                const auto p11 = src.get_pixel(x1, y1);

                // Bilinear interpolation
                // First interpolate horizontally
                auto p0 = p00 * (1.0f - fx) + p10 * fx;
                auto p1 = p01 * (1.0f - fx) + p11 * fx;

                // Then interpolate vertically
                auto p = p0 * (1.0f - fy) + p1 * fy;

                result.set_pixel(dst_x, dst_y, p);
            }
        }

        return result;
    }

    /**
     * Separable bilinear scaling - can be more efficient for large scale factors
     * First scales horizontally, then vertically
     */
    template<typename InputImage, typename OutputImage, typename IntermediateImage = OutputImage>
    auto scale_bilinear_separable(const InputImage& src, float scale_factor)
        -> OutputImage {
        const dimension_t src_width = src.width();
        const dimension_t src_height = src.height();
        const dimension_t dst_width = static_cast<dimension_t>(src_width * scale_factor);
        const dimension_t dst_height = static_cast<dimension_t>(src_height * scale_factor);

        // Handle edge cases
        if (src_width == 0 || src_height == 0) {
            return OutputImage(dst_width, dst_height, src);
        }

        // First pass: horizontal scaling
        IntermediateImage temp(dst_width, src_height, src);
        const float inv_scale_x = 1.0f / scale_factor;

        for (index_t y = 0; y < src_height; ++y) {
            for (index_t dst_x = 0; dst_x < dst_width; ++dst_x) {
                const float src_x = (dst_x + 0.5f) * inv_scale_x - 0.5f;
                const index_t x0 = src_x >= 0 ? static_cast<index_t>(src_x) : 0;
                const index_t x1 = std::min(x0 + 1, src_width - 1);
                const float fx = src_x >= 0 ? src_x - static_cast<float>(x0) : 0.0f;

                const auto p0 = src.get_pixel(x0, y);
                const auto p1 = src.get_pixel(x1, y);

                auto p = p0 * (1.0f - fx) + p1 * fx;
                temp.set_pixel(dst_x, y, p);
            }
        }

        // Second pass: vertical scaling
        OutputImage result(dst_width, dst_height, src);
        const float inv_scale_y = 1.0f / scale_factor;

        for (index_t dst_y = 0; dst_y < dst_height; ++dst_y) {
            const float src_y = (dst_y + 0.5f) * inv_scale_y - 0.5f;
            const index_t y0 = src_y >= 0 ? static_cast<index_t>(src_y) : 0;
            const index_t y1 = std::min(y0 + 1, src_height - 1);
            const float fy = src_y >= 0 ? src_y - static_cast<float>(y0) : 0.0f;

            for (index_t x = 0; x < dst_width; ++x) {
                const auto p0 = temp.get_pixel(x, y0);
                const auto p1 = temp.get_pixel(x, y1);

                auto p = p0 * (1.0f - fy) + p1 * fy;
                result.set_pixel(x, dst_y, p);
            }
        }

        return result;
    }
} // namespace scaler
