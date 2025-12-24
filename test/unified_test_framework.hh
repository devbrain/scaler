#pragma once

#include <scaler/unified_scaler.hh>
#include "test_common.hh"
#include <scaler/gpu/unified_gpu_scaler.hh>
#include <scaler/sdl/sdl_image.hh>
#include <SDL.h>
#include <doctest/doctest.h>
#include <functional>
#include <type_traits>
#include <vector>
#include <cmath>

namespace scaler::test {

/**
 * Unified test framework for CPU and GPU scalers
 *
 * This framework provides templated test functions that work with both
 * CPU image types and GPU texture types, enabling identical tests to be
 * run on both implementations.
 */

// Type traits to detect CPU vs GPU types
template<typename T>
struct is_gpu_type : std::false_type {};

template<>
struct is_gpu_type<gpu::input_texture> : std::true_type {};

template<>
struct is_gpu_type<gpu::output_texture> : std::true_type {};

template<typename T>
inline constexpr bool is_gpu_type_v = is_gpu_type<T>::value;

// Test pattern generation
namespace patterns {

    /**
     * Generate a checkerboard pattern for testing
     * @param width Pattern width
     * @param height Pattern height
     * @param tile_size Size of each checker tile
     * @return Vector of RGBA pixel data
     */
    inline std::vector<uint8_t> generate_checkerboard(int width, int height, int tile_size = 4) {
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool is_white = ((x / tile_size) + (y / tile_size)) % 2;
                uint8_t value = is_white ? 255 : 0;

                size_t idx = static_cast<size_t>((y * width + x) * 4);
                pixels[idx] = value;     // R
                pixels[idx + 1] = value; // G
                pixels[idx + 2] = value; // B
                pixels[idx + 3] = 255;   // A
            }
        }

        return pixels;
    }

    /**
     * Generate a gradient pattern for testing
     * @param width Pattern width
     * @param height Pattern height
     * @param horizontal If true, gradient goes left to right; otherwise top to bottom
     * @return Vector of RGBA pixel data
     */
    inline std::vector<uint8_t> generate_gradient(int width, int height, bool horizontal = true) {
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float progress = horizontal ?
                    static_cast<float>(x) / static_cast<float>(width - 1) :
                    static_cast<float>(y) / static_cast<float>(height - 1);

                uint8_t value = static_cast<uint8_t>(progress * 255);

                size_t idx = static_cast<size_t>((y * width + x) * 4);
                pixels[idx] = value;     // R
                pixels[idx + 1] = value; // G
                pixels[idx + 2] = value; // B
                pixels[idx + 3] = 255;   // A
            }
        }

        return pixels;
    }

    /**
     * Generate a solid color pattern
     * @param width Pattern width
     * @param height Pattern height
     * @param r Red component
     * @param g Green component
     * @param b Blue component
     * @param a Alpha component
     * @return Vector of RGBA pixel data
     */
    inline std::vector<uint8_t> generate_solid(int width, int height,
                                              uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 4));

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = static_cast<size_t>((y * width + x) * 4);
                pixels[idx] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = a;
            }
        }

        return pixels;
    }
}

// CPU test image creation
template<typename ImageType>
inline typename std::enable_if<!is_gpu_type_v<ImageType>, ImageType>::type
create_test_input(const std::vector<uint8_t>& pixels, int width, int height) {
    // For CPU images, create directly from pixel data
    ImageType image(static_cast<size_t>(width), static_cast<size_t>(height));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>((y * width + x) * 4);
            image.set_pixel(static_cast<size_t>(x), static_cast<size_t>(y), typename ImageType::pixel_type{
                pixels[idx], pixels[idx + 1], pixels[idx + 2]
            });
        }
    }

    return image;
}

// GPU texture creation
template<typename TextureType>
inline typename std::enable_if<is_gpu_type_v<TextureType>, TextureType>::type
create_test_input(const std::vector<uint8_t>& pixels, int width, int height) {
    // For GPU textures, create OpenGL texture and wrap it
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return TextureType(texture, static_cast<size_t>(width), static_cast<size_t>(height));
}

// Generic test function template
template<typename InputType, typename OutputType, typename ScalerType>
void run_scaling_test(const std::string& test_name,
                     algorithm algo,
                     float scale_factor,
                     int input_width,
                     int input_height,
                     const std::vector<uint8_t>& input_pixels) {

    INFO("Running " << test_name << " for " <<
         (is_gpu_type_v<InputType> ? "GPU" : "CPU"));

    // Create input
    auto input = create_test_input<InputType>(input_pixels, input_width, input_height);

    // Calculate expected output dimensions
    auto dims = ScalerType::calculate_output_dimensions(input, algo, scale_factor);

    // Test with created output
    SUBCASE("Created output pattern") {
        auto output = ScalerType::scale(input, algo, scale_factor);

        CHECK(output.width() == dims.width);
        CHECK(output.height() == dims.height);
    }

    // Test with preallocated output (only for CPU - GPU requires texture ID)
    if constexpr (!is_gpu_type_v<OutputType>) {
        SUBCASE("Preallocated output pattern") {
            // Create preallocated output with correct dimensions
            OutputType output(dims.width, dims.height);

            ScalerType::scale(input, output, algo);

            CHECK(output.width() == dims.width);
            CHECK(output.height() == dims.height);
        }
    }

    // Test dimension mismatch exception (only for CPU - GPU requires texture ID)
    if constexpr (!is_gpu_type_v<OutputType>) {
        SUBCASE("Dimension mismatch handling") {
            // Create output with wrong dimensions
            OutputType wrong_output(dims.width + 1, dims.height + 1);

            bool exception_thrown = false;
            try {
                ScalerType::scale(input, wrong_output, algo);
            } catch (const std::exception&) {
                exception_thrown = true;
            }

            CHECK(exception_thrown);
        }
    }
}

// Performance measurement utilities
struct performance_result {
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double std_dev_ms;
    int iterations;
};

template<typename Func>
performance_result measure_performance(Func&& func, int iterations = 100) {
    std::vector<double> times;
    times.reserve(static_cast<size_t>(iterations));

    // Warmup
    for (int i = 0; i < 5; ++i) {
        func();
    }

    // Measure
    for (int i = 0; i < iterations; ++i) {
        auto start = SDL_GetPerformanceCounter();
        func();
        auto end = SDL_GetPerformanceCounter();

        double ms = (static_cast<double>(end - start) * 1000.0) /
                   static_cast<double>(SDL_GetPerformanceFrequency());
        times.push_back(ms);
    }

    // Calculate statistics
    double sum = 0.0;
    double min = times[0];
    double max = times[0];

    for (double t : times) {
        sum += t;
        min = std::min(min, t);
        max = std::max(max, t);
    }

    double avg = sum / iterations;

    // Standard deviation
    double variance = 0.0;
    for (double t : times) {
        double diff = t - avg;
        variance += diff * diff;
    }
    variance /= iterations;
    double std_dev = std::sqrt(variance);

    return {avg, min, max, std_dev, iterations};
}

// Comparison utilities
template<typename ImageType>
typename std::enable_if<!is_gpu_type_v<ImageType>, std::vector<uint8_t>>::type
extract_pixels(const ImageType& image) {
    std::vector<uint8_t> pixels(image.width() * image.height() * 4);

    for (size_t y = 0; y < image.height(); ++y) {
        for (size_t x = 0; x < image.width(); ++x) {
            auto pixel = image.get_pixel(x, y);
            size_t idx = (y * image.width() + x) * 4;
            pixels[idx] = static_cast<uint8_t>(pixel.x);
            pixels[idx + 1] = static_cast<uint8_t>(pixel.y);
            pixels[idx + 2] = static_cast<uint8_t>(pixel.z);
            pixels[idx + 3] = 255; // Alpha
        }
    }

    return pixels;
}

template<typename TextureType>
typename std::enable_if<is_gpu_type_v<TextureType>, std::vector<uint8_t>>::type
extract_pixels(const TextureType& texture) {
    std::vector<uint8_t> pixels(texture.width() * texture.height() * 4);

    // Create FBO to read texture
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture.id(), 0);

    glReadPixels(0, 0, static_cast<GLsizei>(texture.width()), static_cast<GLsizei>(texture.height()),
                GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    return pixels;
}

// Pixel comparison with tolerance
struct comparison_result {
    bool matches;
    double avg_diff;
    double max_diff;
    int mismatched_pixels;
    double mismatch_percentage;
};

inline comparison_result compare_pixels(const std::vector<uint8_t>& pixels1,
                                       const std::vector<uint8_t>& pixels2,
                                       int tolerance = 1) {
    if (pixels1.size() != pixels2.size()) {
        return {false, 255.0, 255.0, -1, 100.0};
    }

    size_t pixel_count = pixels1.size() / 4;
    size_t mismatches = 0;
    double total_diff = 0.0;
    double max_diff = 0.0;

    for (size_t i = 0; i < pixels1.size(); i += 4) {
        double dr = std::abs(static_cast<int>(pixels1[i]) - static_cast<int>(pixels2[i]));
        double dg = std::abs(static_cast<int>(pixels1[i+1]) - static_cast<int>(pixels2[i+1]));
        double db = std::abs(static_cast<int>(pixels1[i+2]) - static_cast<int>(pixels2[i+2]));

        double pixel_diff = (dr + dg + db) / 3.0;
        total_diff += pixel_diff;
        max_diff = std::max(max_diff, pixel_diff);

        if (dr > tolerance || dg > tolerance || db > tolerance) {
            mismatches++;
        }
    }

    double avg_diff = total_diff / static_cast<double>(pixel_count);
    double mismatch_pct = (static_cast<double>(mismatches) / static_cast<double>(pixel_count)) * 100.0;

    return {
        mismatches == 0,
        avg_diff,
        max_diff,
        static_cast<int>(mismatches),
        mismatch_pct
    };
}

} // namespace scaler::test