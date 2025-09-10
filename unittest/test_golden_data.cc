#include <doctest/doctest.h>
#include <scaler/sdl_image.hh>
#include <scaler/epx_crtp.hh>
#include <scaler/eagle_crtp.hh>
#include <scaler/2xsai_crtp.hh>
#include <scaler/xbr_crtp.hh>
#include <scaler/hq2x_crtp.hh>
#include <memory>
#include <vector>
#include <cstring>

// Include embedded test image
#include "rotozoom_bmp.h"

// Include golden data for test patterns (smaller, faster tests)
#include "golden_test_pattern_source.h"
#include "golden_test_pattern_epx.h"
#include "golden_test_pattern_eagle.h"
#include "golden_test_pattern_2xsai.h"
#include "golden_test_pattern_xbr.h"
#include "golden_test_pattern_hq2x.h"

// Helper to create SDL surface from raw RGBA data
std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> 
createSurfaceFromData(const unsigned char* data, int width, int height) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    
    if (!surface) return {nullptr, SDL_FreeSurface};
    
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    
    // Copy pixel data
    Uint32* pixels = (Uint32*)surface->pixels;
    for (int i = 0; i < width * height; ++i) {
        Uint8 r = data[i * 4 + 0];
        Uint8 g = data[i * 4 + 1];
        Uint8 b = data[i * 4 + 2];
        Uint8 a = data[i * 4 + 3];
        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
    }
    
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    
    return {surface, SDL_FreeSurface};
}

// Helper to compare surface with golden data
bool compareSurfaceWithGolden(SDL_Surface* surface, 
                              const unsigned char* golden_data,
                              int expected_width, int expected_height,
                              int tolerance = 0) {
    if (!surface) return false;
    if (surface->w != expected_width || surface->h != expected_height) return false;
    
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    
    bool matches = true;
    Uint32* pixels = (Uint32*)surface->pixels;
    
    for (int i = 0; i < expected_width * expected_height && matches; ++i) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], surface->format, &r, &g, &b, &a);
        
        Uint8 expected_r = golden_data[i * 4 + 0];
        Uint8 expected_g = golden_data[i * 4 + 1];
        Uint8 expected_b = golden_data[i * 4 + 2];
        Uint8 expected_a = golden_data[i * 4 + 3];
        
        int dr = abs(r - expected_r);
        int dg = abs(g - expected_g);
        int db = abs(b - expected_b);
        int da = abs(a - expected_a);
        
        if (dr > tolerance || dg > tolerance || db > tolerance || da > tolerance) {
            if (tolerance == 0) {
                // For exact comparison, report first mismatch
                INFO("Pixel mismatch at index " << i 
                     << " (x=" << (i % expected_width) 
                     << ", y=" << (i / expected_width) << ")");
                INFO("Expected RGBA: (" << (int)expected_r << ", " << (int)expected_g 
                     << ", " << (int)expected_b << ", " << (int)expected_a << ")");
                INFO("Got RGBA: (" << (int)r << ", " << (int)g 
                     << ", " << (int)b << ", " << (int)a << ")");
            }
            matches = false;
        }
    }
    
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    
    return matches;
}

TEST_CASE("Golden Data Tests - Test Pattern") {
    // Initialize SDL
    static bool sdl_initialized = false;
    if (!sdl_initialized) {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        sdl_initialized = true;
    }
    
    // Create source surface from golden test pattern
    auto source = createSurfaceFromData(
        GOLDEN_TEST_PATTERN_SOURCE_DATA,
        GOLDEN_TEST_PATTERN_SOURCE_WIDTH,
        GOLDEN_TEST_PATTERN_SOURCE_HEIGHT);
    
    REQUIRE(source != nullptr);
    
    SUBCASE("EPX algorithm matches golden data") {
        SDLInputImage input(source.get());
        auto output = scaleEpx<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_TEST_PATTERN_EPX_WIDTH);
        CHECK(output.height() == GOLDEN_TEST_PATTERN_EPX_HEIGHT);
        
        CHECK(compareSurfaceWithGolden(
            output.get_surface(),
            GOLDEN_TEST_PATTERN_EPX_DATA,
            GOLDEN_TEST_PATTERN_EPX_WIDTH,
            GOLDEN_TEST_PATTERN_EPX_HEIGHT));
    }
    
    SUBCASE("Eagle algorithm matches golden data") {
        SDLInputImage input(source.get());
        auto output = scaleEagle<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_TEST_PATTERN_EAGLE_WIDTH);
        CHECK(output.height() == GOLDEN_TEST_PATTERN_EAGLE_HEIGHT);
        
        CHECK(compareSurfaceWithGolden(
            output.get_surface(),
            GOLDEN_TEST_PATTERN_EAGLE_DATA,
            GOLDEN_TEST_PATTERN_EAGLE_WIDTH,
            GOLDEN_TEST_PATTERN_EAGLE_HEIGHT));
    }
    
    SUBCASE("2xSaI algorithm matches golden data") {
        SDLInputImage input(source.get());
        auto output = scale2xSaI<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_TEST_PATTERN_2XSAI_WIDTH);
        CHECK(output.height() == GOLDEN_TEST_PATTERN_2XSAI_HEIGHT);
        
        CHECK(compareSurfaceWithGolden(
            output.get_surface(),
            GOLDEN_TEST_PATTERN_2XSAI_DATA,
            GOLDEN_TEST_PATTERN_2XSAI_WIDTH,
            GOLDEN_TEST_PATTERN_2XSAI_HEIGHT));
    }
    
    SUBCASE("XBR algorithm matches golden data") {
        SDLInputImage input(source.get());
        auto output = scaleXbr<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_TEST_PATTERN_XBR_WIDTH);
        CHECK(output.height() == GOLDEN_TEST_PATTERN_XBR_HEIGHT);
        
        CHECK(compareSurfaceWithGolden(
            output.get_surface(),
            GOLDEN_TEST_PATTERN_XBR_DATA,
            GOLDEN_TEST_PATTERN_XBR_WIDTH,
            GOLDEN_TEST_PATTERN_XBR_HEIGHT));
    }
    
    SUBCASE("HQ2x algorithm matches golden data") {
        SDLInputImage input(source.get());
        auto output = scaleHq2x<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_TEST_PATTERN_HQ2X_WIDTH);
        CHECK(output.height() == GOLDEN_TEST_PATTERN_HQ2X_HEIGHT);
        
        CHECK(compareSurfaceWithGolden(
            output.get_surface(),
            GOLDEN_TEST_PATTERN_HQ2X_DATA,
            GOLDEN_TEST_PATTERN_HQ2X_WIDTH,
            GOLDEN_TEST_PATTERN_HQ2X_HEIGHT));
    }
}

// Test with full rotozoom image (only spot checks for performance)
TEST_CASE("Golden Data Tests - Full Image Spot Checks") {
    // Initialize SDL
    static bool sdl_initialized = false;
    if (!sdl_initialized) {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        sdl_initialized = true;
    }
    
    // Load the embedded BMP
    SDL_IOStream* io = SDL_IOFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    REQUIRE(io != nullptr);
    
    SDL_Surface* surface = SDL_LoadBMP_IO(io, true);
    REQUIRE(surface != nullptr);
    
    // Convert to RGBA
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    REQUIRE(rgba_surface != nullptr);
    
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> source(rgba_surface, SDL_FreeSurface);
    
    // Helper to check specific pixels from golden data
    auto checkPixels = [](SDL_Surface* surf, const unsigned char* golden, 
                          int width, int height, 
                          const std::vector<int>& indices) {
        if (SDL_MUSTLOCK(surf)) SDL_LockSurface(surf);
        Uint32* pixels = (Uint32*)surf->pixels;
        
        bool all_match = true;
        for (int idx : indices) {
            if (idx >= width * height) continue;
            
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixels[idx], surf->format, &r, &g, &b, &a);
            
            Uint8 expected_r = golden[idx * 4 + 0];
            Uint8 expected_g = golden[idx * 4 + 1];
            Uint8 expected_b = golden[idx * 4 + 2];
            Uint8 expected_a = golden[idx * 4 + 3];
            
            if (r != expected_r || g != expected_g || b != expected_b || a != expected_a) {
                INFO("Pixel mismatch at index " << idx);
                INFO("Expected: (" << (int)expected_r << ", " << (int)expected_g 
                     << ", " << (int)expected_b << ", " << (int)expected_a << ")");
                INFO("Got: (" << (int)r << ", " << (int)g 
                     << ", " << (int)b << ", " << (int)a << ")");
                all_match = false;
            }
        }
        
        if (SDL_MUSTLOCK(surf)) SDL_UnlockSurface(surf);
        return all_match;
    };
    
    // Test indices: corners, center, and a few random points
    std::vector<int> test_indices;
    
    SUBCASE("EPX full image spot check") {
        // Include the full golden data for spot checks
        #include "golden_epx.h"
        
        SDLInputImage input(source.get());
        auto output = scaleEpx<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_EPX_WIDTH);
        CHECK(output.height() == GOLDEN_EPX_HEIGHT);
        
        // Check corners and center
        test_indices = {
            0,                                          // Top-left
            GOLDEN_EPX_WIDTH - 1,                      // Top-right
            (GOLDEN_EPX_HEIGHT - 1) * GOLDEN_EPX_WIDTH, // Bottom-left
            GOLDEN_EPX_WIDTH * GOLDEN_EPX_HEIGHT - 1,   // Bottom-right
            GOLDEN_EPX_WIDTH * GOLDEN_EPX_HEIGHT / 2    // Center
        };
        
        CHECK(checkPixels(output.get_surface(), GOLDEN_EPX_DATA, 
                         GOLDEN_EPX_WIDTH, GOLDEN_EPX_HEIGHT, test_indices));
    }
    
    SUBCASE("Eagle full image spot check") {
        #include "golden_eagle.h"
        
        SDLInputImage input(source.get());
        auto output = scaleEagle<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_EAGLE_WIDTH);
        CHECK(output.height() == GOLDEN_EAGLE_HEIGHT);
        
        test_indices = {
            0,
            GOLDEN_EAGLE_WIDTH - 1,
            (GOLDEN_EAGLE_HEIGHT - 1) * GOLDEN_EAGLE_WIDTH,
            GOLDEN_EAGLE_WIDTH * GOLDEN_EAGLE_HEIGHT - 1,
            GOLDEN_EAGLE_WIDTH * GOLDEN_EAGLE_HEIGHT / 2
        };
        
        CHECK(checkPixels(output.get_surface(), GOLDEN_EAGLE_DATA,
                         GOLDEN_EAGLE_WIDTH, GOLDEN_EAGLE_HEIGHT, test_indices));
    }
    
    SUBCASE("2xSaI full image spot check") {
        #include "golden_2xsai.h"
        
        SDLInputImage input(source.get());
        auto output = scale2xSaI<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_2XSAI_WIDTH);
        CHECK(output.height() == GOLDEN_2XSAI_HEIGHT);
        
        test_indices = {
            0,
            GOLDEN_2XSAI_WIDTH - 1,
            (GOLDEN_2XSAI_HEIGHT - 1) * GOLDEN_2XSAI_WIDTH,
            GOLDEN_2XSAI_WIDTH * GOLDEN_2XSAI_HEIGHT - 1,
            GOLDEN_2XSAI_WIDTH * GOLDEN_2XSAI_HEIGHT / 2
        };
        
        CHECK(checkPixels(output.get_surface(), GOLDEN_2XSAI_DATA,
                         GOLDEN_2XSAI_WIDTH, GOLDEN_2XSAI_HEIGHT, test_indices));
    }
    
    SUBCASE("XBR full image spot check") {
        #include "golden_xbr.h"
        
        SDLInputImage input(source.get());
        auto output = scaleXbr<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_XBR_WIDTH);
        CHECK(output.height() == GOLDEN_XBR_HEIGHT);
        
        test_indices = {
            0,
            GOLDEN_XBR_WIDTH - 1,
            (GOLDEN_XBR_HEIGHT - 1) * GOLDEN_XBR_WIDTH,
            GOLDEN_XBR_WIDTH * GOLDEN_XBR_HEIGHT - 1,
            GOLDEN_XBR_WIDTH * GOLDEN_XBR_HEIGHT / 2
        };
        
        CHECK(checkPixels(output.get_surface(), GOLDEN_XBR_DATA,
                         GOLDEN_XBR_WIDTH, GOLDEN_XBR_HEIGHT, test_indices));
    }
    
    SUBCASE("HQ2x full image spot check") {
        #include "golden_hq2x.h"
        
        SDLInputImage input(source.get());
        auto output = scaleHq2x<SDLInputImage, SDLOutputImage>(input);
        
        CHECK(output.width() == GOLDEN_HQ2X_WIDTH);
        CHECK(output.height() == GOLDEN_HQ2X_HEIGHT);
        
        test_indices = {
            0,
            GOLDEN_HQ2X_WIDTH - 1,
            (GOLDEN_HQ2X_HEIGHT - 1) * GOLDEN_HQ2X_WIDTH,
            GOLDEN_HQ2X_WIDTH * GOLDEN_HQ2X_HEIGHT - 1,
            GOLDEN_HQ2X_WIDTH * GOLDEN_HQ2X_HEIGHT / 2
        };
        
        CHECK(checkPixels(output.get_surface(), GOLDEN_HQ2X_DATA,
                         GOLDEN_HQ2X_WIDTH, GOLDEN_HQ2X_HEIGHT, test_indices));
    }
}