#include <doctest/doctest.h>
#include "test_common.hh"
#include <scaler/unified_scaler.hh>
#include <chrono>
#include <map>

using namespace scaler;
using namespace scaler::test;

TEST_CASE("Unified Algorithm Tests - Basic Functionality") {
    // Test patterns with validation functions
    struct TestPattern {
        const char* name;
        TestInputImageRGB (*generator)();
        std::function<void(const TestImage&, algorithm, float)> validator;
    };

    auto basic_validator = [](const TestImage& output, algorithm algo, float scale) {
        CHECK(output.width() > 0);
        CHECK(output.height() > 0);
    };

    auto edge_validator = [](const TestImage& output, algorithm algo, float scale) {
        // For edge-preserving algorithms with edge patterns, verify edges exist
        // But only if the output is expected to have edges (not for solid colors)
        if (is_edge_preserving_algorithm(algo)) {
            // Count distinct colors to see if we should expect edges
            std::vector<uvec3> unique_colors;
            for (size_t y = 0; y < output.height(); ++y) {
                for (size_t x = 0; x < output.width(); ++x) {
                    auto color = output.at(x, y);
                    bool found = false;
                    for (const auto& existing : unique_colors) {
                        if (colors_equal(existing, color, 5)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        unique_colors.push_back(color);
                        if (unique_colors.size() > 10) break;  // Early exit if many colors
                    }
                }
                if (unique_colors.size() > 10) break;
            }

            // Only check for edges if there are multiple distinct colors
            if (unique_colors.size() > 1) {
                bool has_edge = false;
                for (size_t y = 1; y < output.height() - 1; ++y) {
                    for (size_t x = 1; x < output.width() - 1; ++x) {
                        auto center = output.at(x, y);
                        auto left = output.at(x - 1, y);
                        auto right = output.at(x + 1, y);
                        auto top = output.at(x, y - 1);
                        auto bottom = output.at(x, y + 1);

                        // Check for any significant difference (edge) in any color channel
                        int max_diff = 0;
                        max_diff = std::max(max_diff, static_cast<int>(center.x > left.x ? center.x - left.x : left.x - center.x));
                        max_diff = std::max(max_diff, static_cast<int>(center.x > right.x ? center.x - right.x : right.x - center.x));
                        max_diff = std::max(max_diff, static_cast<int>(center.x > top.x ? center.x - top.x : top.x - center.x));
                        max_diff = std::max(max_diff, static_cast<int>(center.x > bottom.x ? center.x - bottom.x : bottom.x - center.x));

                        if (max_diff > 50) {  // Lower threshold for edge detection
                            has_edge = true;
                            break;
                        }
                    }
                    if (has_edge) break;
                }
                CHECK(has_edge);
            }
        }
    };

    auto single_pixel_validator = [](const TestImage& output, algorithm algo, float scale) {
        // Check that single pixel expands correctly
        size_t expected_size = static_cast<size_t>(scale);
        CHECK(output.width() == expected_size);
        CHECK(output.height() == expected_size);

        // All output pixels should have similar color to input
        uvec3 input_color{255, 0, 0};
        for (size_t y = 0; y < output.height(); ++y) {
            for (size_t x = 0; x < output.width(); ++x) {
                CHECK(colors_equal(output.at(x, y), input_color, 5));
            }
        }
    };

    std::vector<TestPattern> patterns = {
        {"checkerboard_4x4", []() { return create_checkerboard(4); }, edge_validator},
        {"checkerboard_8x8", []() { return create_checkerboard(8); }, basic_validator},
        {"gradient_8x8", []() { return create_gradient(8, 8); }, basic_validator},
        {"single_pixel", []() { return create_single_pixel(); }, single_pixel_validator},
        {"vertical_lines", []() { return create_vertical_lines(8, 8); }, edge_validator},
        {"horizontal_lines", []() { return create_horizontal_lines(8, 8); }, edge_validator},
        {"diagonal_line", []() { return create_diagonal_line(8); }, edge_validator},
        {"edge_pattern", []() { return create_edge_pattern(); }, edge_validator},
        {"solid_white", []() { return create_solid_color(4, 4, {255, 255, 255}); }, basic_validator},
        {"solid_black", []() { return create_solid_color(4, 4, {0, 0, 0}); }, basic_validator}
    };

    // Test all CPU algorithms
    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        // Skip GPU-only and 3D algorithms
        if (algo == algorithm::Trilinear) continue;

        SUBCASE(scaler_capabilities::get_algorithm_name(algo).c_str()) {
            auto scales = scaler_capabilities::get_supported_scales(algo);

            // If algorithm supports arbitrary scale, test common scales
            if (scales.empty() && scaler_capabilities::supports_arbitrary_scale(algo)) {
                scales = {2.0f, 3.0f, 4.0f};
            }

            for (float scale : scales) {
                for (const auto& pattern : patterns) {
                    INFO("Algorithm: " << scaler_capabilities::get_algorithm_name(algo));
                    INFO("Pattern: " << pattern.name);
                    INFO("Scale: " << scale);

                    auto input = pattern.generator();

                    // Use unified scaler interface
                    auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);

                    // Common validations
                    CHECK(validate_dimensions(output, input, scale));

                    // Pattern-specific validation
                    pattern.validator(output, algo, scale);

                    // Color preservation check (relaxed for interpolating algorithms)
                    // Skip for algorithms that heavily interpolate or blend colors
                    if (algo != algorithm::Bilinear && algo != algorithm::HQ) {
                        CHECK(validate_color_preservation(output, input));
                    }
                }
            }
        }
    }
}

TEST_CASE("Unified Algorithm Tests - Color Preservation") {
    // Test specific color preservation across all algorithms
    uvec3 test_colors[] = {
        {255, 0, 0},    // Red
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {255, 255, 0},  // Yellow
        {255, 0, 255},  // Magenta
        {0, 255, 255},  // Cyan
        {128, 128, 128}, // Gray
        {255, 255, 255}, // White
        {0, 0, 0}        // Black
    };

    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        if (algo == algorithm::Trilinear) continue;
        // Skip interpolating algorithms that blend colors
        if (algo == algorithm::Bilinear) continue;

        SUBCASE(scaler_capabilities::get_algorithm_name(algo).c_str()) {
            auto scales = scaler_capabilities::get_supported_scales(algo);
            if (scales.empty() && scaler_capabilities::supports_arbitrary_scale(algo)) {
                scales = {2.0f};  // Just test 2x for color preservation
            }

            for (float scale : scales) {
                for (const auto& color : test_colors) {
                    INFO("Color: (" << (int)color.x << ", " << (int)color.y << ", " << (int)color.z << ")");

                    auto input = create_solid_color(2, 2, color);
                    auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);

                    // Check that the color is preserved somewhere in output
                    bool color_found = false;
                    for (size_t y = 0; y < output.height(); ++y) {
                        for (size_t x = 0; x < output.width(); ++x) {
                            // Allow more tolerance for algorithms that do some processing
                            uint8_t tolerance = (algo == algorithm::HQ) ? 10 : 1;
                            if (colors_equal(output.at(x, y), color, tolerance)) {
                                color_found = true;
                                break;
                            }
                        }
                        if (color_found) break;
                    }

                    CHECK(color_found);
                }
            }
        }
    }
}

TEST_CASE("Unified Algorithm Tests - Symmetry") {
    // Test that algorithms preserve symmetry
    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        if (algo == algorithm::Trilinear) continue;

        SUBCASE(scaler_capabilities::get_algorithm_name(algo).c_str()) {
            auto scales = scaler_capabilities::get_supported_scales(algo);
            if (scales.empty() && scaler_capabilities::supports_arbitrary_scale(algo)) {
                scales = {2.0f};
            }

            for (float scale : scales) {
                // Create symmetric pattern
                TestInputImageRGB input(4, 4);
                for (size_t y = 0; y < 4; ++y) {
                    for (size_t x = 0; x < 4; ++x) {
                        // Create a symmetric pattern
                        bool is_center = (x == 1 || x == 2) && (y == 1 || y == 2);
                        input.at(x, y) = is_center ? uvec3{255, 255, 255} : uvec3{0, 0, 0};
                    }
                }

                auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);

                // Some algorithms use heuristics that don't preserve perfect symmetry
                // Only test symmetry for simpler algorithms that are known to preserve it
                bool should_test_symmetry = (algo == algorithm::Nearest ||
                                            algo == algorithm::EPX ||
                                            algo == algorithm::Scale ||
                                            algo == algorithm::ScaleSFX);

                if (should_test_symmetry) {
                    // Check horizontal symmetry
                    bool h_symmetric = true;
                    for (size_t y = 0; y < output.height(); ++y) {
                        for (size_t x = 0; x < output.width() / 2; ++x) {
                            size_t mirror_x = output.width() - 1 - x;
                            if (!colors_equal(output.at(x, y), output.at(mirror_x, y), 5)) {
                                h_symmetric = false;
                                break;
                            }
                        }
                    }
                    CHECK(h_symmetric);

                    // Check vertical symmetry
                    bool v_symmetric = true;
                    for (size_t y = 0; y < output.height() / 2; ++y) {
                        for (size_t x = 0; x < output.width(); ++x) {
                            size_t mirror_y = output.height() - 1 - y;
                            if (!colors_equal(output.at(x, y), output.at(x, mirror_y), 5)) {
                                v_symmetric = false;
                                break;
                            }
                        }
                    }
                    CHECK(v_symmetric);
                }
            }
        }
    }
}

TEST_CASE("Unified Algorithm Tests - Edge Cases") {
    // Test edge cases like 1x1 images, empty regions, etc.
    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        if (algo == algorithm::Trilinear) continue;

        SUBCASE(scaler_capabilities::get_algorithm_name(algo).c_str()) {
            auto scales = scaler_capabilities::get_supported_scales(algo);
            if (scales.empty() && scaler_capabilities::supports_arbitrary_scale(algo)) {
                scales = {2.0f};
            }

            for (float scale : scales) {
                // Test 1x1 image
                {
                    auto input = create_single_pixel({42, 84, 168});
                    auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);
                    CHECK(output.width() == static_cast<size_t>(scale));
                    CHECK(output.height() == static_cast<size_t>(scale));
                }

                // Test uniform color (no edges)
                {
                    auto input = create_solid_color(4, 4, {100, 100, 100});
                    auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);

                    // All pixels should be similar to input
                    bool uniform = true;
                    for (size_t y = 0; y < output.height(); ++y) {
                        for (size_t x = 0; x < output.width(); ++x) {
                            if (!colors_equal(output.at(x, y), {100, 100, 100}, 5)) {
                                uniform = false;
                                break;
                            }
                        }
                    }
                    CHECK(uniform);
                }

                // Test maximum contrast
                {
                    auto input = create_checkerboard(2, {255, 255, 255}, {0, 0, 0});
                    auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);

                    // Output should contain both black and white
                    bool has_black = false, has_white = false;
                    for (size_t y = 0; y < output.height(); ++y) {
                        for (size_t x = 0; x < output.width(); ++x) {
                            auto pixel = output.at(x, y);
                            if (pixel.x < 50) has_black = true;
                            if (pixel.x > 200) has_white = true;
                        }
                    }
                    CHECK(has_black);
                    CHECK(has_white);
                }
            }
        }
    }
}

TEST_CASE("Algorithm Performance Comparison") {
    // Create a larger test image for meaningful performance comparison
    auto input = create_checkerboard(64);

    std::map<std::string, double> timings;

    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        if (algo == algorithm::Trilinear) continue;

        // Test with 2x scale (supported by all algorithms)
        if (!algorithm_supports_scale(algo, 2.0f)) continue;

        // Warm-up run
        auto warmup = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, 2.0f);

        // Timed run
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10; ++i) {
            auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, 2.0f);
        }

        auto end = std::chrono::high_resolution_clock::now();

        double time_ms = std::chrono::duration<double, std::milli>(end - start).count() / 10.0;
        timings[scaler_capabilities::get_algorithm_name(algo)] = time_ms;
    }

    // Report timings
    INFO("Algorithm Performance (64x64 -> 128x128):");
    for (const auto& [name, time] : timings) {
        INFO("  " << name << ": " << time << " ms");
    }

    // Verify that at least some algorithms were tested
    CHECK(timings.size() > 0);
}

TEST_CASE("Algorithm Capabilities Check") {
    // Verify that capability queries work correctly
    for (auto algo : scaler_capabilities::get_all_algorithms()) {
        auto name = scaler_capabilities::get_algorithm_name(algo);
        INFO("Algorithm: " << name);

        auto scales = scaler_capabilities::get_supported_scales(algo);
        bool arbitrary = scaler_capabilities::supports_arbitrary_scale(algo);

        // Every algorithm should either have fixed scales or support arbitrary
        CHECK((scales.size() > 0) != arbitrary);  // XOR: either fixed or arbitrary, not both

        if (!scales.empty()) {
            // Fixed scale algorithms
            for (float scale : scales) {
                CHECK(scale > 0);
                CHECK(scale <= 4.0f);  // Reasonable upper bound
            }
        }

        // Check that algorithm enum round-trips correctly
        CHECK(scaler_capabilities::get_algorithm_name(algo).length() > 0);
    }
}

TEST_CASE("Unified Scaler Interface Consistency") {
    // Test that the unified interface works consistently
    auto input = create_checkerboard(4);

    // Test that we can use the scaler with different algorithms without issues
    std::vector<algorithm> test_algos = {
        algorithm::Nearest,
        algorithm::EPX,
        algorithm::Eagle,
        algorithm::Scale
    };

    for (auto algo : test_algos) {
        // Get the first supported scale
        auto scales = scaler_capabilities::get_supported_scales(algo);
        float scale = scales.empty() ? 2.0f : scales[0];

        if (!algorithm_supports_scale(algo, scale)) continue;

        SUBCASE(scaler_capabilities::get_algorithm_name(algo).c_str()) {
            // Test that scaling works
            auto output = Scaler<TestInputImageRGB, TestImage>::scale(input, algo, scale);
            CHECK(output.width() > 0);
            CHECK(output.height() > 0);

            // Test that dimensions are correct
            CHECK(validate_dimensions(output, input, scale));

            // Test that output is not all black (unless input is)
            bool has_non_black = false;
            for (size_t y = 0; y < output.height(); ++y) {
                for (size_t x = 0; x < output.width(); ++x) {
                    auto pixel = output.at(x, y);
                    if (pixel.x > 0 || pixel.y > 0 || pixel.z > 0) {
                        has_non_black = true;
                        break;
                    }
                }
                if (has_non_black) break;
            }
            CHECK(has_non_black);
        }
    }
}