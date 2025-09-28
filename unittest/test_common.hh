#pragma once

#include <scaler/image_base.hh>
#include <scaler/unified_scaler.hh>
#include <scaler/algorithm_capabilities.hh>
#include <scaler/warning_macros.hh>
#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <set>

namespace scaler::test {

    // Common test image types - using the types from test_unified_scaler.cc
    template <typename PixelType>
    class TestInputImage : public scaler::input_image_base<TestInputImage<PixelType>, PixelType> {
    public:
        using pixel_type = PixelType;

        TestInputImage(size_t w, size_t h) : width_(w), height_(h), data_(w * h) {}

        // CRTP implementation methods
        size_t width_impl() const { return width_; }
        size_t height_impl() const { return height_; }
        PixelType get_pixel_impl(size_t x, size_t y) const { return data_[y * width_ + x]; }

        size_t width() const { return width_; }
        size_t height() const { return height_; }
        // Don't override safe_access - use base class implementation with NEAREST strategy
        PixelType& at(size_t x, size_t y) { return data_[y * width_ + x]; }
        const PixelType& at(size_t x, size_t y) const { return data_[y * width_ + x]; }

    private:
        size_t width_, height_;
        std::vector<PixelType> data_;
    };

    template <typename PixelType>
    class TestOutputImage : public scaler::input_image_base<TestOutputImage<PixelType>, PixelType>,
                            public scaler::output_image_base<TestOutputImage<PixelType>, PixelType> {
    public:
        using pixel_type = PixelType;

        TestOutputImage(size_t w, size_t h) : width_(w), height_(h), data_(w * h) {}

        // 3-argument constructor for algorithms that need it (HQ2x, etc)
        template<typename T>
        TestOutputImage(size_t w, size_t h, const T&) : TestOutputImage(w, h) {}

        // Disambiguate width() and height() by hiding base class versions
        using scaler::input_image_base<TestOutputImage<PixelType>, PixelType>::width;
        using scaler::input_image_base<TestOutputImage<PixelType>, PixelType>::height;

        // CRTP implementation methods
        size_t width_impl() const { return width_; }
        size_t height_impl() const { return height_; }
        PixelType get_pixel_impl(size_t x, size_t y) const { return data_[y * width_ + x]; }
        void set_pixel_impl(size_t x, size_t y, const PixelType& p) { data_[y * width_ + x] = p; }

        // Input image interface
        size_t width() const { return width_; }
        size_t height() const { return height_; }
        // Don't override safe_access - use base class implementation with NEAREST strategy

        // Output image interface
        void set_pixel(size_t x, size_t y, const PixelType& p) { data_[y * width_ + x] = p; }

        // Common accessors
        const PixelType& at(size_t x, size_t y) const { return data_[y * width_ + x]; }
        PixelType& at(size_t x, size_t y) { return data_[y * width_ + x]; }

        // Make sure get_pixel is available (from input_image_base)
        PixelType get_pixel(size_t x, size_t y) const { return at(x, y); }

    private:
        size_t width_, height_;
        std::vector<PixelType> data_;
    };

    // Common pixel types
    using TestImage = TestOutputImage<uvec3>;
    using TestInputImageRGB = TestInputImage<uvec3>;
    using TestOutputImageRGB = TestOutputImage<uvec3>;

    // Common test pattern generators
    inline TestInputImageRGB create_checkerboard(size_t size, uvec3 color1 = {255, 255, 255}, uvec3 color2 = {0, 0, 0}) {
        TestInputImageRGB img(size, size);
        for (size_t y = 0; y < size; ++y) {
            for (size_t x = 0; x < size; ++x) {
                img.at(x, y) = ((x + y) % 2 == 0) ? color1 : color2;
            }
        }
        return img;
    }

    inline TestInputImageRGB create_gradient(size_t width, size_t height) {
        TestInputImageRGB img(width, height);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                auto val = static_cast<uint8_t>((x * 255) / (width - 1));
                img.at(x, y) = {val, val, val};
            }
        }
        return img;
    }

    inline TestInputImageRGB create_single_pixel(uvec3 color = {255, 0, 0}) {
        TestInputImageRGB img(1, 1);
        img.at(0, 0) = color;
        return img;
    }

    inline TestInputImageRGB create_vertical_lines(size_t width, size_t height) {
        TestInputImageRGB img(width, height);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                img.at(x, y) = (x % 2 == 0) ? uvec3{255, 255, 255} : uvec3{0, 0, 0};
            }
        }
        return img;
    }

    inline TestInputImageRGB create_horizontal_lines(size_t width, size_t height) {
        TestInputImageRGB img(width, height);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                img.at(x, y) = (y % 2 == 0) ? uvec3{255, 255, 255} : uvec3{0, 0, 0};
            }
        }
        return img;
    }

    inline TestInputImageRGB create_diagonal_line(size_t size) {
        TestInputImageRGB img(size, size);
        for (size_t y = 0; y < size; ++y) {
            for (size_t x = 0; x < size; ++x) {
                img.at(x, y) = (x == y) ? uvec3{255, 255, 255} : uvec3{0, 0, 0};
            }
        }
        return img;
    }

    inline TestInputImageRGB create_solid_color(size_t width, size_t height, uvec3 color) {
        TestInputImageRGB img(width, height);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                img.at(x, y) = color;
            }
        }
        return img;
    }

    // Common test pattern for testing edge detection
    inline TestInputImageRGB create_edge_pattern() {
        TestInputImageRGB img(4, 4);
        // Create a simple edge: left half white, right half black
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                img.at(x, y) = (x < 2) ? uvec3{255, 255, 255} : uvec3{0, 0, 0};
            }
        }
        return img;
    }

    // Common validation helpers
    inline bool validate_dimensions(const TestImage& output, const TestInputImageRGB& input, float scale) {
        size_t expected_width = static_cast<size_t>(SCALER_SIZE_TO_FLOAT(input.width()) * scale);
        size_t expected_height = static_cast<size_t>(SCALER_SIZE_TO_FLOAT(input.height()) * scale);
        return output.width() == expected_width && output.height() == expected_height;
    }

    inline bool colors_equal(const uvec3& a, const uvec3& b, uint8_t tolerance = 0) {
        return (a.x > b.x ? a.x - b.x : b.x - a.x) <= tolerance &&
               (a.y > b.y ? a.y - b.y : b.y - a.y) <= tolerance &&
               (a.z > b.z ? a.z - b.z : b.z - a.z) <= tolerance;
    }

    inline bool validate_color_preservation(const TestImage& output, const TestInputImageRGB& input) {
        // For scaling algorithms, just check that each unique input color exists somewhere in the output
        // This is a more relaxed check that allows for interpolation and edge effects
        std::vector<uvec3> input_colors;
        for (size_t y = 0; y < input.height(); ++y) {
            for (size_t x = 0; x < input.width(); ++x) {
                auto color = input.at(x, y);
                // Check if color is already in the list
                bool already_exists = false;
                for (const auto& existing : input_colors) {
                    if (existing.x == color.x && existing.y == color.y && existing.z == color.z) {
                        already_exists = true;
                        break;
                    }
                }
                if (!already_exists) {
                    input_colors.push_back(color);
                }
            }
        }

        // For each unique input color, check if it exists somewhere in the output
        for (const auto& color : input_colors) {
            bool found = false;
            for (size_t y = 0; y < output.height() && !found; ++y) {
                for (size_t x = 0; x < output.width() && !found; ++x) {
                    if (colors_equal(output.at(x, y), color, 5)) {  // Allow small tolerance
                        found = true;
                    }
                }
            }
            // Only fail if a non-black color was completely lost
            if (!found && (color.x > 10 || color.y > 10 || color.z > 10)) {
                return false;
            }
        }
        return true;
    }

    // Helper to check if algorithm is edge-preserving
    inline bool is_edge_preserving_algorithm(algorithm algo) {
        switch (algo) {
            case algorithm::EPX:
            case algorithm::Eagle:
            case algorithm::Scale:
            case algorithm::ScaleSFX:
            case algorithm::Super2xSaI:
            case algorithm::HQ:
            case algorithm::xBR:
            case algorithm::xBRZ:
                return true;
            default:
                return false;
        }
    }

    // Helper to check if algorithm supports a specific scale
    inline bool algorithm_supports_scale(algorithm algo, float scale) {
        auto scales = scaler_capabilities::get_supported_scales(algo);
        if (scales.empty()) {
            // Arbitrary scale support
            return scaler_capabilities::supports_arbitrary_scale(algo);
        }
        // Check if scale is in the list
        return std::find(scales.begin(), scales.end(), scale) != scales.end();
    }

} // namespace scaler::test