#include <doctest/doctest.h>
#include <scaler/sliding_window_buffer.hh>
#include <scaler/vec3.hh>
#include <type_traits>

using namespace scaler;
// Mock image accessor for testing
template<typename T>
class TestImageAccessor {
private:
    std::vector<std::vector<T>> data_;
    int width_;
    int height_;
    T boundary_value_;
    
public:
    TestImageAccessor(int width, int height, T boundary = T{})
        : width_(width), height_(height), boundary_value_(boundary) {
        data_.resize(static_cast<size_t>(height));
        for (int y = 0; y < height; ++y) {
            data_[static_cast<size_t>(y)].resize(static_cast<size_t>(width));
            for (int x = 0; x < width; ++x) {
                // Fill with test pattern: value = y * 100 + x
                if constexpr (std::is_integral_v<T>) {
                    data_[static_cast<size_t>(y)][static_cast<size_t>(x)] = T(y * 100 + x);
                } else {
                    // For vec3 or other complex types
                    auto val = static_cast<unsigned int>(y * 100 + x);
                    data_[static_cast<size_t>(y)][static_cast<size_t>(x)] = T{val, val, val};
                }
            }
        }
    }
    
    T safeAccess(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return boundary_value_;
        }
        return data_[static_cast<size_t>(y)][static_cast<size_t>(x)];
    }
    
    int width() const { return width_; }
    int height() const { return height_; }
};

TEST_CASE("SlidingWindowBuffer basic functionality") {
    using PixelType = int;
    TestImageAccessor<PixelType> image(10, 10, -1);
    
    SUBCASE("3x3 window initialization and access") {
        SlidingWindowBuffer<PixelType> buffer(3, 10, 1, -1);
        buffer.initialize(image, 0);
        
        // At y=0, x=0, the buffer has padding=1, so:
        // Buffer internally stores: [-1, 0, 1, 2, ...] for row 0
        // Buffer internally stores: [-1, 100, 101, 102, ...] for row 1
        // The get() function takes x without padding (0-based)
        
        // Access with x=0 actually accesses buffer position 1 (due to padding)
        CHECK(buffer.get(0, -1) == -1);   // top (row -1 is out of bounds)
        CHECK(buffer.get(1, -1) == -1);   // top-right (row -1 is out of bounds)
        
        CHECK(buffer.get(0, 0) == 0);     // x=0 at current row
        CHECK(buffer.get(1, 0) == 1);     // x=1 at current row
        
        CHECK(buffer.get(0, 1) == 100);   // x=0 at next row
        CHECK(buffer.get(1, 1) == 101);   // x=1 at next row
        
        // Note: To access padded values (x=-1), the buffer handles this internally
        // through safeAccess when loading rows
    }
    
    SUBCASE("Sliding window advancement") {
        SlidingWindowBuffer<PixelType> buffer(3, 10, 1, -1);
        buffer.initialize(image, 0);
        
        // Advance to y=1
        buffer.advance(image);
        
        // At y=1, x=1, we should get:
        // 0,   1,   2  (row 0)
        // 100, 101, 102 (row 1)
        // 200, 201, 202 (row 2)
        
        CHECK(buffer.get(0, -1) == 0);    // top-left
        CHECK(buffer.get(1, -1) == 1);    // top
        CHECK(buffer.get(2, -1) == 2);    // top-right
        
        CHECK(buffer.get(0, 0) == 100);   // left
        CHECK(buffer.get(1, 0) == 101);   // center
        CHECK(buffer.get(2, 0) == 102);   // right
        
        CHECK(buffer.get(0, 1) == 200);   // bottom-left
        CHECK(buffer.get(1, 1) == 201);   // bottom
        CHECK(buffer.get(2, 1) == 202);   // bottom-right
    }
    
    SUBCASE("Multiple advances") {
        SlidingWindowBuffer<PixelType> buffer(3, 10, 1, -1);
        buffer.initialize(image, 0);
        
        // Advance to y=5
        for (int i = 0; i < 5; ++i) {
            buffer.advance(image);
        }
        
        CHECK(buffer.getCurrentY() == 5);
        
        // At y=5, x=3, center pixel should be 503
        CHECK(buffer.get(3, 0) == 503);
        CHECK(buffer.get(3, -1) == 403);  // above
        CHECK(buffer.get(3, 1) == 603);   // below
    }
}

TEST_CASE("SlidingWindow3x3 specialized class") {
    using PixelType = int;
    TestImageAccessor<PixelType> image(10, 10, -1);
    
    SlidingWindow3x3<PixelType> window(10);
    window.initialize(image, 1);
    
    // At y=1, x=1
    CHECK(window.getTopLeft(1) == 0);
    CHECK(window.getTop(1) == 1);
    CHECK(window.getTopRight(1) == 2);
    CHECK(window.getLeft(1) == 100);
    CHECK(window.getCenter(1) == 101);
    CHECK(window.getRight(1) == 102);
    CHECK(window.getBottomLeft(1) == 200);
    CHECK(window.getBottom(1) == 201);
    CHECK(window.getBottomRight(1) == 202);
}

TEST_CASE("SlidingWindow5x5 specialized class") {
    using PixelType = int;
    TestImageAccessor<PixelType> image(10, 10, -1);
    
    SlidingWindow5x5<PixelType> window(10);
    window.initialize(image, 2);
    
    // At y=2, x=2, get 5x5 neighborhood
    PixelType neighborhood[5][5];
    window.getNeighborhood(2, neighborhood);
    
    // Check the center 3x3
    CHECK(neighborhood[1][1] == 101);  // (-1, -1) relative to (2,2)
    CHECK(neighborhood[1][2] == 102);  // (0, -1)
    CHECK(neighborhood[1][3] == 103);  // (1, -1)
    
    CHECK(neighborhood[2][1] == 201);  // (-1, 0)
    CHECK(neighborhood[2][2] == 202);  // (0, 0) - center
    CHECK(neighborhood[2][3] == 203);  // (1, 0)
    
    CHECK(neighborhood[3][1] == 301);  // (-1, 1)
    CHECK(neighborhood[3][2] == 302);  // (0, 1)
    CHECK(neighborhood[3][3] == 303);  // (1, 1)
}

TEST_CASE("Buffer circular indexing stress test") {
    using PixelType = int;
    TestImageAccessor<PixelType> image(100, 100, -1);
    
    SUBCASE("Many advances maintain correct indexing") {
        SlidingWindowBuffer<PixelType> buffer(3, 100, 1, -1);
        buffer.initialize(image, 0);
        
        // Advance through entire image
        for (int y = 1; y < 100; ++y) {
            buffer.advance(image);
            
            // Verify center pixel is correct
            int expected = y * 100 + 50;  // pixel at (50, y)
            CHECK(buffer.get(50, 0) == expected);
            
            // Verify above and below are correct
            if (y > 0) {
                CHECK(buffer.get(50, -1) == expected - 100);
            }
            if (y < 99) {
                CHECK(buffer.get(50, 1) == expected + 100);
            }
        }
    }
}

TEST_CASE("Buffer with vec3 pixels") {
    using PixelType = vec3<unsigned int>;
    TestImageAccessor<PixelType> image(5, 5, PixelType{0, 0, 0});
    
    SlidingWindow3x3<PixelType> window(5);
    window.initialize(image, 1);
    
    // The test pattern creates vec3{y*100+x, y*100+x, y*100+x}
    auto center = window.getCenter(1);
    CHECK(center.x == 101);
    CHECK(center.y == 101);
    CHECK(center.z == 101);
}