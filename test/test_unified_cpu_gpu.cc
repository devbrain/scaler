#include <doctest/doctest.h>
#include "unified_test_framework.hh"
#include "test_common.hh"
#include <scaler/gpu/unified_gpu_scaler.hh>
#include <SDL.h>
#include <memory>

using namespace scaler;
using namespace scaler::test;

#include "gpu_test_context.hh"

TEST_CASE("Unified CPU/GPU Scaling Tests") {
    // Test parameters
    const int input_size = 8;
    const algorithm test_algo = algorithm::EPX;
    const float scale_factor = 2.0f;

    // Generate test pattern
    auto checkerboard = patterns::generate_checkerboard(input_size, input_size, 2);

    SUBCASE("CPU Scaling") {
        using InputType = TestOutputImageRGB;  // Can be used as input too
        using OutputType = TestOutputImageRGB;
        using ScalerType = Scaler<InputType, OutputType>;

        run_scaling_test<InputType, OutputType, ScalerType>(
            "Checkerboard EPX",
            test_algo,
            scale_factor,
            input_size,
            input_size,
            checkerboard
        );
    }

    SUBCASE("GPU Scaling") {
        gpu_context::scoped_context gpu_ctx;
        if (!gpu_ctx) {
            INFO("GPU context not available - skipping GPU tests");
            return;
        }

        using InputType = gpu::input_texture;
        using OutputType = gpu::output_texture;
        using ScalerType = GPUScaler;

        run_scaling_test<InputType, OutputType, ScalerType>(
            "Checkerboard EPX",
            test_algo,
            scale_factor,
            input_size,
            input_size,
            checkerboard
        );
    }
}

TEST_CASE("CPU vs GPU Output Comparison") {
    gpu_context::scoped_context gpu_ctx;
    if (!gpu_ctx) {
        INFO("GPU context not available - skipping comparison tests");
        return;
    }

    const int input_size = 16;
    const algorithm test_algo = algorithm::Nearest;
    const float scale_factor = 2.0f;

    // Generate test patterns
    auto gradient = patterns::generate_gradient(input_size, input_size, true);
    auto solid = patterns::generate_solid(input_size, input_size, 128, 128, 128);

    SUBCASE("Gradient Pattern Comparison") {
        // CPU scaling
        auto cpu_input = create_test_input<TestOutputImageRGB>(gradient, input_size, input_size);
        auto cpu_output = Scaler<TestOutputImageRGB, TestOutputImageRGB>::scale(
            cpu_input, test_algo, scale_factor);
        auto cpu_pixels = extract_pixels(cpu_output);

        // GPU scaling
        auto gpu_input = create_test_input<gpu::input_texture>(gradient, input_size, input_size);
        auto gpu_output = GPUScaler::scale(gpu_input, test_algo, scale_factor);
        auto gpu_pixels = extract_pixels(gpu_output);

        // Compare
        auto result = compare_pixels(cpu_pixels, gpu_pixels, 1);

        CHECK(result.matches);
        CHECK(result.avg_diff < 1.0);
        CHECK(result.mismatch_percentage < 1.0);

        INFO("Average difference: " << result.avg_diff);
        INFO("Max difference: " << result.max_diff);
        INFO("Mismatch percentage: " << result.mismatch_percentage << "%");

        // Cleanup GPU resources
        GLuint gpu_input_id = gpu_input.id();
        GLuint gpu_output_id = gpu_output.id();
        glDeleteTextures(1, &gpu_input_id);
        glDeleteTextures(1, &gpu_output_id);
    }

    SUBCASE("Solid Color Pattern Comparison") {
        // CPU scaling
        auto cpu_input = create_test_input<TestOutputImageRGB>(solid, input_size, input_size);
        auto cpu_output = Scaler<TestOutputImageRGB, TestOutputImageRGB>::scale(
            cpu_input, test_algo, scale_factor);
        auto cpu_pixels = extract_pixels(cpu_output);

        // GPU scaling
        auto gpu_input = create_test_input<gpu::input_texture>(solid, input_size, input_size);
        auto gpu_output = GPUScaler::scale(gpu_input, test_algo, scale_factor);
        auto gpu_pixels = extract_pixels(gpu_output);

        // Compare
        auto result = compare_pixels(cpu_pixels, gpu_pixels, 0);

        CHECK(result.matches);
        CHECK(result.avg_diff == 0.0);
        CHECK(result.mismatch_percentage == 0.0);

        // Cleanup GPU resources
        GLuint gpu_input_id = gpu_input.id();
        GLuint gpu_output_id = gpu_output.id();
        glDeleteTextures(1, &gpu_input_id);
        glDeleteTextures(1, &gpu_output_id);
    }
}

TEST_CASE("Performance Comparison") {
    gpu_context::scoped_context gpu_ctx;
    if (!gpu_ctx) {
        INFO("GPU context not available - skipping performance tests");
        return;
    }

    // Test with different sizes
    std::vector<int> test_sizes = {32, 64, 128, 256};
    const algorithm test_algo = algorithm::EPX;
    const float scale_factor = 2.0f;

    for (int size : test_sizes) {
        SUBCASE(("Size " + std::to_string(size)).c_str()) {
            // Generate test data
            auto pattern = patterns::generate_checkerboard(size, size, 8);

            // Measure CPU performance
            auto cpu_input = create_test_input<TestOutputImageRGB>(pattern, size, size);

            auto cpu_perf = measure_performance([&]() {
                auto output = Scaler<TestOutputImageRGB, TestOutputImageRGB>::scale(
                    cpu_input, test_algo, scale_factor);
            }, 50);

            // Measure GPU performance
            auto gpu_input = create_test_input<gpu::input_texture>(pattern, size, size);

            auto gpu_perf = measure_performance([&]() {
                auto output = GPUScaler::scale(gpu_input, test_algo, scale_factor);
                GLuint id = output.id();
                glDeleteTextures(1, &id);
            }, 50);

            // Report results
            INFO("Size: " << size << "x" << size);
            INFO("CPU: avg=" << cpu_perf.avg_time_ms << "ms, "
                 << "min=" << cpu_perf.min_time_ms << "ms, "
                 << "max=" << cpu_perf.max_time_ms << "ms");
            INFO("GPU: avg=" << gpu_perf.avg_time_ms << "ms, "
                 << "min=" << gpu_perf.min_time_ms << "ms, "
                 << "max=" << gpu_perf.max_time_ms << "ms");
            INFO("Speedup: " << cpu_perf.avg_time_ms / gpu_perf.avg_time_ms << "x");

            // GPU should generally be faster for larger sizes
            if (size >= 128) {
                CHECK(gpu_perf.avg_time_ms < cpu_perf.avg_time_ms);
            }

            // Cleanup
            GLuint gpu_input_id = gpu_input.id();
            glDeleteTextures(1, &gpu_input_id);
        }
    }
}

TEST_CASE("Algorithm Availability Consistency") {
    // Check that algorithm availability is consistent between CPU and GPU
    std::vector<algorithm> test_algorithms = {
        algorithm::Nearest,
        algorithm::Bilinear,
        algorithm::EPX,
        algorithm::Eagle,
        algorithm::Scale,
        algorithm::HQ,
        algorithm::xBR
    };

    for (algorithm algo : test_algorithms) {
        SUBCASE((std::string("Algorithm: ") + scaler_capabilities::get_algorithm_name(algo)).c_str()) {
            bool cpu_available = true; // All algorithms available on CPU
            bool gpu_available = GPUScaler::is_gpu_accelerated(algo);

            INFO("Algorithm: " << scaler_capabilities::get_algorithm_name(algo));
            INFO("CPU available: " << cpu_available);
            INFO("GPU available: " << gpu_available);

            // HQ and xBR should not be GPU accelerated
            if (algo == algorithm::HQ || algo == algorithm::xBR) {
                CHECK(!gpu_available);
            }
            // Basic algorithms should be GPU accelerated
            else if (algo == algorithm::Nearest || algo == algorithm::Bilinear ||
                    algo == algorithm::EPX || algo == algorithm::Eagle) {
                CHECK(gpu_available);
            }
        }
    }
}

TEST_CASE("Memory Pattern Tests") {
    gpu_context::scoped_context gpu_ctx;
    if (!gpu_ctx) {
        INFO("GPU context not available - skipping memory tests");
        return;
    }

    const int input_size = 16;
    const algorithm test_algo = algorithm::EPX;
    const float scale_factor = 2.0f;
    auto pattern = patterns::generate_gradient(input_size, input_size, false);

    SUBCASE("CPU Preallocated vs Created") {
        auto input = create_test_input<TestOutputImageRGB>(pattern, input_size, input_size);

        // Created pattern
        auto created = Scaler<TestOutputImageRGB, TestOutputImageRGB>::scale(
            input, test_algo, scale_factor);

        // Preallocated pattern
        TestOutputImageRGB preallocated(created.width(), created.height());
        Scaler<TestOutputImageRGB, TestOutputImageRGB>::scale(
            input, preallocated, test_algo);

        // Compare outputs
        auto created_pixels = extract_pixels(created);
        auto preallocated_pixels = extract_pixels(preallocated);

        auto result = compare_pixels(created_pixels, preallocated_pixels, 0);
        CHECK(result.matches);
        CHECK(result.mismatch_percentage == 0.0);
    }

    SUBCASE("GPU Preallocated vs Created") {
        auto input = create_test_input<gpu::input_texture>(pattern, input_size, input_size);

        // Created pattern
        auto created = GPUScaler::scale(input, test_algo, scale_factor);

        // Preallocated pattern
        GLuint preallocated_tex;
        glGenTextures(1, &preallocated_tex);
        glBindTexture(GL_TEXTURE_2D, preallocated_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                    static_cast<GLsizei>(created.width()), static_cast<GLsizei>(created.height()), 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        gpu::output_texture preallocated(preallocated_tex, created.width(), created.height());
        GPUScaler::scale(input, preallocated, test_algo);

        // Compare outputs
        auto created_pixels = extract_pixels(created);
        auto preallocated_pixels = extract_pixels(preallocated);

        auto result = compare_pixels(created_pixels, preallocated_pixels, 0);
        CHECK(result.matches);
        CHECK(result.mismatch_percentage == 0.0);

        // Cleanup
        GLuint input_id = input.id();
        GLuint created_id = created.id();
        glDeleteTextures(1, &input_id);
        glDeleteTextures(1, &created_id);
        glDeleteTextures(1, &preallocated_tex);
    }
}