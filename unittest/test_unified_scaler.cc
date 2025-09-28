#include <doctest/doctest.h>
#include <scaler/unified_scaler.hh>
#include <scaler/sdl/sdl_image.hh>
#include <scaler/image_base.hh>
#include <vector>

// Basic image implementation for testing
template<typename PixelType>
class BasicInputImage : public scaler::input_image_base<BasicInputImage<PixelType>, PixelType> {
    std::vector<std::vector<PixelType>> data_;
public:
    BasicInputImage(const std::vector<std::vector<PixelType>>& data) : data_(data) {}

    size_t width_impl() const { return data_[0].size(); }
    size_t height_impl() const { return data_.size(); }
    PixelType get_pixel_impl(size_t x, size_t y) const { return data_[y][x]; }
};

// Combined input/output image for intermediate results
template<typename PixelType>
class BasicIOImage : public scaler::input_image_base<BasicIOImage<PixelType>, PixelType>,
                     public scaler::output_image_base<BasicIOImage<PixelType>, PixelType> {
    std::vector<std::vector<PixelType>> data_;
public:
    BasicIOImage(size_t width, size_t height) {
        data_.resize(height, std::vector<PixelType>(width));
    }

    template<typename T>
    BasicIOImage(size_t width, size_t height, const T&) : BasicIOImage(width, height) {}

    // Disambiguate width() and height() by hiding base class versions
    using scaler::input_image_base<BasicIOImage<PixelType>, PixelType>::width;
    using scaler::input_image_base<BasicIOImage<PixelType>, PixelType>::height;

    size_t width_impl() const { return data_.empty() ? 0 : data_[0].size(); }
    size_t height_impl() const { return data_.size(); }

    PixelType get_pixel_impl(size_t x, size_t y) const { return data_[y][x]; }

    void set_pixel_impl(size_t x, size_t y, const PixelType& pixel) {
        data_[y][x] = pixel;
    }

    // Make sure get_pixel and safeAccess are available
    using scaler::input_image_base<BasicIOImage<PixelType>, PixelType>::get_pixel;
    using scaler::input_image_base<BasicIOImage<PixelType>, PixelType>::safe_access;
};

template<typename T>
using BasicImage = BasicIOImage<T>;

using namespace scaler;

TEST_CASE("Unified Scaler Interface") {
    // Create a simple test image
    std::vector<std::vector<uvec3>> data = {
        {{255, 0, 0}, {0, 255, 0}},
        {{0, 0, 255}, {255, 255, 0}}
    };
    BasicInputImage<uvec3> input(data);

    SUBCASE("Basic scaling works") {
        // Test EPX at 2x
        auto output = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>::scale(input, algorithm::EPX, 2.0f);
        CHECK(output.width() == 4);
        CHECK(output.height() == 4);
    }

    SUBCASE("Exception thrown for unsupported scale") {
        // EPX only supports 2x
        using TestScaler = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>;

        CHECK_THROWS_AS(
            TestScaler::scale(input, algorithm::EPX, 3.0f),
            unsupported_scale_exception
        );

        try {
            TestScaler::scale(input, algorithm::EPX, 3.0f);
        } catch (const unsupported_scale_exception& e) {
            CHECK(e.m_algorithm == algorithm::EPX);
            CHECK(e.m_requested_scale == 3.0f);
            CHECK(e.m_supported_scales == std::vector<float>{2.0f});
        }
    }

    SUBCASE("algorithm capabilities queries") {
        // Check EPX capabilities
        CHECK(scaler_capabilities::get_algorithm_name(algorithm::EPX) == "EPX");
        CHECK(scaler_capabilities::get_supported_scales(algorithm::EPX) == std::vector<float>{2.0f});
        CHECK(scaler_capabilities::supports_arbitrary_scale(algorithm::EPX) == false);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::EPX, 2.0f) == true);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::EPX, 3.0f) == false);

        // Check OmniScale capabilities
        CHECK(scaler_capabilities::supports_arbitrary_scale(algorithm::OmniScale) == true);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::OmniScale, 2.5f) == true);
        CHECK(scaler_capabilities::is_scale_supported(algorithm::OmniScale, 3.7f) == true);
    }

    SUBCASE("Get algorithms for specific scale") {
        auto algos_2x = scaler_capabilities::get_algorithms_for_scale(2.0f);
        CHECK(std::find(algos_2x.begin(), algos_2x.end(), algorithm::EPX) != algos_2x.end());
        CHECK(std::find(algos_2x.begin(), algos_2x.end(), algorithm::Eagle) != algos_2x.end());
        CHECK(std::find(algos_2x.begin(), algos_2x.end(), algorithm::HQ) != algos_2x.end());

        auto algos_3x = scaler_capabilities::get_algorithms_for_scale(3.0f);
        CHECK(std::find(algos_3x.begin(), algos_3x.end(), algorithm::EPX) == algos_3x.end()); // EPX not in 3x
        CHECK(std::find(algos_3x.begin(), algos_3x.end(), algorithm::Scale) != algos_3x.end());
        CHECK(std::find(algos_3x.begin(), algos_3x.end(), algorithm::HQ) != algos_3x.end());
    }

    SUBCASE("algorithm info queries") {
        // Test HQ info
        CHECK(scaler_capabilities::get_algorithm_name(algorithm::HQ) == "HQ");
        CHECK(scaler_capabilities::get_supported_scales(algorithm::HQ) == std::vector<float>{2.0f, 3.0f, 4.0f});

        // Test Scale info
        CHECK(scaler_capabilities::get_algorithm_name(algorithm::Scale) == "Scale");
        CHECK(scaler_capabilities::get_supported_scales(algorithm::Scale) == std::vector<float>{2.0f, 3.0f, 4.0f});
    }

    SUBCASE("Default scale selection") {
        // Use explicit scale since we removed the default overload
        auto output = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>::scale(input, algorithm::EPX, 2.0f);
        CHECK(output.width() == 4);  // 2x scale
        CHECK(output.height() == 4);
    }

    SUBCASE("Multiple algorithms produce correct output size") {
        struct TestCase {
            algorithm algo;
            float scale;
            size_t expected_width;
            size_t expected_height;
        };

        std::vector<TestCase> test_cases = {
            {algorithm::EPX, 2.0f, 4, 4},
            {algorithm::Eagle, 2.0f, 4, 4},
            {algorithm::Scale, 2.0f, 4, 4},
            {algorithm::Scale, 3.0f, 6, 6},
            {algorithm::HQ, 2.0f, 4, 4},
            {algorithm::HQ, 3.0f, 6, 6},
            {algorithm::OmniScale, 2.5f, 5, 5},
            {algorithm::Nearest, 3.5f, 7, 7},
        };

        for (const auto& tc : test_cases) {
            INFO("Testing algorithm: " << scaler_capabilities::get_algorithm_name(tc.algo)
                 << " at scale " << tc.scale);

            auto output = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>::scale(
                input, tc.algo, tc.scale
            );
            CHECK(output.width() == tc.expected_width);
            CHECK(output.height() == tc.expected_height);
        }
    }
}

TEST_CASE("Nearest neighbor implementation") {
    // Create a 2x2 test pattern
    std::vector<std::vector<uvec3>> data = {
        {{255, 0, 0}, {0, 255, 0}},   // red, green
        {{0, 0, 255}, {255, 255, 255}} // blue, white
    };
    BasicInputImage<uvec3> input(data);

    SUBCASE("2x scaling") {
        auto output = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>::scale(
            input, algorithm::Nearest, 2.0f
        );

        // Check that each pixel is duplicated correctly
        CHECK(output.get_pixel(0, 0) == uvec3{255, 0, 0}); // red
        CHECK(output.get_pixel(1, 0) == uvec3{255, 0, 0}); // red
        CHECK(output.get_pixel(2, 0) == uvec3{0, 255, 0}); // green
        CHECK(output.get_pixel(3, 0) == uvec3{0, 255, 0}); // green

        CHECK(output.get_pixel(0, 2) == uvec3{0, 0, 255}); // blue
        CHECK(output.get_pixel(1, 2) == uvec3{0, 0, 255}); // blue
        CHECK(output.get_pixel(2, 2) == uvec3{255, 255, 255}); // white
        CHECK(output.get_pixel(3, 2) == uvec3{255, 255, 255}); // white
    }

    SUBCASE("Non-integer scaling") {
        auto output = Scaler<BasicInputImage<uvec3>, BasicImage<uvec3>>::scale(
            input, algorithm::Nearest, 1.5f
        );

        CHECK(output.width() == 3);
        CHECK(output.height() == 3);

        // Center pixel should be from the overlap region
        CHECK((output.get_pixel(1, 1) == uvec3{255, 0, 0} ||
               output.get_pixel(1, 1) == uvec3{0, 255, 0} ||
               output.get_pixel(1, 1) == uvec3{0, 0, 255} ||
               output.get_pixel(1, 1) == uvec3{255, 255, 255}));
    }
}