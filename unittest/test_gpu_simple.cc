#include <doctest/doctest.h>
#include <scaler/gpu/sdl_opengl_multi_scaler.hh>
#include <../include/scaler/cpu/epx.hh>
#include <scaler/sdl/sdl_image.hh>
#include <SDL.h>
#include <memory>
#include <cmath>

using namespace scaler;
using namespace scaler::gpu;

// Create simple test pattern
SDL_Surface* createTestPattern(int size = 8) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, size, size, 32, SDL_PIXELFORMAT_RGBA8888);

    if (!surface) return nullptr;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    Uint32* pixels = static_cast<Uint32*>(surface->pixels);

    // Create a simple checkerboard pattern
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool checker = ((x / 2) + (y / 2)) % 2;
            Uint8 val = checker ? 255 : 0;

            // Add some color variation
            Uint8 r = val;
            Uint8 g = static_cast<Uint8>((x * 255) / size);
            Uint8 b = static_cast<Uint8>((y * 255) / size);

            pixels[y * size + x] = SDL_MapRGBA(surface->format, r, g, b, 255);
        }
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    return surface;
}

// Simple comparison function
bool compareSurfacesSimple(SDL_Surface* surf1, SDL_Surface* surf2, int tolerance = 2) {
    if (!surf1 || !surf2) return false;
    if (surf1->w != surf2->w || surf1->h != surf2->h) return false;

    if (SDL_MUSTLOCK(surf1)) SDL_LockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_LockSurface(surf2);

    Uint32* pixels1 = static_cast<Uint32*>(surf1->pixels);
    Uint32* pixels2 = static_cast<Uint32*>(surf2->pixels);

    int total_pixels = surf1->w * surf1->h;
    int mismatches = 0;

    for (int i = 0; i < total_pixels; ++i) {
        Uint8 r1, g1, b1, a1, r2, g2, b2, a2;
        SDL_GetRGBA(pixels1[i], surf1->format, &r1, &g1, &b1, &a1);
        SDL_GetRGBA(pixels2[i], surf2->format, &r2, &g2, &b2, &a2);

        int dr = abs(r1 - r2);
        int dg = abs(g1 - g2);
        int db = abs(b1 - b2);

        if (dr > tolerance || dg > tolerance || db > tolerance) {
            mismatches++;
        }
    }

    if (SDL_MUSTLOCK(surf1)) SDL_UnlockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_UnlockSurface(surf2);

    // Allow up to 1% pixel difference due to floating point precision
    double mismatch_ratio = static_cast<double>(mismatches) / total_pixels;
    if (mismatch_ratio > 0.01) {
        INFO("Mismatch ratio: " << mismatch_ratio * 100 << "%");
        INFO("Mismatches: " << mismatches << " / " << total_pixels);
    }

    return mismatch_ratio <= 0.01;
}

TEST_CASE("Simple GPU EPX Test") {
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

    if (!window) {
        INFO("Could not create OpenGL window - skipping GPU tests");
        SDL_Quit();
        return;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        INFO("Could not create OpenGL context - skipping GPU tests");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    SUBCASE("Basic EPX GPU vs CPU") {
        // Create small test pattern
        SDL_Surface* input = createTestPattern(8);
        REQUIRE(input != nullptr);


        // CPU EPX scaling
        SDLInputImage input_img(input);
        auto cpu_result = scale_epx<SDLInputImage, SDLOutputImage>(input_img);
        SDL_Surface* cpu_surface = cpu_result.get_surface();
        REQUIRE(cpu_surface != nullptr);

        // GPU EPX scaling
        sdl_opengl_multi_scaler gpu_scaler;
        bool init_success = gpu_scaler.initialize(window);

        if (!init_success) {
            INFO("GPU scaler initialization failed - OpenGL may not be available");
            SDL_FreeSurface(input);
            SDL_GL_DeleteContext(gl_context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        SDL_Surface* gpu_surface = gpu_scaler.scale_surface(input, 2.0f, sdl_opengl_multi_scaler::EPX);
        REQUIRE(gpu_surface != nullptr);

        // Compare dimensions
        CHECK(gpu_surface->w == cpu_surface->w);
        CHECK(gpu_surface->h == cpu_surface->h);
        CHECK(gpu_surface->w == 16);  // 8 * 2
        CHECK(gpu_surface->h == 16);  // 8 * 2


        // Convert GPU surface to same format as CPU surface if needed
        SDL_Surface* gpu_surface_converted = gpu_surface;
        if (gpu_surface->format->format != cpu_surface->format->format) {
            gpu_surface_converted = SDL_ConvertSurface(gpu_surface, cpu_surface->format, 0);
            SDL_FreeSurface(gpu_surface);
            gpu_surface = gpu_surface_converted;
        }

        // Compare pixels (allow small tolerance for GPU precision)
        bool match = compareSurfacesSimple(cpu_surface, gpu_surface, 2);

        CHECK(match);

        // Cleanup
        SDL_FreeSurface(gpu_surface);
        SDL_FreeSurface(input);
    }

    SUBCASE("Different Sizes") {
        for (int size : {4, 8, 16, 32}) {
            SDL_Surface* input = createTestPattern(size);
            REQUIRE(input != nullptr);

            sdl_opengl_multi_scaler gpu_scaler;
            bool init_success = gpu_scaler.initialize(window);

            if (!init_success) {
                SDL_FreeSurface(input);
                continue;
            }

            SDL_Surface* gpu_surface = gpu_scaler.scale_surface(input, 2.0f, sdl_opengl_multi_scaler::EPX);

            if (gpu_surface) {
                CHECK(gpu_surface->w == size * 2);
                CHECK(gpu_surface->h == size * 2);
                SDL_FreeSurface(gpu_surface);
            }

            SDL_FreeSurface(input);
        }
    }

    // Cleanup
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}