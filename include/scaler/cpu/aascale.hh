#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <vector>

namespace scaler {
    // AAScale2x - Anti-aliased Scale2x algorithm
    namespace detail {
        template<typename InputImage, typename OutputImage>
        class aa_scale_2x_impl {
            public:
                explicit aa_scale_2x_impl(const InputImage& src)
                    : src_(src) {
                }

                OutputImage operator()() const {
                    const size_t src_width = src_.width();
                    const size_t src_height = src_.height();
                    const size_t dst_width = src_width * 2;
                    const size_t dst_height = src_height * 2;

                    OutputImage dst(dst_width, dst_height, src_);

                    for (size_t y = 0; y < src_height; ++y) {
                        for (size_t x = 0; x < src_width; ++x) {
                            // Get 3x3 neighborhood (as uvec3)
                            auto E = src_.get_pixel(x, y);
                            auto B = (y > 0) ? src_.get_pixel(x, y - 1) : E;
                            auto D = (x > 0) ? src_.get_pixel(x - 1, y) : E;
                            auto F = (x < src_width - 1) ? src_.get_pixel(x + 1, y) : E;
                            auto H = (y < src_height - 1) ? src_.get_pixel(x, y + 1) : E;

                            // Calculate Scale2x pixels
                            auto E0 = E; // Top-left
                            auto E1 = E; // Top-right
                            auto E2 = E; // Bottom-left
                            auto E3 = E; // Bottom-right

                            // Scale2x algorithm rules
                            if (B != H && D != F) {
                                E0 = (D == B) ? D : E;
                                E1 = (B == F) ? F : E;
                                E2 = (D == H) ? D : E;
                                E3 = (H == F) ? F : E;
                            }

                            // Anti-aliasing: blend with original pixel (50% mix)
                            auto blend = [](auto pixel1, auto pixel2) {
                                return decltype(pixel1){
                                    (pixel1.x + pixel2.x) / 2,
                                    (pixel1.y + pixel2.y) / 2,
                                    (pixel1.z + pixel2.z) / 2
                                };
                            };

                            E0 = blend(E0, E);
                            E1 = blend(E1, E);
                            E2 = blend(E2, E);
                            E3 = blend(E3, E);

                            // Write 2x2 block
                            dst.set_pixel(x * 2, y * 2, E0);
                            dst.set_pixel(x * 2 + 1, y * 2, E1);
                            dst.set_pixel(x * 2, y * 2 + 1, E2);
                            dst.set_pixel(x * 2 + 1, y * 2 + 1, E3);
                        }
                    }

                    return dst;
                }

            private:
                const InputImage& src_;
        };

        // AAScale4x - Anti-aliased Scale4x algorithm
        template<typename InputImage, typename OutputImage>
        class aa_scale_4x_impl {
            public:
                explicit aa_scale_4x_impl(const InputImage& src)
                    : src_(src) {
                }

                OutputImage operator()() const {
                    const size_t src_width = src_.width();
                    const size_t src_height = src_.height();
                    const size_t dst_width = src_width * 4;
                    const size_t dst_height = src_height * 4;

                    OutputImage dst(dst_width, dst_height, src_);

                    // Store intermediate result in a vector
                    using PixelType = decltype(src_.get_pixel(0, 0));
                    std::vector <std::vector <PixelType>> intermediate(src_height * 2,
                                                                       std::vector <PixelType>(src_width * 2));

                    // First pass: Scale2x on original
                    for (size_t y = 0; y < src_height; ++y) {
                        for (size_t x = 0; x < src_width; ++x) {
                            auto E = src_.get_pixel(x, y);
                            auto B = (y > 0) ? src_.get_pixel(x, y - 1) : E;
                            auto D = (x > 0) ? src_.get_pixel(x - 1, y) : E;
                            auto F = (x < src_width - 1) ? src_.get_pixel(x + 1, y) : E;
                            auto H = (y < src_height - 1) ? src_.get_pixel(x, y + 1) : E;

                            auto E0 = E, E1 = E, E2 = E, E3 = E;

                            if (B != H && D != F) {
                                E0 = (D == B) ? D : E;
                                E1 = (B == F) ? F : E;
                                E2 = (D == H) ? D : E;
                                E3 = (H == F) ? F : E;
                            }

                            intermediate[y * 2][x * 2] = E0;
                            intermediate[y * 2][x * 2 + 1] = E1;
                            intermediate[y * 2 + 1][x * 2] = E2;
                            intermediate[y * 2 + 1][x * 2 + 1] = E3;
                        }
                    }

                    // Second pass: Scale2x on intermediate result
                    for (size_t y = 0; y < src_height * 2; ++y) {
                        for (size_t x = 0; x < src_width * 2; ++x) {
                            auto E = intermediate[y][x];
                            auto B = (y > 0) ? intermediate[y - 1][x] : E;
                            auto D = (x > 0) ? intermediate[y][x - 1] : E;
                            auto F = (x < src_width * 2 - 1) ? intermediate[y][x + 1] : E;
                            auto H = (y < src_height * 2 - 1) ? intermediate[y + 1][x] : E;

                            auto E0 = E, E1 = E, E2 = E, E3 = E;

                            if (B != H && D != F) {
                                E0 = (D == B) ? D : E;
                                E1 = (B == F) ? F : E;
                                E2 = (D == H) ? D : E;
                                E3 = (H == F) ? F : E;
                            }

                            // Anti-aliasing: blend with the intermediate pixel (50% mix)
                            auto blend = [](auto pixel1, auto pixel2) {
                                return PixelType{
                                    (pixel1.x + pixel2.x) / 2,
                                    (pixel1.y + pixel2.y) / 2,
                                    (pixel1.z + pixel2.z) / 2
                                };
                            };

                            E0 = blend(E0, E);
                            E1 = blend(E1, E);
                            E2 = blend(E2, E);
                            E3 = blend(E3, E);

                            dst.set_pixel(x * 2, y * 2, E0);
                            dst.set_pixel(x * 2 + 1, y * 2, E1);
                            dst.set_pixel(x * 2, y * 2 + 1, E2);
                            dst.set_pixel(x * 2 + 1, y * 2 + 1, E3);
                        }
                    }

                    return dst;
                }

            private:
                const InputImage& src_;
        };

        // Scale4x - Standard Scale4x algorithm (Scale2x applied twice)
        template<typename InputImage, typename OutputImage>
        class scale_4x_impl {
            public:
                explicit scale_4x_impl(const InputImage& src)
                    : src_(src) {
                }

                OutputImage operator()() const {
                    const size_t src_width = src_.width();
                    const size_t src_height = src_.height();
                    const size_t dst_width = src_width * 4;
                    const size_t dst_height = src_height * 4;

                    OutputImage dst(dst_width, dst_height, src_);

                    // Store intermediate result in a vector
                    using PixelType = decltype(src_.get_pixel(0, 0));
                    std::vector <std::vector <PixelType>> intermediate(src_height * 2,
                                                                       std::vector <PixelType>(src_width * 2));

                    // First pass: Scale2x on original
                    for (size_t y = 0; y < src_height; ++y) {
                        for (size_t x = 0; x < src_width; ++x) {
                            auto E = src_.get_pixel(x, y);
                            auto B = (y > 0) ? src_.get_pixel(x, y - 1) : E;
                            auto D = (x > 0) ? src_.get_pixel(x - 1, y) : E;
                            auto F = (x < src_width - 1) ? src_.get_pixel(x + 1, y) : E;
                            auto H = (y < src_height - 1) ? src_.get_pixel(x, y + 1) : E;

                            auto E0 = E, E1 = E, E2 = E, E3 = E;

                            if (B != H && D != F) {
                                E0 = (D == B) ? D : E;
                                E1 = (B == F) ? F : E;
                                E2 = (D == H) ? D : E;
                                E3 = (H == F) ? F : E;
                            }

                            intermediate[y * 2][x * 2] = E0;
                            intermediate[y * 2][x * 2 + 1] = E1;
                            intermediate[y * 2 + 1][x * 2] = E2;
                            intermediate[y * 2 + 1][x * 2 + 1] = E3;
                        }
                    }

                    // Second pass: Scale2x on intermediate result
                    for (size_t y = 0; y < src_height * 2; ++y) {
                        for (size_t x = 0; x < src_width * 2; ++x) {
                            auto E = intermediate[y][x];
                            auto B = (y > 0) ? intermediate[y - 1][x] : E;
                            auto D = (x > 0) ? intermediate[y][x - 1] : E;
                            auto F = (x < src_width * 2 - 1) ? intermediate[y][x + 1] : E;
                            auto H = (y < src_height * 2 - 1) ? intermediate[y + 1][x] : E;

                            auto E0 = E, E1 = E, E2 = E, E3 = E;

                            if (B != H && D != F) {
                                E0 = (D == B) ? D : E;
                                E1 = (B == F) ? F : E;
                                E2 = (D == H) ? D : E;
                                E3 = (H == F) ? F : E;
                            }

                            dst.set_pixel(x * 2, y * 2, E0);
                            dst.set_pixel(x * 2 + 1, y * 2, E1);
                            dst.set_pixel(x * 2, y * 2 + 1, E2);
                            dst.set_pixel(x * 2 + 1, y * 2 + 1, E3);
                        }
                    }

                    return dst;
                }

            private:
                const InputImage& src_;
        };
    } // namespace detail

    // Public API functions
    template<typename InputImage, typename OutputImage>
    auto scale_aa_scale_2x(const InputImage& src, [[maybe_unused]] size_t scale_factor = 2) {
        return detail::aa_scale_2x_impl <InputImage, OutputImage>{src}();
    }

    template<typename InputImage, typename OutputImage>
    auto scale_aa_scale_4x(const InputImage& src, [[maybe_unused]] size_t scale_factor = 4) {
        return detail::aa_scale_4x_impl <InputImage, OutputImage>{src}();
    }

    template<typename InputImage, typename OutputImage>
    auto scale_scale_4x(const InputImage& src, [[maybe_unused]] size_t scale_factor = 4) {
        return detail::scale_4x_impl <InputImage, OutputImage>{src}();
    }
} // namespace scaler
