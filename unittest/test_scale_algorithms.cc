#include <doctest/doctest.h>
#include <scaler/scale2x_sfx.hh>
#include <scaler/scale3x.hh>
#include <scaler/scale3x_sfx.hh>
#include <scaler/image_base.hh>
#include <vector>
using namespace scaler;
// Simple test image class - separate input and output classes to avoid ambiguity
template<typename PixelType>
class TestInputImage : public InputImageBase<TestInputImage<PixelType>, PixelType> {
private:
    int width_;
    int height_;
    std::vector<PixelType> data_;
    
public:
    TestInputImage(int w, int h) : width_(w), height_(h), data_(static_cast<size_t>(w * h)) {}
    
    int width_impl() const { return width_; }
    int height_impl() const { return height_; }
    
    PixelType get_pixel_impl(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return data_[0]; // Return first pixel for out of bounds
        }
        return data_[static_cast<size_t>(y * width_ + x)];
    }
    
    PixelType safeAccess(int x, int y) const {
        return get_pixel_impl(x, y);
    }
    
    void setData(int x, int y, PixelType pixel) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            data_[static_cast<size_t>(y * width_ + x)] = pixel;
        }
    }
};

template<typename PixelType>
class TestOutputImage : public OutputImageBase<TestOutputImage<PixelType>, PixelType> {
private:
    int width_;
    int height_;
    std::vector<PixelType> data_;
    
public:
    TestOutputImage(int w, int h) : width_(w), height_(h), data_(static_cast<size_t>(w * h)) {}
    
    template<typename OtherImage>
    TestOutputImage(int w, int h, const OtherImage&) : width_(w), height_(h), data_(static_cast<size_t>(w * h)) {}
    
    int width_impl() const { return width_; }
    int height_impl() const { return height_; }
    
    PixelType get_pixel_impl(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return PixelType{};
        }
        return data_[static_cast<size_t>(y * width_ + x)];
    }
    
    void set_pixel_impl(int x, int y, PixelType pixel) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            data_[static_cast<size_t>(y * width_ + x)] = pixel;
        }
    }
    
    PixelType get_pixel(int x, int y) const {
        return get_pixel_impl(x, y);
    }
};

using TestInputVec3 = TestInputImage<vec3<unsigned int>>;
using TestOutputVec3 = TestOutputImage<vec3<unsigned int>>;

TEST_CASE("Scale2xSFX algorithm basic test") {
    // Create a small test image
    TestInputVec3 input(4, 4);
    
    // Create a simple pattern
    input.setData(0, 0, vec3<unsigned int>(255, 0, 0)); // Red
    input.setData(1, 0, vec3<unsigned int>(0, 255, 0)); // Green
    input.setData(1, 1, vec3<unsigned int>(0, 0, 255)); // Blue
    
    // Test Scale2xSFX
    auto result = scaleScale2xSFX<TestInputVec3, TestOutputVec3>(input, 2);
    
    CHECK(result.width() == 8);
    CHECK(result.height() == 8);
    
    // Basic sanity check - corners should exist
    auto tl = result.get_pixel(0, 0);
    auto br = result.get_pixel(7, 7);
    CHECK((tl.x > 0 || tl.y > 0 || tl.z > 0));  // Changed from >= 0 since unsigned is always >= 0
    CHECK((br.x > 0 || br.y > 0 || br.z > 0));  // Check if any component is non-zero
}

TEST_CASE("Scale3x algorithm basic test") {
    // Create a small test image
    TestInputVec3 input(4, 4);
    
    // Create a simple pattern
    input.setData(0, 0, vec3<unsigned int>(255, 0, 0)); // Red
    input.setData(1, 0, vec3<unsigned int>(0, 255, 0)); // Green
    input.setData(1, 1, vec3<unsigned int>(0, 0, 255)); // Blue
    
    // Test Scale3x
    auto result = scaleScale3x<TestInputVec3, TestOutputVec3>(input, 3);
    
    CHECK(result.width() == 12);
    CHECK(result.height() == 12);
    
    // Basic sanity check - corners should exist
    auto tl = result.get_pixel(0, 0);
    auto br = result.get_pixel(11, 11);
    CHECK((tl.x > 0 || tl.y > 0 || tl.z > 0));  // Changed from >= 0 since unsigned is always >= 0
    CHECK((br.x > 0 || br.y > 0 || br.z > 0));  // Check if any component is non-zero
}

TEST_CASE("Scale3xSFX algorithm basic test") {
    // Create a small test image
    TestInputVec3 input(4, 4);
    
    // Create a simple pattern
    input.setData(0, 0, vec3<unsigned int>(255, 0, 0)); // Red
    input.setData(1, 0, vec3<unsigned int>(0, 255, 0)); // Green
    input.setData(1, 1, vec3<unsigned int>(0, 0, 255)); // Blue
    
    // Test Scale3xSFX
    auto result = scaleScale3xSFX<TestInputVec3, TestOutputVec3>(input, 3);
    
    CHECK(result.width() == 12);
    CHECK(result.height() == 12);
    
    // Basic sanity check - corners should exist
    auto tl = result.get_pixel(0, 0);
    auto br = result.get_pixel(11, 11);
    CHECK((tl.x > 0 || tl.y > 0 || tl.z > 0));  // Changed from >= 0 since unsigned is always >= 0
    CHECK((br.x > 0 || br.y > 0 || br.z > 0));  // Check if any component is non-zero
}

TEST_CASE("Scale algorithms preserve single pixel") {
    // Single pixel should be expanded uniformly
    TestInputVec3 input(1, 1);
    input.setData(0, 0, vec3<unsigned int>(128, 64, 192));
    
    SUBCASE("Scale2xSFX") {
        auto result = scaleScale2xSFX<TestInputVec3, TestOutputVec3>(input, 2);
        CHECK(result.width() == 2);
        CHECK(result.height() == 2);
        
        // All pixels should be the same as input
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                auto pixel = result.get_pixel(x, y);
                CHECK(pixel.x == 128);
                CHECK(pixel.y == 64);
                CHECK(pixel.z == 192);
            }
        }
    }
    
    SUBCASE("Scale3x") {
        auto result = scaleScale3x<TestInputVec3, TestOutputVec3>(input, 3);
        CHECK(result.width() == 3);
        CHECK(result.height() == 3);
        
        // All pixels should be the same as input
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                auto pixel = result.get_pixel(x, y);
                CHECK(pixel.x == 128);
                CHECK(pixel.y == 64);
                CHECK(pixel.z == 192);
            }
        }
    }
    
    SUBCASE("Scale3xSFX") {
        auto result = scaleScale3xSFX<TestInputVec3, TestOutputVec3>(input, 3);
        CHECK(result.width() == 3);
        CHECK(result.height() == 3);
        
        // Center pixel should definitely be the same
        auto center = result.get_pixel(1, 1);
        CHECK(center.x == 128);
        CHECK(center.y == 64);
        CHECK(center.z == 192);
    }
}