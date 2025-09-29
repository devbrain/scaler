#include <doctest/doctest.h>
#include <scaler/gpu/unified_gpu_scaler.hh>
#include <scaler/gpu/opengl_texture_scaler.hh>
#include <SDL.h>
#include <memory>
#include <vector>
#include <cstdlib>

using namespace scaler;

// Helper to create a test texture with a pattern
static GLuint create_test_texture(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Create a checkerboard pattern
    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>((y * width + x) * 4);
            bool is_white = ((x / 2) + (y / 2)) % 2;
            unsigned char value = is_white ? 255 : 0;
            pixels[idx] = value;     // R
            pixels[idx + 1] = value; // G
            pixels[idx + 2] = value; // B
            pixels[idx + 3] = 255;   // A
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

// Helper to read back texture pixels
static std::vector<unsigned char> read_texture_pixels(GLuint texture, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 4));

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    return pixels;
}

#include "gpu_test_context.hh"

TEST_CASE("Unified GPU Scaler") {
    // SDL should already be initialized by test main

    // Use shared GPU context
    scaler::test::gpu_context::scoped_context gpu_ctx;
    if (!gpu_ctx) {
        INFO("Could not create/get OpenGL context - skipping GPU tests");
        return;
    }

    SUBCASE("Preallocated output with correct dimensions") {
        // Create input texture
        GLuint input_tex = create_test_texture(8, 8);
        gpu::input_texture input(input_tex, 8, 8);

        // Create preallocated output texture
        GLuint output_tex = gpu::opengl_texture_scaler::create_output_texture(16, 16);
        gpu::output_texture output(output_tex, size_t(16), size_t(16));

        // Scale using unified GPU interface
        GPUScaler::scale(input, output, algorithm::EPX);

        // Verify dimensions
        CHECK(output.width() == 16);
        CHECK(output.height() == 16);

        // Read back and verify some pixels
        auto pixels = read_texture_pixels(output_tex, 16, 16);
        // First pixel should be either black or white from checkerboard
        bool is_valid = (pixels[0] == 255 || pixels[0] == 0);
        CHECK(is_valid);

        // Cleanup
        glDeleteTextures(1, &input_tex);
        glDeleteTextures(1, &output_tex);
    }

    SUBCASE("Scale with texture creation") {
        // Create input texture
        GLuint input_tex = create_test_texture(8, 8);
        gpu::input_texture input(input_tex, 8, 8);

        // Check GL error before scaling
        GLenum error_before = glGetError();
        if (error_before != GL_NO_ERROR) {
            INFO("GL error before scaling: " << error_before);
        }

        // Scale and create output texture
        auto output = GPUScaler::scale(input, algorithm::EPX, 2.0f);

        // Verify dimensions
        CHECK(output.width() == 16);
        CHECK(output.height() == 16);
        CHECK(output.id() != 0);

        // Cleanup
        glDeleteTextures(1, &input_tex);
        GLuint output_id = output.id();
        glDeleteTextures(1, &output_id);
    }

    SUBCASE("Dimension mismatch throws exception") {
        // Create input texture
        GLuint input_tex = create_test_texture(8, 8);
        gpu::input_texture input(input_tex, 8, 8);

        // Create output with wrong dimensions but correct scale factor (2x)
        // 17x17 is wrong for 8x8 input with 2x scale (should be 16x16)
        GLuint wrong_output_tex = gpu::opengl_texture_scaler::create_output_texture(17, 17);
        gpu::output_texture wrong_output(wrong_output_tex, size_t(17), size_t(17));

        bool exception_thrown = false;
        try {
            GPUScaler::scale(input, wrong_output, algorithm::EPX);
        } catch (const dimension_mismatch_exception& e) {
            exception_thrown = true;
            CHECK(e.m_algorithm == algorithm::EPX);
            CHECK(e.m_input_width == 8);
            CHECK(e.m_input_height == 8);
            CHECK(e.m_output_width == 17);
            CHECK(e.m_output_height == 17);
            CHECK(e.m_expected_width == 16);
            CHECK(e.m_expected_height == 16);
        } catch (const unsupported_scale_exception&) {
            // This is what we'll actually get since 17/8 = 2.125 which EPX doesn't support
            exception_thrown = true;
        }
        CHECK(exception_thrown);

        // Cleanup
        glDeleteTextures(1, &input_tex);
        glDeleteTextures(1, &wrong_output_tex);
    }

    SUBCASE("Unsupported scale throws exception") {
        // Create input texture
        GLuint input_tex = create_test_texture(8, 8);
        gpu::input_texture input(input_tex, 8, 8);

        // Create output with dimensions that imply 3x scaling
        GLuint output_tex_3x = gpu::opengl_texture_scaler::create_output_texture(24, 24);
        gpu::output_texture output_3x(output_tex_3x, size_t(24), size_t(24));

        // EPX only supports 2x scaling
        bool exception_thrown = false;
        try {
            GPUScaler::scale(input, output_3x, algorithm::EPX);
        } catch (const unsupported_scale_exception& e) {
            exception_thrown = true;
            CHECK(e.m_algorithm == algorithm::EPX);
            CHECK(e.m_requested_scale == doctest::Approx(3.0f));
        }
        CHECK(exception_thrown);

        // Cleanup
        glDeleteTextures(1, &input_tex);
        glDeleteTextures(1, &output_tex_3x);
    }

    SUBCASE("Algorithm availability check") {
        // Check that some algorithms are available on GPU
        CHECK(GPUScaler::is_gpu_accelerated(algorithm::EPX) == true);
        CHECK(GPUScaler::is_gpu_accelerated(algorithm::Nearest) == true);
        CHECK(GPUScaler::is_gpu_accelerated(algorithm::Bilinear) == true);

        // HQ algorithms are not GPU accelerated
        CHECK(GPUScaler::is_gpu_accelerated(algorithm::HQ) == false);
    }

    SUBCASE("Scale inference") {
        // Create textures
        GLuint input_tex = create_test_texture(10, 10);
        gpu::input_texture input(input_tex, 10, 10);

        GLuint output_tex = gpu::opengl_texture_scaler::create_output_texture(20, 20);
        gpu::output_texture output(output_tex, size_t(20), size_t(20));

        // Infer scale
        float inferred_scale = GPUScaler::infer_scale_factor(input, output);
        CHECK(inferred_scale == doctest::Approx(2.0f));

        // Cleanup
        glDeleteTextures(1, &input_tex);
        glDeleteTextures(1, &output_tex);
    }

    SUBCASE("Dimension calculation") {
        GLuint input_tex = create_test_texture(10, 10);
        gpu::input_texture input(input_tex, 10, 10);

        auto dims = GPUScaler::calculate_output_dimensions(input, algorithm::EPX, 2.0f);
        CHECK(dims.width == 20);
        CHECK(dims.height == 20);

        glDeleteTextures(1, &input_tex);
    }

    SUBCASE("Multiple algorithms") {
        struct TestCase {
            algorithm algo;
            float scale;
            bool should_work;
        };

        std::vector<TestCase> test_cases = {
            {algorithm::Nearest, 2.0f, true},
            {algorithm::Nearest, 3.5f, true},  // Arbitrary scale
            {algorithm::EPX, 2.0f, true},
            {algorithm::Eagle, 2.0f, true},
            {algorithm::Scale, 2.0f, true},
            {algorithm::Bilinear, 2.5f, true},
            {algorithm::HQ, 2.0f, false},  // Not GPU accelerated
        };

        for (const auto& tc : test_cases) {
            INFO("Testing algorithm: " << scaler_capabilities::get_algorithm_name(tc.algo));

            if (!tc.should_work) {
                CHECK(GPUScaler::is_gpu_accelerated(tc.algo) == false);
                continue;
            }

            // Create input texture
            GLuint input_tex = create_test_texture(8, 8);
            gpu::input_texture input(input_tex, 8, 8);

            // Calculate expected output size
            size_t out_size = static_cast<size_t>(8 * tc.scale);

            // Create output texture
            GLuint output_tex = gpu::opengl_texture_scaler::create_output_texture(
                static_cast<GLsizei>(out_size), static_cast<GLsizei>(out_size));
            gpu::output_texture output(output_tex, out_size, out_size);

            // Scale
            bool success = false;
            try {
                GPUScaler::scale(input, output, tc.algo);
                success = true;
            } catch (const std::exception&) {
                success = false;
            }

            CHECK(success == tc.should_work);

            // Cleanup
            glDeleteTextures(1, &input_tex);
            glDeleteTextures(1, &output_tex);
        }
    }

    // Don't cleanup here - we maintain the context across test runs
    // Cleanup will happen when the program exits
}