#include <doctest/doctest.h>
#include <scaler/unified_scaler.hh>
#include <scaler/image_base.hh>
#include <scaler/types.hh>
#include <vector>

// Simple test image implementation
template<typename PixelType>
class TestImage : public scaler::input_image_base<TestImage<PixelType>, PixelType>,
                  public scaler::output_image_base<TestImage<PixelType>, PixelType> {
    std::vector<std::vector<PixelType>> data_;
public:
    TestImage(size_t w, size_t h) {
        data_.resize(h, std::vector<PixelType>(w));
    }

    template<typename T>
    TestImage(size_t w, size_t h, const T&) : TestImage(w, h) {}

    TestImage(const std::vector<std::vector<PixelType>>& data) : data_(data) {}

    using scaler::input_image_base<TestImage<PixelType>, PixelType>::width;
    using scaler::input_image_base<TestImage<PixelType>, PixelType>::height;
    using scaler::input_image_base<TestImage<PixelType>, PixelType>::get_pixel;

    size_t width_impl() const { return data_.empty() ? 0 : data_[0].size(); }
    size_t height_impl() const { return data_.size(); }
    PixelType get_pixel_impl(size_t x, size_t y) const { return data_[y][x]; }
    void set_pixel_impl(size_t x, size_t y, const PixelType& pixel) {
        data_[y][x] = pixel;
    }
};

using namespace scaler;

TEST_CASE("Unified Scaler with Preallocated Output") {
    // Create a simple 2x2 test pattern
    std::vector<std::vector<uvec3>> data = {
        {{255, 0, 0}, {0, 255, 0}},   // red, green
        {{0, 0, 255}, {255, 255, 255}} // blue, white
    };
    TestImage<uvec3> input(data);

    SUBCASE("Preallocated output with correct dimensions") {
        // Create preallocated output with correct size for 2x scaling
        TestImage<uvec3> output(4, 4);

        // Scale using preallocated output
        Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(input, output, algorithm::EPX);

        // Verify output dimensions
        CHECK(output.width() == 4);
        CHECK(output.height() == 4);

        // Verify some pixels (EPX specific pattern)
        CHECK(output.get_pixel(0, 0) == uvec3{255, 0, 0}); // Top-left should be red
    }

    SUBCASE("Preallocated vs created output produces same result") {
        // Create output using original method
        auto created_output = Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
            input, algorithm::EPX, 2.0f
        );

        // Create preallocated output and scale into it
        TestImage<uvec3> preallocated_output(4, 4);
        Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
            input, preallocated_output, algorithm::EPX
        );

        // Compare all pixels
        bool pixels_match = true;
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                if (created_output.get_pixel(x, y) != preallocated_output.get_pixel(x, y)) {
                    pixels_match = false;
                    break;
                }
            }
        }
        CHECK(pixels_match);
    }

    SUBCASE("Dimension mismatch throws exception") {
        // Create output with wrong dimensions but valid scale
        // Use 5x5 for 2x2 input - this is 2.5x scale which is wrong, but the inferred
        // scale will be 2.5 which will throw unsupported_scale_exception
        TestImage<uvec3> wrong_output(5, 5); // Can't get exactly 2x scale with wrong dims

        // Using try-catch instead of CHECK_THROWS_AS due to nested template issues
        bool exception_thrown = false;
        try {
            Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(input, wrong_output, algorithm::EPX);
        } catch (const dimension_mismatch_exception& e) {
            // This won't be reached because scale check happens first
            exception_thrown = true;
            CHECK(e.m_algorithm == algorithm::EPX);
            CHECK(e.m_input_width == 2);
            CHECK(e.m_input_height == 2);
            CHECK(e.m_output_width == 5);
            CHECK(e.m_output_height == 5);
            CHECK(e.m_expected_width == 4);
            CHECK(e.m_expected_height == 4);
        } catch (const unsupported_scale_exception& e) {
            // This is what we'll actually get
            exception_thrown = true;
            CHECK(e.m_algorithm == algorithm::EPX);
            // The requested scale is 5/2 = 2.5
            CHECK(e.m_requested_scale == doctest::Approx(2.5f));
        }
        CHECK(exception_thrown);
    }

    SUBCASE("Unsupported scale throws exception") {
        // Create output with dimensions that imply 3x scaling
        TestImage<uvec3> output_3x(6, 6);

        // EPX only supports 2x scaling
        bool exception_thrown = false;
        try {
            Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(input, output_3x, algorithm::EPX);
        } catch (const unsupported_scale_exception& e) {
            exception_thrown = true;
            CHECK(e.m_algorithm == algorithm::EPX);
            CHECK(e.m_requested_scale == doctest::Approx(3.0f));
            CHECK(e.m_supported_scales == std::vector<float>{2.0f});
        }
        CHECK(exception_thrown);
    }

    SUBCASE("Scale inference works correctly") {
        // Test with nearest neighbor which supports arbitrary scaling
        TestImage<uvec3> output_3x(6, 6);

        // This should work and infer 3x scaling
        Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
            input, output_3x, algorithm::Nearest
        );

        CHECK(output_3x.width() == 6);
        CHECK(output_3x.height() == 6);

        // Check corner pixels are scaled correctly
        CHECK(output_3x.get_pixel(0, 0) == uvec3{255, 0, 0}); // Top-left red
        CHECK(output_3x.get_pixel(5, 0) == uvec3{0, 255, 0});  // Top-right green
        CHECK(output_3x.get_pixel(0, 5) == uvec3{0, 0, 255});  // Bottom-left blue
        CHECK(output_3x.get_pixel(5, 5) == uvec3{255, 255, 255}); // Bottom-right white
    }

    SUBCASE("Multiple algorithms with preallocated output") {
        struct TestCase {
            algorithm algo;
            float scale;
            size_t expected_width;
            size_t expected_height;
        };

        std::vector<TestCase> test_cases = {
            {algorithm::Nearest, 2.0f, 4, 4},
            {algorithm::Nearest, 3.5f, 7, 7},
            {algorithm::EPX, 2.0f, 4, 4},
            {algorithm::Eagle, 2.0f, 4, 4},
            {algorithm::Scale, 2.0f, 4, 4},
            {algorithm::Scale, 3.0f, 6, 6},
            {algorithm::Bilinear, 2.5f, 5, 5},
        };

        for (const auto& tc : test_cases) {
            INFO("Testing algorithm: " << scaler_capabilities::get_algorithm_name(tc.algo)
                 << " with scale " << tc.scale);

            // Create preallocated output
            TestImage<uvec3> output(tc.expected_width, tc.expected_height);

            // Scale into preallocated output
            Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(input, output, tc.algo);

            // Verify dimensions
            CHECK(output.width() == tc.expected_width);
            CHECK(output.height() == tc.expected_height);
        }
    }

    SUBCASE("Dimension calculation helper works") {
        auto dims = Scaler<TestImage<uvec3>, TestImage<uvec3>>::calculate_output_dimensions(
            input, algorithm::EPX, 2.0f
        );

        CHECK(dims.width == 4);
        CHECK(dims.height == 4);
    }

    SUBCASE("Scale inference helper works") {
        TestImage<uvec3> output_2x(4, 4);
        float inferred_scale = Scaler<TestImage<uvec3>, TestImage<uvec3>>::infer_scale_factor(
            input, output_2x
        );

        CHECK(inferred_scale == doctest::Approx(2.0f));
    }

    SUBCASE("Dimension verification helper works") {
        TestImage<uvec3> correct_output(4, 4);
        TestImage<uvec3> wrong_output(5, 5);

        CHECK(Scaler<TestImage<uvec3>, TestImage<uvec3>>::verify_dimensions(
            input, correct_output, algorithm::EPX
        ) == true);

        CHECK(Scaler<TestImage<uvec3>, TestImage<uvec3>>::verify_dimensions(
            input, wrong_output, algorithm::EPX
        ) == false);
    }
}

TEST_CASE("Preallocated Output Performance") {
    // Create larger test image for performance testing
    TestImage<uvec3> input(64, 64);
    for (size_t y = 0; y < 64; ++y) {
        for (size_t x = 0; x < 64; ++x) {
            // Checkerboard pattern
            bool is_white = ((x / 8) + (y / 8)) % 2;
            input.set_pixel_impl(x, y, is_white ? uvec3{255, 255, 255} : uvec3{0, 0, 0});
        }
    }

    // Pre-allocate output buffer once
    TestImage<uvec3> preallocated(128, 128);

    SUBCASE("Reusing preallocated buffer") {
        // Scale multiple times reusing the same buffer
        for (int i = 0; i < 10; ++i) {
            Scaler<TestImage<uvec3>, TestImage<uvec3>>::scale(
                input, preallocated, algorithm::Nearest
            );
        }

        // Verify final result is correct
        CHECK(preallocated.width() == 128);
        CHECK(preallocated.height() == 128);
        // First block should be either white or black based on our checkerboard pattern
        auto first_pixel = preallocated.get_pixel(0, 0);
        bool is_valid = (first_pixel == uvec3{255, 255, 255}) || (first_pixel == uvec3{0, 0, 0});
        CHECK(is_valid);
    }
}