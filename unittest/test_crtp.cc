#include <doctest/doctest.h>
#include <scaler/image_base.hh>
#include <scaler/epx.hh>
#include <vector>
using namespace scaler;
// Simple test image implementation - combined input/output
class TestImage : public InputImageBase<TestImage, uvec3>,
                  public OutputImageBase<TestImage, uvec3> {
public:
    TestImage(size_t w, size_t h) 
        : m_width(w), m_height(h), m_data(w * h) {}
    
    TestImage(size_t w, size_t h, const TestImage&)
        : TestImage(w, h) {}
    
    // Resolve ambiguity by providing unified implementations
    using InputImageBase<TestImage, uvec3>::width;
    using InputImageBase<TestImage, uvec3>::height;
    using InputImageBase<TestImage, uvec3>::get_pixel;
    using InputImageBase<TestImage, uvec3>::safeAccess;
    using OutputImageBase<TestImage, uvec3>::set_pixel;
    
    [[nodiscard]] size_t width_impl() const { return m_width; }
    [[nodiscard]] size_t height_impl() const { return m_height; }
    
    [[nodiscard]] uvec3 get_pixel_impl(size_t x, size_t y) const {
        if (x < m_width && y < m_height) {
            return m_data[y * m_width + x];
        }
        return {0, 0, 0};
    }
    
    void set_pixel_impl(size_t x, size_t y, const uvec3& pixel) {
        if (x < m_width && y < m_height) {
            m_data[y * m_width + x] = pixel;
        }
    }
    
    [[nodiscard]] const uvec3& at(size_t x, size_t y) const {
        return m_data[y * m_width + x];
    }
    
private:
    size_t m_width, m_height;
    std::vector<uvec3> m_data;
};

TEST_CASE("CRTP Image Base Classes") {
    SUBCASE("Basic image operations") {
        TestImage img(10, 10);
        
        CHECK(img.width() == 10);
        CHECK(img.height() == 10);
        
        uvec3 test_pixel{255, 128, 64};
        img.set_pixel(5, 5, test_pixel);
        
        auto retrieved = img.get_pixel(5, 5);
        CHECK(retrieved.x == test_pixel.x);
        CHECK(retrieved.y == test_pixel.y);
        CHECK(retrieved.z == test_pixel.z);
    }
    
    SUBCASE("Safe access with boundaries") {
        TestImage img(5, 5);
        
        // Set a known pixel
        uvec3 center_pixel{100, 100, 100};
        img.set_pixel(2, 2, center_pixel);
        
        // Test out of bounds with ZERO strategy
        auto zero_result = img.safeAccess(-1, -1, ZERO);
        CHECK(zero_result.x == 0);
        CHECK(zero_result.y == 0);
        CHECK(zero_result.z == 0);
        
        // Test out of bounds with NEAREST strategy
        auto nearest_result = img.safeAccess(-1, 2, NEAREST);
        CHECK(nearest_result.x == 0); // Should get pixel at (0, 2)
        
        // Test in bounds access
        auto valid_result = img.safeAccess(2, 2);
        CHECK(valid_result.x == center_pixel.x);
        CHECK(valid_result.y == center_pixel.y);
        CHECK(valid_result.z == center_pixel.z);
    }
}

TEST_CASE("EPX Scaling with CRTP") {
    SUBCASE("2x2 to 4x4 scaling") {
        TestImage input(2, 2);
        
        // Create a simple pattern
        uvec3 white{255, 255, 255};
        uvec3 black{0, 0, 0};
        
        input.set_pixel(0, 0, white);
        input.set_pixel(1, 0, black);
        input.set_pixel(0, 1, black);
        input.set_pixel(1, 1, white);
        
        // Scale using EPX
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        // Check dimensions
        CHECK(output.width() == 4);
        CHECK(output.height() == 4);
        
        // Verify some pixels (EPX should preserve diagonal patterns)
        auto top_left = output.get_pixel(0, 0);
        CHECK(top_left.x == white.x);
        
        auto bottom_right = output.get_pixel(3, 3);
        CHECK(bottom_right.x == white.x);
    }
    
    SUBCASE("Single pixel scaling") {
        TestImage input(1, 1);
        uvec3 red{255, 0, 0};
        input.set_pixel(0, 0, red);
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 2);
        CHECK(output.height() == 2);
        
        // All pixels should be the same color
        for (size_t y = 0; y < 2; ++y) {
            for (size_t x = 0; x < 2; ++x) {
                auto pixel = output.get_pixel(x, y);
                CHECK(pixel.x == red.x);
                CHECK(pixel.y == red.y);
                CHECK(pixel.z == red.z);
            }
        }
    }
}