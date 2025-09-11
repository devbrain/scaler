#include <doctest/doctest.h>
#include <../include/scaler/sdl/sdl_image.hh>
#include <scaler/omniscale.hh>
#include <memory>
#include <vector>
#include <cstring>

// Include embedded test images
#include "rotozoom_bmp.h"
#include "rotozoom_omniscale_2x_bmp.h"
#include "rotozoom_omniscale_3x_bmp.h"
using namespace scaler;
// Helper to load BMP from embedded data
std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> 
loadBMPFromMemory(const unsigned char* data, unsigned int size) {
    SDL_IOStream* io = SDL_IOFromConstMem(data, size);
    if (!io) {
        return {nullptr, SDL_FreeSurface};
    }
    
    SDL_Surface* surface = SDL_LoadBMP_IO(io, true);
    if (!surface) {
        return {nullptr, SDL_FreeSurface};
    }
    
    // Convert to RGBA for consistency
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    
    return {rgba_surface, SDL_FreeSurface};
}

// Helper to compare two surfaces
bool compareSurfaces(SDL_Surface* surf1, SDL_Surface* surf2, int tolerance = 0) {
    if (!surf1 || !surf2) return false;
    if (surf1->w != surf2->w || surf1->h != surf2->h) return false;
    
    if (SDL_MUSTLOCK(surf1)) SDL_LockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_LockSurface(surf2);
    
    bool matches = true;
    Uint32* pixels1 = static_cast<Uint32*>(surf1->pixels);
    Uint32* pixels2 = static_cast<Uint32*>(surf2->pixels);
    
    int mismatches = 0;
    for (int i = 0; i < surf1->w * surf1->h && matches; ++i) {
        Uint8 r1, g1, b1, a1;
        Uint8 r2, g2, b2, a2;
        SDL_GetRGBA(pixels1[i], surf1->format, &r1, &g1, &b1, &a1);
        SDL_GetRGBA(pixels2[i], surf2->format, &r2, &g2, &b2, &a2);
        
        int dr = abs(r1 - r2);
        int dg = abs(g1 - g2);
        int db = abs(b1 - b2);
        int da = abs(a1 - a2);
        
        if (dr > tolerance || dg > tolerance || db > tolerance || da > tolerance) {
            if (mismatches < 5) { // Report first few mismatches
                INFO("Pixel mismatch at index " << i 
                     << " (x=" << (i % surf1->w) 
                     << ", y=" << (i / surf1->w) << ")");
                INFO("Surface1 RGBA: (" << static_cast<int>(r1) << ", " << static_cast<int>(g1) 
                     << ", " << static_cast<int>(b1) << ", " << static_cast<int>(a1) << ")");
                INFO("Surface2 RGBA: (" << static_cast<int>(r2) << ", " << static_cast<int>(g2) 
                     << ", " << static_cast<int>(b2) << ", " << static_cast<int>(a2) << ")");
            }
            mismatches++;
            if (tolerance == 0) {
                matches = false; // For exact comparison, fail on first mismatch
            }
        }
    }
    
    if (SDL_MUSTLOCK(surf1)) SDL_UnlockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_UnlockSurface(surf2);
    
    if (mismatches > 0 && tolerance > 0) {
        // For tolerant comparison, allow up to 5% pixel differences
        float mismatch_percentage = static_cast<float>(mismatches) / static_cast<float>(surf1->w * surf1->h) * 100.0f;
        INFO("Total mismatches: " << mismatches << " (" << mismatch_percentage << "%)");
        matches = (mismatch_percentage <= 5.0f);
    }
    
    return matches;
}

TEST_CASE("OmniScale Golden Data Tests") {
    // Initialize SDL
    static bool sdl_initialized = false;
    if (!sdl_initialized) {
        REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
        sdl_initialized = true;
    }
    
    // Load the source rotozoom image from embedded data
    auto source = loadBMPFromMemory(data_rotozoom_bmp, data_rotozoom_bmp_len);
    REQUIRE(source != nullptr);
    
    SUBCASE("OmniScale 2x matches golden BMP") {
        // Load the golden data from embedded BMP
        auto golden = loadBMPFromMemory(rotozoom_omniscale_2x_bmp_data, 
                                        rotozoom_omniscale_2x_bmp_len);
        REQUIRE(golden != nullptr);
        
        // Run OmniScale 2x on the source image
        SDLInputImage input(source.get());
        auto output = scaleOmniScale2x<SDLInputImage, SDLOutputImage>(input);
        
        // Check dimensions
        CHECK(output.width() == golden->w);
        CHECK(output.height() == golden->h);
        CHECK(output.width() == source->w * 2);
        CHECK(output.height() == source->h * 2);
        
        // Compare the output with the golden data
        // Use a reasonable tolerance since the shader and C++ implementations
        // can have different rounding and interpolation behavior
        INFO("Comparing surfaces: output=" << output.width() << "x" << output.height() 
             << " golden=" << golden->w << "x" << golden->h);
        bool matches = compareSurfaces(output.get_surface(), golden.get(), 10);
        if (!matches) {
            INFO("Surfaces do not match within tolerance=10");
            // Try with other tolerances to see how close we are
            bool matches5 = compareSurfaces(output.get_surface(), golden.get(), 5);
            bool matches20 = compareSurfaces(output.get_surface(), golden.get(), 20);
            bool matches50 = compareSurfaces(output.get_surface(), golden.get(), 50);
            INFO("Match with tolerance=5: " << matches5);
            INFO("Match with tolerance=20: " << matches20);
            INFO("Match with tolerance=50: " << matches50);
        }
        CHECK(matches);
    }
    
    SUBCASE("OmniScale 3x matches golden BMP") {
        // Load the golden data from embedded BMP
        auto golden = loadBMPFromMemory(rotozoom_omniscale_3x_bmp_data,
                                        rotozoom_omniscale_3x_bmp_len);
        REQUIRE(golden != nullptr);
        
        // Run OmniScale 3x on the source image
        SDLInputImage input(source.get());
        auto output = scaleOmniScale3x<SDLInputImage, SDLOutputImage>(input);
        
        // Check dimensions
        CHECK(output.width() == golden->w);
        CHECK(output.height() == golden->h);
        CHECK(output.width() == source->w * 3);
        CHECK(output.height() == source->h * 3);
        
        // Compare the output with the golden data
        // Use a reasonable tolerance since the shader and C++ implementations
        // can have different rounding and interpolation behavior
        INFO("Comparing surfaces: output=" << output.width() << "x" << output.height() 
             << " golden=" << golden->w << "x" << golden->h);
        bool matches = compareSurfaces(output.get_surface(), golden.get(), 10);
        if (!matches) {
            INFO("Surfaces do not match within tolerance=10");
            // Try with other tolerances to see how close we are
            bool matches5 = compareSurfaces(output.get_surface(), golden.get(), 5);
            bool matches20 = compareSurfaces(output.get_surface(), golden.get(), 20);
            bool matches50 = compareSurfaces(output.get_surface(), golden.get(), 50);
            INFO("Match with tolerance=5: " << matches5);
            INFO("Match with tolerance=20: " << matches20);
            INFO("Match with tolerance=50: " << matches50);
        }
        CHECK(matches);
    }
    
    SUBCASE("OmniScale 2x basic patterns") {
        // Test some basic patterns to ensure algorithm is working correctly
        
        // Create a small test pattern: 4x4 checkerboard
        SDL_Surface* pattern = SDL_CreateRGBSurfaceWithFormat(
            0, 4, 4, 32, SDL_PIXELFORMAT_RGBA8888);
        REQUIRE(pattern != nullptr);
        
        Uint32* pixels = static_cast<Uint32*>(pattern->pixels);
        Uint32 white = SDL_MapRGBA(pattern->format, 255, 255, 255, 255);
        Uint32 black = SDL_MapRGBA(pattern->format, 0, 0, 0, 255);
        
        // Create checkerboard
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                pixels[y * 4 + x] = ((x + y) % 2 == 0) ? white : black;
            }
        }
        
        // Scale it
        SDLInputImage input(pattern);
        auto output = scaleOmniScale2x<SDLInputImage, SDLOutputImage>(input);
        
        // Check dimensions
        CHECK(output.width() == 8);
        CHECK(output.height() == 8);
        
        // The output should maintain the checkerboard pattern
        // but with smooth transitions at edges
        SDL_FreeSurface(pattern);
    }
    
    SUBCASE("OmniScale 3x basic patterns") {
        // Similar test for 3x scaling
        SDL_Surface* pattern = SDL_CreateRGBSurfaceWithFormat(
            0, 3, 3, 32, SDL_PIXELFORMAT_RGBA8888);
        REQUIRE(pattern != nullptr);
        
        Uint32* pixels = static_cast<Uint32*>(pattern->pixels);
        Uint32 white = SDL_MapRGBA(pattern->format, 255, 255, 255, 255);
        Uint32 black = SDL_MapRGBA(pattern->format, 0, 0, 0, 255);
        
        // Create a cross pattern
        for (int i = 0; i < 9; i++) pixels[i] = black;
        pixels[1] = white; // top center
        pixels[3] = white; // middle left
        pixels[4] = white; // center
        pixels[5] = white; // middle right
        pixels[7] = white; // bottom center
        
        // Scale it
        SDLInputImage input(pattern);
        auto output = scaleOmniScale3x<SDLInputImage, SDLOutputImage>(input);
        
        // Check dimensions
        CHECK(output.width() == 9);
        CHECK(output.height() == 9);
        
        SDL_FreeSurface(pattern);
    }
}