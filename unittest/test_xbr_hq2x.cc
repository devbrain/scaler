#include <doctest/doctest.h>
#include <scaler/image_base.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>
#include <scaler/hq3x.hh>
#include <vector>
using namespace scaler;
// Test image implementation for XBR/HQ2x tests
class TestImageXBR : public InputImageBase<TestImageXBR, uvec3>,
                     public OutputImageBase<TestImageXBR, uvec3> {
public:
    TestImageXBR(size_t w, size_t h) 
        : m_width(w), m_height(h), m_data(w * h, {0, 0, 0}) {}
    
    TestImageXBR(size_t w, size_t h, const TestImageXBR&)
        : TestImageXBR(w, h) {}
    
    // Resolve ambiguity
    using InputImageBase<TestImageXBR, uvec3>::width;
    using InputImageBase<TestImageXBR, uvec3>::height;
    using InputImageBase<TestImageXBR, uvec3>::get_pixel;
    using InputImageBase<TestImageXBR, uvec3>::safeAccess;
    using OutputImageBase<TestImageXBR, uvec3>::set_pixel;
    
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
    
    void fill(const uvec3& color) {
        std::fill(m_data.begin(), m_data.end(), color);
    }
    
    void fillPattern(const std::vector<std::vector<uvec3>>& pattern) {
        for (size_t y = 0; y < std::min(pattern.size(), m_height); ++y) {
            for (size_t x = 0; x < std::min(pattern[y].size(), m_width); ++x) {
                set_pixel(x, y, pattern[y][x]);
            }
        }
    }
    
    [[nodiscard]] size_t countPixelsOfColor(const uvec3& color) const {
        return static_cast<size_t>(std::count(m_data.begin(), m_data.end(), color));
    }
    
private:
    size_t m_width, m_height;
    std::vector<uvec3> m_data;
};

// Color constants
namespace {
    const uvec3 BLACK{0, 0, 0};
    const uvec3 WHITE{255, 255, 255};
    const uvec3 RED{255, 0, 0};
    const uvec3 GREEN{0, 255, 0};
    const uvec3 BLUE{0, 0, 255};
    const uvec3 YELLOW{255, 255, 0};
    const uvec3 GRAY{128, 128, 128};
}

TEST_CASE("XBR Scaler Tests") {
    SUBCASE("Basic functionality - solid color") {
        TestImageXBR input(4, 4);
        input.fill(RED);
        
        auto output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        CHECK(output.countPixelsOfColor(RED) == 64);
    }
    
    SUBCASE("Edge detection - vertical line") {
        TestImageXBR input(4, 4);
        // Create vertical edge
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                input.set_pixel(x, y, (x < 2) ? BLACK : WHITE);
            }
        }
        
        auto output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // XBR should preserve the edge
        CHECK(output.get_pixel(0, 0) == BLACK);
        CHECK(output.get_pixel(7, 0) == WHITE);
    }
    
    SUBCASE("Diagonal edge handling") {
        TestImageXBR input(4, 4);
        // Create diagonal pattern
        input.fillPattern({
            {BLACK, BLACK, WHITE, WHITE},
            {BLACK, BLACK, WHITE, WHITE},
            {WHITE, WHITE, BLACK, BLACK},
            {WHITE, WHITE, BLACK, BLACK}
        });
        
        auto output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // XBR excels at diagonal edges - check corners are preserved
        CHECK(output.get_pixel(0, 0) == BLACK);
        CHECK(output.get_pixel(7, 7) == BLACK);
    }
    
    SUBCASE("Anti-aliasing behavior") {
        TestImageXBR input(3, 3);
        // Create pattern that XBR will anti-alias
        input.fillPattern({
            {WHITE, BLACK, WHITE},
            {BLACK, GRAY, BLACK},
            {WHITE, BLACK, WHITE}
        });
        
        auto output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        // With correct operator!=, XBR anti-aliasing behaves differently
        // The algorithm now correctly identifies edges vs smooth regions
        auto center = output.get_pixel(2, 2);
        // Test updated to match correct XBR behavior - accept interpolated values
        bool center_valid = (center.x >= 85 && center.x <= 170);
        CHECK(center_valid);
    }
    
    SUBCASE("Empty input") {
        TestImageXBR input(0, 0);
        auto output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 0);
        CHECK(output.height() == 0);
    }
}

TEST_CASE("HQ2x Scaler Tests") {
    SUBCASE("Basic functionality - solid color") {
        TestImageXBR input(3, 3);
        input.fill(BLUE);
        
        auto output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        CHECK(output.countPixelsOfColor(BLUE) == 36);
    }
    
    SUBCASE("Pattern preservation - checkerboard") {
        TestImageXBR input(4, 4);
        // Create checkerboard
        for (size_t y = 0; y < 4; ++y) {
            for (size_t x = 0; x < 4; ++x) {
                input.set_pixel(x, y, ((x + y) % 2 == 0) ? BLACK : WHITE);
            }
        }
        
        auto output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        // HQ2x applies complex interpolation, pattern might not be perfectly preserved
        CHECK(output.countPixelsOfColor(BLACK) >= 4);
        CHECK(output.countPixelsOfColor(WHITE) >= 4);
    }
    
    SUBCASE("Complex interpolation rules") {
        TestImageXBR input(3, 3);
        // Create pattern that triggers HQ2x interpolation
        input.fillPattern({
            {RED, GREEN, BLUE},
            {GREEN, YELLOW, GREEN},
            {BLUE, GREEN, RED}
        });
        
        auto output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        // HQ2x may interpolate center pixel
        bool has_yellow = false;
        for (size_t y = 1; y < 4; ++y) {
            for (size_t x = 1; x < 4; ++x) {
                if (output.get_pixel(x, y) == YELLOW) {
                    has_yellow = true;
                    break;
                }
            }
        }
        CHECK(has_yellow);
        // Check that all original colors are present
        CHECK(output.countPixelsOfColor(RED) >= 2);
        CHECK(output.countPixelsOfColor(GREEN) >= 4);
        CHECK(output.countPixelsOfColor(BLUE) >= 2);
        CHECK(output.countPixelsOfColor(YELLOW) >= 1);
    }
    
    SUBCASE("Edge case - single pixel") {
        TestImageXBR input(1, 1);
        input.set_pixel(0, 0, GRAY);
        
        auto output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 2);
        CHECK(output.height() == 2);
        CHECK(output.countPixelsOfColor(GRAY) == 4);
    }
    
    SUBCASE("Gradient smoothing") {
        TestImageXBR input(4, 1);
        // Create horizontal gradient
        input.set_pixel(0, 0, {0, 0, 0});
        input.set_pixel(1, 0, {85, 85, 85});
        input.set_pixel(2, 0, {170, 170, 170});
        input.set_pixel(3, 0, {255, 255, 255});
        
        auto output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 8);
        CHECK(output.height() == 2);
        // HQ2x should create smooth transitions
        CHECK(output.get_pixel(0, 0) == uvec3{0, 0, 0});
        CHECK(output.get_pixel(7, 0) == uvec3{255, 255, 255});
    }
}

TEST_CASE("XBR vs HQ2x Comparison") {
    SUBCASE("Same input different outputs") {
        TestImageXBR input(3, 3);
        // Diagonal line
        input.fillPattern({
            {BLACK, WHITE, WHITE},
            {WHITE, BLACK, WHITE},
            {WHITE, WHITE, BLACK}
        });
        
        auto xbr_output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        auto hq2x_output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        // Both should produce same dimensions
        CHECK(xbr_output.width() == hq2x_output.width());
        CHECK(xbr_output.height() == hq2x_output.height());
        
        // Both should preserve diagonal pattern to some degree
        CHECK(xbr_output.get_pixel(0, 0) == BLACK);
        CHECK(hq2x_output.get_pixel(0, 0) == BLACK);
        CHECK(xbr_output.get_pixel(4, 4) == BLACK);
        // HQ2x might interpolate differently
        bool has_black_center = (hq2x_output.get_pixel(4, 4) == BLACK) ||
                                (hq2x_output.get_pixel(3, 3) == BLACK) ||
                                (hq2x_output.get_pixel(5, 5) == BLACK);
        CHECK(has_black_center);
        
        // XBR and HQ2x use different algorithms, so outputs will differ
        // but both should have reasonable color distribution
        CHECK(xbr_output.countPixelsOfColor(BLACK) >= 6);
        CHECK(hq2x_output.countPixelsOfColor(BLACK) >= 3);
    }
    
    SUBCASE("Performance characteristics - complex pattern") {
        TestImageXBR input(5, 5);
        // Create complex pattern
        input.fillPattern({
            {RED, GREEN, BLUE, YELLOW, RED},
            {GREEN, BLACK, WHITE, BLACK, GREEN},
            {BLUE, WHITE, GRAY, WHITE, BLUE},
            {YELLOW, BLACK, WHITE, BLACK, YELLOW},
            {RED, GREEN, BLUE, YELLOW, RED}
        });
        
        auto xbr_output = scaleXbr<TestImageXBR, TestImageXBR>(input);
        auto hq2x_output = scaleHq2x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(xbr_output.width() == 10);
        CHECK(hq2x_output.width() == 10);
        CHECK(xbr_output.height() == 10);
        CHECK(hq2x_output.height() == 10);
        
        // With correct operator!=, XBR behaves according to original algorithm
        auto xbr_center = xbr_output.get_pixel(4, 4);
        bool xbr_center_valid = (xbr_center.x >= 85 && xbr_center.x <= 170);
        CHECK(xbr_center_valid); // Accept interpolated values
        // HQ2x might interpolate center area
        bool has_gray = false;
        for (size_t y = 3; y < 6; ++y) {
            for (size_t x = 3; x < 6; ++x) {
                if (hq2x_output.get_pixel(x, y) == GRAY) {
                    has_gray = true;
                    break;
                }
            }
        }
        CHECK(has_gray);
    }
}

TEST_CASE("HQ3x Algorithm Tests") {
    const uvec3 BLACK{0, 0, 0};
    const uvec3 WHITE{255, 255, 255};
    const uvec3 RED{255, 0, 0};
    const uvec3 GREEN{0, 255, 0};
    const uvec3 BLUE{0, 0, 255};
    
    SUBCASE("Basic 3x scaling") {
        TestImageXBR input(2, 2);
        // Set pixels individually
        input.set_pixel(0, 0, BLACK);
        input.set_pixel(1, 0, WHITE);
        input.set_pixel(0, 1, WHITE);
        input.set_pixel(1, 1, BLACK);
        
        auto output = scaleHq3x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 6);
        CHECK(output.height() == 6);
        
        // Check that corners maintain their values
        CHECK(output.get_pixel(0, 0) == BLACK);
        CHECK(output.get_pixel(5, 0) == WHITE);
        CHECK(output.get_pixel(0, 5) == WHITE);
        CHECK(output.get_pixel(5, 5) == BLACK);
    }
    
    SUBCASE("Edge interpolation") {
        TestImageXBR input(3, 3);
        // Set horizontal stripes
        for (size_t x = 0; x < 3; ++x) {
            input.set_pixel(x, 0, RED);
            input.set_pixel(x, 1, GREEN);
            input.set_pixel(x, 2, BLUE);
        }
        
        auto output = scaleHq3x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 9);
        CHECK(output.height() == 9);
        
        // Top row should be red
        CHECK(output.get_pixel(0, 0) == RED);
        CHECK(output.get_pixel(4, 0) == RED);
        CHECK(output.get_pixel(8, 0) == RED);
        
        // Middle row should be green
        CHECK(output.get_pixel(0, 4) == GREEN);
        CHECK(output.get_pixel(4, 4) == GREEN);
        CHECK(output.get_pixel(8, 4) == GREEN);
        
        // Bottom row should be blue
        CHECK(output.get_pixel(0, 8) == BLUE);
        CHECK(output.get_pixel(4, 8) == BLUE);
        CHECK(output.get_pixel(8, 8) == BLUE);
    }
    
    SUBCASE("Diagonal pattern") {
        TestImageXBR input(3, 3);
        // Set checkerboard pattern
        input.set_pixel(0, 0, BLACK);
        input.set_pixel(1, 0, WHITE);
        input.set_pixel(2, 0, BLACK);
        input.set_pixel(0, 1, WHITE);
        input.set_pixel(1, 1, BLACK);
        input.set_pixel(2, 1, WHITE);
        input.set_pixel(0, 2, BLACK);
        input.set_pixel(1, 2, WHITE);
        input.set_pixel(2, 2, BLACK);
        
        auto output = scaleHq3x<TestImageXBR, TestImageXBR>(input);
        
        CHECK(output.width() == 9);
        CHECK(output.height() == 9);
        
        // Center should be black
        CHECK(output.get_pixel(4, 4) == BLACK);
        
        // Corners should maintain their values
        CHECK(output.get_pixel(0, 0) == BLACK);
        CHECK(output.get_pixel(8, 0) == BLACK);
        CHECK(output.get_pixel(0, 8) == BLACK);
        CHECK(output.get_pixel(8, 8) == BLACK);
    }
}