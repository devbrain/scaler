#include <doctest/doctest.h>
#include <scaler/image_base.hh>
#include <scaler/epx_crtp.hh>
#include <scaler/eagle_crtp.hh>
#include <scaler/2xsai_crtp.hh>
#include <vector>
#include <array>
#include <cmath>

// Test image implementation with additional utilities
class TestImage : public InputImageBase<TestImage, uvec3>,
                  public OutputImageBase<TestImage, uvec3> {
public:
    TestImage(int w, int h) 
        : m_width(w), m_height(h), m_data(w * h, {0, 0, 0}) {}
    
    TestImage(int w, int h, const TestImage&)
        : TestImage(w, h) {}
    
    // Resolve ambiguity
    using InputImageBase<TestImage, uvec3>::width;
    using InputImageBase<TestImage, uvec3>::height;
    using InputImageBase<TestImage, uvec3>::get_pixel;
    using InputImageBase<TestImage, uvec3>::safeAccess;
    using OutputImageBase<TestImage, uvec3>::set_pixel;
    
    [[nodiscard]] int width_impl() const { return m_width; }
    [[nodiscard]] int height_impl() const { return m_height; }
    
    [[nodiscard]] uvec3 get_pixel_impl(int x, int y) const {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
            return m_data[y * m_width + x];
        }
        return {0, 0, 0};
    }
    
    void set_pixel_impl(int x, int y, const uvec3& pixel) {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
            m_data[y * m_width + x] = pixel;
        }
    }
    
    // Utility functions for testing
    void fill(const uvec3& color) {
        std::fill(m_data.begin(), m_data.end(), color);
    }
    
    void fillPattern(const std::vector<std::vector<uvec3>>& pattern) {
        for (int y = 0; y < std::min(static_cast<int>(pattern.size()), m_height); ++y) {
            for (int x = 0; x < std::min(static_cast<int>(pattern[y].size()), m_width); ++x) {
                set_pixel(x, y, pattern[y][x]);
            }
        }
    }
    
    [[nodiscard]] bool compareRegion(int x, int y, int w, int h, const uvec3& expectedColor) const {
        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                auto pixel = get_pixel(x + dx, y + dy);
                if (pixel.x != expectedColor.x || 
                    pixel.y != expectedColor.y || 
                    pixel.z != expectedColor.z) {
                    return false;
                }
            }
        }
        return true;
    }
    
    [[nodiscard]] size_t countPixelsOfColor(const uvec3& color) const {
        return std::count(m_data.begin(), m_data.end(), color);
    }
    
private:
    int m_width, m_height;
    std::vector<uvec3> m_data;
};

// Color constants for testing
namespace Colors {
    const uvec3 BLACK{0, 0, 0};
    const uvec3 WHITE{255, 255, 255};
    const uvec3 RED{255, 0, 0};
    const uvec3 GREEN{0, 255, 0};
    const uvec3 BLUE{0, 0, 255};
    const uvec3 YELLOW{255, 255, 0};
    const uvec3 CYAN{0, 255, 255};
    const uvec3 MAGENTA{255, 0, 255};
    const uvec3 GRAY{128, 128, 128};
}

TEST_CASE("EPX Scaler Comprehensive Tests") {
    SUBCASE("Solid color preservation") {
        TestImage input(4, 4);
        input.fill(Colors::RED);
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        CHECK(output.countPixelsOfColor(Colors::RED) == 64);
    }
    
    SUBCASE("Checkerboard pattern") {
        TestImage input(4, 4);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                input.set_pixel(x, y, ((x + y) % 2 == 0) ? Colors::BLACK : Colors::WHITE);
            }
        }
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // EPX should preserve the checkerboard structure
        CHECK(output.countPixelsOfColor(Colors::BLACK) == 32);
        CHECK(output.countPixelsOfColor(Colors::WHITE) == 32);
    }
    
    SUBCASE("Diagonal line preservation") {
        TestImage input(5, 5);
        input.fill(Colors::WHITE);
        // Create diagonal line
        for (int i = 0; i < 5; ++i) {
            input.set_pixel(i, i, Colors::BLACK);
        }
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 10);
        CHECK(output.height() == 10);
        // Check that diagonal is preserved (at least one pixel in each 2x2 block)
        for (int i = 0; i < 5; ++i) {
            bool has_black = (output.get_pixel(i * 2, i * 2) == Colors::BLACK) ||
                            (output.get_pixel(i * 2 + 1, i * 2) == Colors::BLACK) ||
                            (output.get_pixel(i * 2, i * 2 + 1) == Colors::BLACK) ||
                            (output.get_pixel(i * 2 + 1, i * 2 + 1) == Colors::BLACK);
            CHECK(has_black);
        }
    }
    
    SUBCASE("Corner detection") {
        TestImage input(3, 3);
        input.fillPattern({
            {Colors::BLACK, Colors::BLACK, Colors::WHITE},
            {Colors::BLACK, Colors::BLACK, Colors::WHITE},
            {Colors::WHITE, Colors::WHITE, Colors::WHITE}
        });
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        // EPX should smooth the corner appropriately
        CHECK(output.get_pixel(0, 0) == Colors::BLACK);
        CHECK(output.get_pixel(5, 5) == Colors::WHITE);
    }
    
    SUBCASE("Single pixel input") {
        TestImage input(1, 1);
        input.set_pixel(0, 0, Colors::MAGENTA);
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 2);
        CHECK(output.height() == 2);
        CHECK(output.countPixelsOfColor(Colors::MAGENTA) == 4);
    }
    
    SUBCASE("Empty/zero size handling") {
        TestImage input(0, 0);
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 0);
        CHECK(output.height() == 0);
    }
}

TEST_CASE("AdvMAME Scaler Comprehensive Tests") {
    SUBCASE("Solid color preservation") {
        TestImage input(3, 3);
        input.fill(Colors::BLUE);
        
        auto output = scaleAdvMame<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        CHECK(output.countPixelsOfColor(Colors::BLUE) == 36);
    }
    
    SUBCASE("Vertical line pattern") {
        TestImage input(4, 4);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                input.set_pixel(x, y, (x % 2 == 0) ? Colors::GREEN : Colors::RED);
            }
        }
        
        auto output = scaleAdvMame<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // AdvMAME should preserve vertical lines
        for (int y = 0; y < 8; ++y) {
            CHECK(output.get_pixel(0, y) == Colors::GREEN);
            CHECK(output.get_pixel(2, y) == Colors::RED);
            CHECK(output.get_pixel(4, y) == Colors::GREEN);
            CHECK(output.get_pixel(6, y) == Colors::RED);
        }
    }
    
    SUBCASE("Horizontal line pattern") {
        TestImage input(4, 4);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                input.set_pixel(x, y, (y % 2 == 0) ? Colors::CYAN : Colors::YELLOW);
            }
        }
        
        auto output = scaleAdvMame<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // AdvMAME should preserve horizontal lines
        for (int x = 0; x < 8; ++x) {
            CHECK(output.get_pixel(x, 0) == Colors::CYAN);
            CHECK(output.get_pixel(x, 2) == Colors::YELLOW);
            CHECK(output.get_pixel(x, 4) == Colors::CYAN);
            CHECK(output.get_pixel(x, 6) == Colors::YELLOW);
        }
    }
}

TEST_CASE("Eagle Scaler Comprehensive Tests") {
    SUBCASE("Solid color preservation") {
        TestImage input(3, 3);
        input.fill(Colors::GRAY);
        
        auto output = scaleEagle<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        CHECK(output.countPixelsOfColor(Colors::GRAY) == 36);
    }
    
    SUBCASE("Cross pattern") {
        TestImage input(3, 3);
        input.fillPattern({
            {Colors::WHITE, Colors::BLACK, Colors::WHITE},
            {Colors::BLACK, Colors::BLACK, Colors::BLACK},
            {Colors::WHITE, Colors::BLACK, Colors::WHITE}
        });
        
        auto output = scaleEagle<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        // Eagle should preserve the cross pattern
        CHECK(output.get_pixel(2, 0) == Colors::BLACK);
        CHECK(output.get_pixel(3, 0) == Colors::BLACK);
        CHECK(output.get_pixel(2, 2) == Colors::BLACK);
        CHECK(output.get_pixel(3, 2) == Colors::BLACK);
    }
    
    SUBCASE("Corner smoothing") {
        TestImage input(3, 3);
        input.fillPattern({
            {Colors::RED, Colors::RED, Colors::BLUE},
            {Colors::RED, Colors::RED, Colors::BLUE},
            {Colors::GREEN, Colors::GREEN, Colors::GREEN}
        });
        
        auto output = scaleEagle<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        // Check that colors are properly distributed
        CHECK(output.countPixelsOfColor(Colors::RED) >= 8);
        CHECK(output.countPixelsOfColor(Colors::BLUE) >= 4);
        CHECK(output.countPixelsOfColor(Colors::GREEN) >= 6);
    }
    
    SUBCASE("Complex pattern - L shape") {
        TestImage input(4, 4);
        input.fillPattern({
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE},
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE},
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE},
            {Colors::BLACK, Colors::BLACK, Colors::BLACK, Colors::BLACK}
        });
        
        auto output = scaleEagle<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // Verify L-shape is preserved - check key positions
        CHECK(output.get_pixel(0, 0) == Colors::BLACK);
        CHECK(output.get_pixel(0, 2) == Colors::BLACK);
        CHECK(output.get_pixel(0, 4) == Colors::BLACK);
        CHECK(output.get_pixel(0, 6) == Colors::BLACK);
        CHECK(output.get_pixel(2, 6) == Colors::BLACK);
        // Check that bottom row has substantial black pixels
        int black_count = 0;
        for (int x = 0; x < 8; ++x) {
            if (output.get_pixel(x, 6) == Colors::BLACK) black_count++;
        }
        CHECK(black_count >= 3); // At least 3 black pixels in bottom row
    }
}

TEST_CASE("2xSaI Scaler Comprehensive Tests") {
    SUBCASE("Solid color preservation") {
        TestImage input(4, 4);
        input.fill(Colors::MAGENTA);
        
        auto output = scale2xSaI<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        CHECK(output.countPixelsOfColor(Colors::MAGENTA) == 64);
    }
    
    SUBCASE("Gradient pattern") {
        TestImage input(4, 4);
        // Create a simple gradient
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                unsigned int val = (x + y) * 36;
                input.set_pixel(x, y, {val, val, val});
            }
        }
        
        auto output = scale2xSaI<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // 2xSaI should create smooth transitions
        // Check that no extreme jumps occur
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 7; ++x) {
                auto current = output.get_pixel(x, y);
                auto next_x = output.get_pixel(x + 1, y);
                auto next_y = output.get_pixel(x, y + 1);
                
                // Values shouldn't jump by more than original gradient step
                int dx = abs(static_cast<int>(next_x.x) - static_cast<int>(current.x));
                int dy = abs(static_cast<int>(next_y.x) - static_cast<int>(current.x));
                CHECK(dx <= 72);
                CHECK(dy <= 72);
            }
        }
    }
    
    SUBCASE("Edge detection - sharp edge") {
        TestImage input(4, 4);
        // Left half black, right half white
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                input.set_pixel(x, y, (x < 2) ? Colors::BLACK : Colors::WHITE);
            }
        }
        
        auto output = scale2xSaI<TestImage, TestImage>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // Edge should be preserved
        for (int y = 0; y < 8; ++y) {
            CHECK(output.get_pixel(0, y) == Colors::BLACK);
            CHECK(output.get_pixel(1, y) == Colors::BLACK);
            CHECK(output.get_pixel(2, y) == Colors::BLACK);
            CHECK(output.get_pixel(7, y) == Colors::WHITE);
            CHECK(output.get_pixel(6, y) == Colors::WHITE);
        }
    }
    
    SUBCASE("Anti-aliased diagonal") {
        TestImage input(5, 5);
        input.fill(Colors::BLACK);
        // Create anti-aliased diagonal
        for (int i = 0; i < 5; ++i) {
            input.set_pixel(i, i, Colors::WHITE);
            if (i > 0) input.set_pixel(i - 1, i, Colors::GRAY);
            if (i < 4) input.set_pixel(i + 1, i, Colors::GRAY);
        }
        
        auto output = scale2xSaI<TestImage, TestImage>(input);
        
        CHECK(output.width() == 10);
        CHECK(output.height() == 10);
        // 2xSaI should smooth the anti-aliased diagonal
        CHECK(output.countPixelsOfColor(Colors::WHITE) > 0);
        CHECK(output.countPixelsOfColor(Colors::GRAY) > 0);
        CHECK(output.countPixelsOfColor(Colors::BLACK) > 0);
    }
}

TEST_CASE("Algorithm Comparison Tests") {
    SUBCASE("Compare scaling algorithms on same input") {
        TestImage input(3, 3);
        input.fillPattern({
            {Colors::RED, Colors::GREEN, Colors::BLUE},
            {Colors::YELLOW, Colors::CYAN, Colors::MAGENTA},
            {Colors::WHITE, Colors::GRAY, Colors::BLACK}
        });
        
        auto epx_output = scaleEpx<TestImage, TestImage>(input);
        auto eagle_output = scaleEagle<TestImage, TestImage>(input);
        auto advmame_output = scaleAdvMame<TestImage, TestImage>(input);
        auto sai_output = scale2xSaI<TestImage, TestImage>(input);
        
        // All should produce same dimensions
        CHECK(epx_output.width() == 6);
        CHECK(eagle_output.width() == 6);
        CHECK(advmame_output.width() == 6);
        CHECK(sai_output.width() == 6);
        
        CHECK(epx_output.height() == 6);
        CHECK(eagle_output.height() == 6);
        CHECK(advmame_output.height() == 6);
        CHECK(sai_output.height() == 6);
        
        // Center pixel should generally be preserved
        auto center_color = Colors::CYAN;
        CHECK(epx_output.get_pixel(2, 2) == center_color);
        CHECK(eagle_output.get_pixel(2, 2) == center_color);
        CHECK(advmame_output.get_pixel(2, 2) == center_color);
        CHECK(sai_output.get_pixel(2, 2) == center_color);
    }
}

TEST_CASE("Edge Cases and Boundary Tests") {
    SUBCASE("Non-square input") {
        TestImage input(5, 3);
        input.fill(Colors::BLUE);
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 10);
        CHECK(output.height() == 6);
        CHECK(output.countPixelsOfColor(Colors::BLUE) == 60);
    }
    
    SUBCASE("Very small input - 2x1") {
        TestImage input(2, 1);
        input.set_pixel(0, 0, Colors::RED);
        input.set_pixel(1, 0, Colors::GREEN);
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 4);
        CHECK(output.height() == 2);
        CHECK(output.countPixelsOfColor(Colors::RED) >= 2);
        CHECK(output.countPixelsOfColor(Colors::GREEN) >= 2);
    }
    
    SUBCASE("Large uniform areas") {
        TestImage input(10, 10);
        // Create large uniform areas
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) {
                if (x < 5 && y < 5) {
                    input.set_pixel(x, y, Colors::RED);
                } else if (x >= 5 && y < 5) {
                    input.set_pixel(x, y, Colors::GREEN);
                } else if (x < 5 && y >= 5) {
                    input.set_pixel(x, y, Colors::BLUE);
                } else {
                    input.set_pixel(x, y, Colors::YELLOW);
                }
            }
        }
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 20);
        CHECK(output.height() == 20);
        
        // Check that quadrants are preserved
        CHECK(output.compareRegion(0, 0, 8, 8, Colors::RED));
        CHECK(output.compareRegion(12, 0, 8, 8, Colors::GREEN));
        CHECK(output.compareRegion(0, 12, 8, 8, Colors::BLUE));
        CHECK(output.compareRegion(12, 12, 8, 8, Colors::YELLOW));
    }
}

TEST_CASE("Pattern Recognition Tests") {
    SUBCASE("Detect and preserve circles") {
        TestImage input(5, 5);
        // Create a simple circle pattern
        input.fillPattern({
            {Colors::WHITE, Colors::BLACK, Colors::BLACK, Colors::BLACK, Colors::WHITE},
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE, Colors::BLACK},
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE, Colors::BLACK},
            {Colors::BLACK, Colors::WHITE, Colors::WHITE, Colors::WHITE, Colors::BLACK},
            {Colors::WHITE, Colors::BLACK, Colors::BLACK, Colors::BLACK, Colors::WHITE}
        });
        
        auto output = scaleEpx<TestImage, TestImage>(input);
        
        CHECK(output.width() == 10);
        CHECK(output.height() == 10);
        // Circle structure should be recognizable - check for black pixels near corners
        bool has_black_top = (output.get_pixel(2, 0) == Colors::BLACK) || 
                            (output.get_pixel(3, 0) == Colors::BLACK) ||
                            (output.get_pixel(4, 0) == Colors::BLACK);
        bool has_black_left = (output.get_pixel(0, 2) == Colors::BLACK) || 
                             (output.get_pixel(0, 3) == Colors::BLACK) ||
                             (output.get_pixel(0, 4) == Colors::BLACK);
        CHECK(has_black_top);
        CHECK(has_black_left);
    }
    
    SUBCASE("Text-like patterns") {
        TestImage input(3, 5);
        // Create an 'I' shape
        input.fillPattern({
            {Colors::BLACK, Colors::BLACK, Colors::BLACK},
            {Colors::WHITE, Colors::BLACK, Colors::WHITE},
            {Colors::WHITE, Colors::BLACK, Colors::WHITE},
            {Colors::WHITE, Colors::BLACK, Colors::WHITE},
            {Colors::BLACK, Colors::BLACK, Colors::BLACK}
        });
        
        auto output = scaleEagle<TestImage, TestImage>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 10);
        // The 'I' shape should be preserved
        for (int y = 0; y < 10; ++y) {
            bool has_black = (output.get_pixel(2, y) == Colors::BLACK) || 
                            (output.get_pixel(3, y) == Colors::BLACK);
            CHECK(has_black);
        }
    }
}