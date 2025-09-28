#include <doctest/doctest.h>
#include "gpu_test_helper.hh"
#include <../include/scaler/cpu/epx.hh>
#include <scaler/sdl/sdl_image.hh>
#include <SDL.h>
#include <memory>
#include <cmath>

// Test data
#include "data/golden_test_pattern_source.h"
#include "data/golden_test_pattern_epx.h"

using namespace scaler;
using namespace scaler::gpu::test;

// Helper to create surface from raw data
static SDL_Surface* createSurfaceFromRawData(const unsigned char* data, int width, int height) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);

    if (!surface) return nullptr;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    Uint32* pixels = static_cast<Uint32*>(surface->pixels);
    for (int i = 0; i < width * height; ++i) {
        Uint8 r = data[i * 4 + 0];
        Uint8 g = data[i * 4 + 1];
        Uint8 b = data[i * 4 + 2];
        Uint8 a = data[i * 4 + 3];
        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    return surface;
}

// Compare two surfaces pixel by pixel
static bool compareSurfaces(SDL_Surface* surf1, SDL_Surface* surf2, int tolerance = 1) {
    if (!surf1 || !surf2) return false;
    if (surf1->w != surf2->w || surf1->h != surf2->h) return false;

    if (SDL_MUSTLOCK(surf1)) SDL_LockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_LockSurface(surf2);

    bool matches = true;
    Uint32* pixels1 = static_cast<Uint32*>(surf1->pixels);
    Uint32* pixels2 = static_cast<Uint32*>(surf2->pixels);

    int mismatches = 0;

    for (int i = 0; i < surf1->w * surf1->h; ++i) {
        Uint8 r1, g1, b1, a1, r2, g2, b2, a2;
        SDL_GetRGBA(pixels1[i], surf1->format, &r1, &g1, &b1, &a1);
        SDL_GetRGBA(pixels2[i], surf2->format, &r2, &g2, &b2, &a2);

        int dr = abs(r1 - r2);
        int dg = abs(g1 - g2);
        int db = abs(b1 - b2);
        int da = abs(a1 - a2);


        if (dr > tolerance || dg > tolerance || db > tolerance || da > tolerance) {
            if (mismatches < 10) {  // Report first few mismatches
                INFO("Pixel " << i << " mismatch: "
                     << "(" << static_cast<int>(r1) << "," << static_cast<int>(g1) << "," << static_cast<int>(b1) << "," << static_cast<int>(a1) << ") vs "
                     << "(" << static_cast<int>(r2) << "," << static_cast<int>(g2) << "," << static_cast<int>(b2) << "," << static_cast<int>(a2) << ")");
            }
            mismatches++;
            matches = false;
        }
    }

    if (!matches) {
        INFO("Total pixel mismatches: " << mismatches << " / " << (surf1->w * surf1->h));
    }

    if (SDL_MUSTLOCK(surf1)) SDL_UnlockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_UnlockSurface(surf2);

    return matches;
}

TEST_CASE("GPU EPX Implementation") {
    // Initialize SDL with video and OpenGL
    REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create hidden window with OpenGL context
    SDL_Window* window = SDL_CreateWindow(
        "GPU Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
    );
    REQUIRE(window != nullptr);

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    REQUIRE(gl_context != nullptr);

    SUBCASE("GPU EPX vs CPU EPX - Golden Data") {
        // Create test surface from golden data
        SDL_Surface* input = createSurfaceFromRawData(
            GOLDEN_TEST_PATTERN_SOURCE_DATA,
            GOLDEN_TEST_PATTERN_SOURCE_WIDTH,
            GOLDEN_TEST_PATTERN_SOURCE_HEIGHT
        );
        REQUIRE(input != nullptr);

        // CPU EPX scaling
        SDLInputImage input_img(input);
        auto cpu_result = scale_epx<SDLInputImage, SDLOutputImage>(input_img);
        SDL_Surface* cpu_surface = cpu_result.get_surface();
        REQUIRE(cpu_surface != nullptr);

        // GPU EPX scaling
        gpu_test_helper gpu_helper;
        REQUIRE(gpu_helper.initialize());

        SDL_Surface* gpu_surface = gpu_helper.scale_surface(input, algorithm::EPX, 2.0f);
        REQUIRE(gpu_surface != nullptr);

        // Compare dimensions
        CHECK(gpu_surface->w == cpu_surface->w);
        CHECK(gpu_surface->h == cpu_surface->h);
        CHECK(gpu_surface->w == GOLDEN_TEST_PATTERN_EPX_WIDTH);
        CHECK(gpu_surface->h == GOLDEN_TEST_PATTERN_EPX_HEIGHT);

        // Compare against CPU implementation
        bool cpu_gpu_match = compareSurfaces(cpu_surface, gpu_surface, 1);
        CHECK(cpu_gpu_match);

        // Compare GPU result against golden data
        SDL_Surface* golden = createSurfaceFromRawData(
            GOLDEN_TEST_PATTERN_EPX_DATA,
            GOLDEN_TEST_PATTERN_EPX_WIDTH,
            GOLDEN_TEST_PATTERN_EPX_HEIGHT
        );
        REQUIRE(golden != nullptr);

        bool gpu_golden_match = compareSurfaces(gpu_surface, golden, 1);
        CHECK(gpu_golden_match);

        // Cleanup
        SDL_FreeSurface(golden);
        SDL_FreeSurface(gpu_surface);
        SDL_FreeSurface(input);
    }

    SUBCASE("GPU EPX Performance") {
        // Create a larger test image
        int test_size = 256;
        SDL_Surface* input = SDL_CreateRGBSurfaceWithFormat(
            0, test_size, test_size, 32, SDL_PIXELFORMAT_RGBA8888);
        REQUIRE(input != nullptr);

        // Fill with test pattern
        if (SDL_MUSTLOCK(input)) SDL_LockSurface(input);
        Uint32* pixels = static_cast<Uint32*>(input->pixels);
        for (int y = 0; y < test_size; ++y) {
            for (int x = 0; x < test_size; ++x) {
                // Checkerboard pattern
                Uint8 val = ((x / 8) + (y / 8)) % 2 ? 255 : 0;
                pixels[y * test_size + x] = SDL_MapRGBA(input->format, val, val, val, 255);
            }
        }
        if (SDL_MUSTLOCK(input)) SDL_UnlockSurface(input);

        const int iterations = 50;

        // CPU timing
        Uint64 cpu_start = SDL_GetPerformanceCounter();
        for (int i = 0; i < iterations; ++i) {
            SDLInputImage input_img(input);
            auto result = scale_epx<SDLInputImage, SDLOutputImage>(input_img);
        }
        Uint64 cpu_end = SDL_GetPerformanceCounter();
        double cpu_ms = (static_cast<double>(cpu_end - cpu_start) * 1000.0) / static_cast<double>(SDL_GetPerformanceFrequency());

        // GPU timing
        gpu_test_helper gpu_helper;
        REQUIRE(gpu_helper.initialize());

        Uint64 gpu_start = SDL_GetPerformanceCounter();
        for (int i = 0; i < iterations; ++i) {
            SDL_Surface* result = gpu_helper.scale_surface(input, algorithm::EPX, 2.0f);
            SDL_FreeSurface(result);
        }
        Uint64 gpu_end = SDL_GetPerformanceCounter();
        double gpu_ms = (static_cast<double>(gpu_end - gpu_start) * 1000.0) / static_cast<double>(SDL_GetPerformanceFrequency());

        INFO("CPU EPX time: " << cpu_ms / iterations << " ms/frame");
        INFO("GPU EPX time: " << gpu_ms / iterations << " ms/frame");
        INFO("Speedup: " << cpu_ms / gpu_ms << "x");

        // GPU should provide some speedup (or at least not be much slower)
        CHECK(gpu_ms < cpu_ms * 2.0);  // Allow some overhead for small images

        SDL_FreeSurface(input);
    }

    // Cleanup
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}