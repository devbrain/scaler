#pragma once

#include <scaler/cpu/bilinear.hh>
#include <scaler/image_base.hh>
#include <scaler/warning_macros.hh>
#include <algorithm>
#include <cmath>

namespace scaler {
    // Forward declaration
    namespace detail {
        template<typename InputImage, typename OutputImage>
        auto generateMipmap(const InputImage& src, int level) -> OutputImage;
    }

    /**
     * Trilinear scaling - uses mipmapping for downscaling
     * Better quality than bilinear for downscaling (scale < 1.0)
     * Falls back to bilinear for upscaling
     */
    template<typename InputImage, typename OutputImage, typename IntermediateImage = OutputImage>
    OutputImage scale_trilinear(const InputImage& src, float scale_factor) {
        // For upscaling (scale > 1.0), trilinear is same as bilinear
        if (scale_factor >= 1.0f) {
            return scale_bilinear <InputImage, OutputImage>(src, scale_factor);
        }

        // For downscaling, we use mipmap levels
        const size_t src_width = src.width();
        const size_t src_height = src.height();
        const auto dst_width = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(src_width) * scale_factor);
        const auto dst_height = static_cast <size_t>(SCALER_SIZE_TO_FLOAT(src_height) * scale_factor);

        // Handle edge cases
        if (src_width == 0 || src_height == 0) {
            return OutputImage(dst_width, dst_height, src);
        }

        // Calculate which mipmap levels to use
        const float log_scale = -std::log2(scale_factor);
        const int mip_level_0 = static_cast <int>(std::floor(log_scale));
        const int mip_level_1 = mip_level_0 + 1;
        const float mip_blend = log_scale - static_cast<float>(mip_level_0);

        // Generate mipmap level 0
        float level_0_scale = static_cast<float>(std::pow(0.5f, mip_level_0));
        IntermediateImage mip_0 = (mip_level_0 == 0)
                                      ? IntermediateImage(src_width, src_height, src)
                                      : detail::generateMipmap <InputImage, IntermediateImage>(src, mip_level_0);

        // Copy source pixels if mip level 0 is the original
        if (mip_level_0 == 0) {
            for (size_t y = 0; y < src_height; ++y) {
                for (size_t x = 0; x < src_width; ++x) {
                    mip_0.set_pixel(x, y, src.get_pixel(x, y));
                }
            }
        }

        // Generate mipmap level 1
        IntermediateImage mip_1 = detail::generateMipmap <InputImage, IntermediateImage>(src, mip_level_1);

        // Calculate the scale factors relative to each mipmap level
        const float scale_from_mip_0 = scale_factor / level_0_scale;
        const float scale_from_mip_1 = scale_factor / (level_0_scale * 0.5f);

        // Sample from both mipmap levels
        auto sample_0 = scale_bilinear <IntermediateImage, OutputImage>(mip_0, scale_from_mip_0);
        auto sample_1 = scale_bilinear <IntermediateImage, OutputImage>(mip_1, scale_from_mip_1);

        // Blend between the two samples
        OutputImage result(dst_width, dst_height, src);

        for (size_t y = 0; y < dst_height; ++y) {
            for (size_t x = 0; x < dst_width; ++x) {
                auto p0 = sample_0.get_pixel(x, y);
                auto p1 = sample_1.get_pixel(x, y);
                auto p = p0 * (1.0f - mip_blend) + p1 * mip_blend;
                result.set_pixel(x, y, p);
            }
        }

        return result;
    }

    namespace detail {
        /**
         * Generate a specific mipmap level using box filtering
         */
        template<typename InputImage, typename OutputImage>
        auto generateMipmap(const InputImage& src, int level)
            -> OutputImage {
            if (level <= 0) {
                // Level 0 is the original image
                OutputImage result(src.width(), src.height(), src);
                for (size_t y = 0; y < src.height(); ++y) {
                    for (size_t x = 0; x < src.width(); ++x) {
                        result.set_pixel(x, y, src.get_pixel(x, y));
                    }
                }
                return result;
            }

            // Each level halves the dimensions
            const size_t scale_divisor = 1 << level; // 2^level
            const size_t mip_width = std::max(size_t(1), src.width() / scale_divisor);
            const size_t mip_height = std::max(size_t(1), src.height() / scale_divisor);

            OutputImage result(mip_width, mip_height, src);

            // Use box filtering to generate the mipmap
            for (size_t y = 0; y < mip_height; ++y) {
                for (size_t x = 0; x < mip_width; ++x) {
                    // Calculate source region to average
                    const size_t src_x0 = x * scale_divisor;
                    const size_t src_y0 = y * scale_divisor;
                    const size_t src_x1 = std::min(src_x0 + scale_divisor, src.width());
                    const size_t src_y1 = std::min(src_y0 + scale_divisor, src.height());

                    // Average all pixels in the source region
                    auto sum = decltype(src.get_pixel(0, 0)){};
                    size_t count = 0;

                    for (size_t sy = src_y0; sy < src_y1; ++sy) {
                        for (size_t sx = src_x0; sx < src_x1; ++sx) {
                            sum = sum + src.get_pixel(sx, sy);
                            count++;
                        }
                    }

                    // Store average pixel
                    if (count > 0) {
                        result.set_pixel(x, y, sum * (1.0f / SCALER_SIZE_TO_FLOAT(count)));
                    }
                }
            }

            return result;
        }
    }

    /**
     * Fast trilinear scaling using separable filters
     * More efficient but may have slightly different results
     */
    template<typename InputImage, typename OutputImage>
    OutputImage scale_trilinear_fast(const InputImage& src, float scale_factor) {
        // For upscaling, use bilinear separable
        if (scale_factor >= 1.0f) {
            return scale_bilinear_separable <InputImage, OutputImage>(src, scale_factor);
        }

        // For significant downscaling (< 0.5x), use box filter first
        if (scale_factor < 0.5f) {
            // Calculate how many halvings we can do
            int halvings = 0;
            float current_scale = scale_factor;
            while (current_scale < 0.5f && halvings < 4) {
                // Limit to 4 halvings
                current_scale *= 2.0f;
                halvings++;
            }

            // Generate appropriate mipmap level
            auto mipmap = detail::generateMipmap <InputImage, OutputImage>(src, halvings);

            // Apply bilinear to the mipmap
            return scale_bilinear_separable <OutputImage, OutputImage>(mipmap, current_scale);
        }

        // For modest downscaling (0.5x to 1.0x), just use bilinear
        return scale_bilinear_separable <InputImage, OutputImage>(src, scale_factor);
    }
} // namespace scaler
