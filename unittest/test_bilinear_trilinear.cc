#include <doctest/doctest.h>
#include <scaler/unified_scaler.hh>
#include <../include/scaler/cpu/bilinear.hh>
#include <../include/scaler/cpu/trilinear.hh>
#include <scaler/image_base.hh>
#include <scaler/types.hh>
#include <vector>
#include <cmath>

// Test image implementation
template<typename PixelType>
class TestImage : public scaler::input_image_base<TestImage<PixelType>, PixelType>,
                  public scaler::output_image_base<TestImage<PixelType>, PixelType> {
    std::vector<std::vector<PixelType>> data_;
public:
    TestImage(scaler::dimension_t width, scaler::dimension_t height) {
        data_.resize(height, std::vector<PixelType>(width));
    }

    template<typename T>
    TestImage(scaler::dimension_t width, scaler::dimension_t height, const T&) : TestImage(width, height) {}

    TestImage(const std::vector<std::vector<PixelType>>& data) : data_(data) {}

    using scaler::input_image_base<TestImage<PixelType>, PixelType>::width;
    using scaler::input_image_base<TestImage<PixelType>, PixelType>::height;
    using scaler::input_image_base<TestImage<PixelType>, PixelType>::get_pixel;
    using scaler::input_image_base<TestImage<PixelType>, PixelType>::safe_access;

    scaler::dimension_t width_impl() const { return data_.empty() ? 0 : data_[0].size(); }
    scaler::dimension_t height_impl() const { return data_.size(); }
    PixelType get_pixel_impl(scaler::index_t x, scaler::index_t y) const { return data_[y][x]; }
    void set_pixel_impl(scaler::index_t x, scaler::index_t y, const PixelType& pixel) {
        data_[y][x] = pixel;
    }
};

using namespace scaler;

TEST_CASE("Bilinear Scaler") {
    SUBCASE("2x2 to 4x4 upscaling") {
        // Create a simple 2x2 pattern
        std::vector<std::vector<uvec3>> data = {
            {{0, 0, 0}, {255, 255, 255}},     // black, white
            {{255, 0, 0}, {0, 255, 0}}         // red, green
        };
        TestImage<uvec3> input(data);

        auto output = scale_bilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 2.0f);

        CHECK(output.width() == 4);
        CHECK(output.height() == 4);

        // Check corners are close to original pixels (bilinear won't be exact at edges)
        // Top-left should be mostly black
        auto tl = output.get_pixel(0, 0);
        INFO("Top-left pixel: " << tl.x << ", " << tl.y << ", " << tl.z);
        CHECK(tl.x < 100);  // Mostly black

        // Check approximate colors for other corners
        auto tr = output.get_pixel(3, 0);
        CHECK(tr.x > 100);  // Has significant white component

        auto bl = output.get_pixel(0, 3);
        CHECK(bl.x > 100);  // Has significant red component

        auto br = output.get_pixel(3, 3);
        CHECK(br.y > 100);  // Has significant green component

        // Check center is interpolated (should be a mix of all 4 colors)
        auto center1 = output.get_pixel(1, 1);
        auto center2 = output.get_pixel(2, 2);
        // These should be interpolated values
        CHECK(center1.x > 0);
        CHECK(center1.y > 0);
        CHECK(center2.x > 0);
        CHECK(center2.y > 0);
    }

    SUBCASE("Downscaling 4x4 to 2x2") {
        // Create a 4x4 checkerboard
        TestImage<uvec3> input(4, 4);
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                bool is_black = (x + y) % 2 == 0;
                input.set_pixel_impl(x, y, is_black ? uvec3{0, 0, 0} : uvec3{255, 255, 255});
            }
        }

        auto output = scale_bilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 0.5f);

        CHECK(output.width() == 2);
        CHECK(output.height() == 2);

        // All pixels should be gray (average of black and white)
        for (size_t y = 0; y < 2; ++y) {
            for (size_t x = 0; x < 2; ++x) {
                auto pixel = output.get_pixel(x, y);
                // Should be close to middle gray
                CHECK(pixel.x > 100);
                CHECK(pixel.x < 155);
                CHECK(pixel.y > 100);
                CHECK(pixel.y < 155);
                CHECK(pixel.z > 100);
                CHECK(pixel.z < 155);
            }
        }
    }

    SUBCASE("Non-integer scaling") {
        std::vector<std::vector<uvec3>> data = {
            {{255, 0, 0}, {0, 255, 0}},
            {{0, 0, 255}, {255, 255, 0}}
        };
        TestImage<uvec3> input(data);

        auto output = scale_bilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 1.5f);

        CHECK(output.width() == 3);
        CHECK(output.height() == 3);

        // Check that we have smooth interpolation
        auto top_middle = output.get_pixel(1, 0);
        // Should be between red and green
        CHECK(top_middle.x > 0);
        CHECK(top_middle.y > 0);
    }

    SUBCASE("Single pixel image") {
        std::vector<std::vector<uvec3>> data = {{{128, 64, 32}}};
        TestImage<uvec3> input(data);

        auto output = scale_bilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 3.0f);

        CHECK(output.width() == 3);
        CHECK(output.height() == 3);

        // All pixels should be the same
        for (size_t y = 0; y < 3; ++y) {
            for (size_t x = 0; x < 3; ++x) {
                CHECK(output.get_pixel(x, y) == uvec3{128, 64, 32});
            }
        }
    }
}

TEST_CASE("Trilinear Scaler") {
    SUBCASE("Upscaling uses bilinear") {
        std::vector<std::vector<uvec3>> data = {
            {{255, 0, 0}, {0, 255, 0}},
            {{0, 0, 255}, {255, 255, 0}}
        };
        TestImage<uvec3> input(data);

        auto tri_output = scale_trilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 2.0f);
        auto bi_output = scale_bilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 2.0f);

        CHECK(tri_output.width() == bi_output.width());
        CHECK(tri_output.height() == bi_output.height());

        // For upscaling, trilinear should be identical to bilinear
        for (size_t y = 0; y < tri_output.height(); ++y) {
            for (size_t x = 0; x < tri_output.width(); ++x) {
                CHECK(tri_output.get_pixel(x, y) == bi_output.get_pixel(x, y));
            }
        }
    }

    SUBCASE("Downscaling 8x8 to 2x2") {
        // Create an 8x8 gradient
        TestImage<uvec3> input(8, 8);
        for (size_t y = 0; y < 8; ++y) {
            for (size_t x = 0; x < 8; ++x) {
                unsigned char val = static_cast<unsigned char>((x + y) * 255 / 14);
                input.set_pixel_impl(x, y, uvec3{val, val, val});
            }
        }

        auto output = scale_trilinear<TestImage<uvec3>, TestImage<uvec3>>(input, 0.25f);

        CHECK(output.width() == 2);
        CHECK(output.height() == 2);

        // Check that we have smooth downscaling
        auto tl = output.get_pixel(0, 0);
        auto br = output.get_pixel(1, 1);

        // Top-left should be darker than bottom-right
        CHECK(tl.x < br.x);
    }

    SUBCASE("Mipmap generation") {
        // Create a 4x4 test image
        TestImage<uvec3> input(4, 4);
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                unsigned char val = static_cast<unsigned char>(x + y * 4) * 16;
                input.set_pixel_impl(x, y, uvec3{val, val, val});
            }
        }

        // Generate mipmap level 1 (2x2)
        auto mip1 = detail::generateMipmap<TestImage<uvec3>, TestImage<uvec3>>(input, 1);

        CHECK(mip1.width() == 2);
        CHECK(mip1.height() == 2);

        // Each pixel should be average of 2x2 block
        auto tl = mip1.get_pixel(0, 0);
        // Average of (0,16,64,80)/4 = 40
        CHECK(tl.x == 40);

        // Generate mipmap level 2 (1x1)
        auto mip2 = detail::generateMipmap<TestImage<uvec3>, TestImage<uvec3>>(input, 2);

        CHECK(mip2.width() == 1);
        CHECK(mip2.height() == 1);

        // Should be average of all pixels
        auto pixel = mip2.get_pixel(0, 0);
        CHECK(pixel.x == 120); // Average of 0-15 * 16 = 120
    }
}

TEST_CASE("Unified Scaler with Bilinear and Trilinear") {
    std::vector<std::vector<uvec3>> data = {
        {{255, 0, 0}, {0, 255, 0}},
        {{0, 0, 255}, {255, 255, 255}}
    };
    TestImage<uvec3> input(data);

    SUBCASE("Bilinear through unified interface") {
        auto output = Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
            input, algorithm::Bilinear, 2.5f
        );

        CHECK(output.width() == 5);
        CHECK(output.height() == 5);
    }

    SUBCASE("Trilinear through unified interface") {
        auto output = Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
            input, algorithm::Trilinear, 0.5f
        );

        CHECK(output.width() == 1);
        CHECK(output.height() == 1);
    }

    SUBCASE("algorithm capabilities") {
        CHECK(scaler_capabilities::supports_arbitrary_scale(algorithm::Bilinear) == true);
        CHECK(scaler_capabilities::supports_arbitrary_scale(algorithm::Trilinear) == true);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::Bilinear, 3.7f) == true);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::Trilinear, 0.3f) == true);

        CHECK(scaler_capabilities::get_algorithm_name(algorithm::Bilinear) == "Bilinear");
        CHECK(scaler_capabilities::get_algorithm_name(algorithm::Trilinear) == "Trilinear");
    }

    SUBCASE("Get algorithms for scale includes bilinear and trilinear") {
        auto algos = scaler_capabilities::get_algorithms_for_scale(1.5f);
        CHECK(std::find(algos.begin(), algos.end(), algorithm::Bilinear) != algos.end());
        CHECK(std::find(algos.begin(), algos.end(), algorithm::Trilinear) != algos.end());
    }
}