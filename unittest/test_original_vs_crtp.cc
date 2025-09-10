#include <doctest/doctest.h>
#include <scaler/sdl_image.hh>
#include <scaler/sdl_scalers.hh>
#include <scaler/epx_crtp.hh>
#include <scaler/epx.hh>
#include <memory>
#include <string>

// Include embedded BMP data
#include "rotozoom_bmp.h"

// Helper to load test image from memory
std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> loadTestImage() {
    SDL_IOStream* io = SDL_IOFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    if (!io) {
        return {nullptr, SDL_FreeSurface};
    }
    SDL_Surface* surface = SDL_LoadBMP_IO(io, true); // true = close IO after load
    return {surface, SDL_FreeSurface};
}

// Helper to compare two surfaces pixel by pixel
bool compareSurfaces(SDL_Surface* surf1, SDL_Surface* surf2) {
    if (!surf1 || !surf2) return false;
    if (surf1->w != surf2->w || surf1->h != surf2->h) return false;
    
    // Lock surfaces if needed
    if (SDL_MUSTLOCK(surf1)) SDL_LockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_LockSurface(surf2);
    
    bool identical = true;
    for (int y = 0; y < surf1->h && identical; ++y) {
        for (int x = 0; x < surf1->w && identical; ++x) {
            Uint8 r1, g1, b1, a1;
            Uint8 r2, g2, b2, a2;
            
            Uint32* pixels1 = (Uint32*)surf1->pixels;
            Uint32* pixels2 = (Uint32*)surf2->pixels;
            
            Uint32 pixel1 = pixels1[y * surf1->w + x];
            Uint32 pixel2 = pixels2[y * surf2->w + x];
            
            SDL_GetRGBA(pixel1, surf1->format, &r1, &g1, &b1, &a1);
            SDL_GetRGBA(pixel2, surf2->format, &r2, &g2, &b2, &a2);
            
            if (r1 != r2 || g1 != g2 || b1 != b2 || a1 != a2) {
                identical = false;
            }
        }
    }
    
    if (SDL_MUSTLOCK(surf1)) SDL_UnlockSurface(surf1);
    if (SDL_MUSTLOCK(surf2)) SDL_UnlockSurface(surf2);
    
    return identical;
}

TEST_CASE("Original vs CRTP EPX Implementation Comparison") {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        FAIL("SDL initialization failed: " << SDL_GetError());
    }
    
    SUBCASE("EPX scaling produces identical results") {
        // Load the test BMP image from embedded data
        auto input_surface = loadTestImage();
        
        REQUIRE(input_surface != nullptr);
        
        // Convert to 32-bit RGBA format for consistency
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(input_surface.get(), SDL_PIXELFORMAT_RGBA8888, 0);
        REQUIRE(converted != nullptr);
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> input_32bit(converted, SDL_FreeSurface);
        
        // Scale using original EPX implementation
        input_image<uvec3> original_input(input_32bit.get());
        SDL_Surface* original_output = scaleEpx(original_input);
        REQUIRE(original_output != nullptr);
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> original_result(original_output, SDL_FreeSurface);
        
        // Scale using CRTP EPX implementation
        SDLInputImage input_img(input_32bit.get());
        auto output_img = scaleEpx<SDLInputImage, SDLOutputImage>(input_img);
        SDL_Surface* crtp_output = output_img.get_surface();
        
        // Compare the results
        CHECK(compareSurfaces(original_output, crtp_output));
        
        // Additional checks for dimensions
        CHECK(original_output->w == crtp_output->w);
        CHECK(original_output->h == crtp_output->h);
        CHECK(original_output->w == input_32bit->w * 2);
        CHECK(original_output->h == input_32bit->h * 2);
        
        // Sample specific pixels for detailed comparison
        if (SDL_MUSTLOCK(original_output)) SDL_LockSurface(original_output);
        if (SDL_MUSTLOCK(crtp_output)) SDL_LockSurface(crtp_output);
        
        // Check corners and center
        int test_positions[][2] = {
            {0, 0},                                    // Top-left
            {original_output->w - 1, 0},              // Top-right
            {0, original_output->h - 1},              // Bottom-left
            {original_output->w - 1, original_output->h - 1}, // Bottom-right
            {original_output->w / 2, original_output->h / 2}  // Center
        };
        
        for (auto& pos : test_positions) {
            int x = pos[0];
            int y = pos[1];
            
            Uint32* orig_pixels = (Uint32*)original_output->pixels;
            Uint32* crtp_pixels = (Uint32*)crtp_output->pixels;
            
            Uint32 orig_pixel = orig_pixels[y * original_output->w + x];
            Uint32 crtp_pixel = crtp_pixels[y * crtp_output->w + x];
            
            Uint8 r1, g1, b1, a1;
            Uint8 r2, g2, b2, a2;
            
            SDL_GetRGBA(orig_pixel, original_output->format, &r1, &g1, &b1, &a1);
            SDL_GetRGBA(crtp_pixel, crtp_output->format, &r2, &g2, &b2, &a2);
            
            INFO("Pixel at (" << x << ", " << y << "):");
            CHECK(r1 == r2);
            CHECK(g1 == g2);
            CHECK(b1 == b2);
            CHECK(a1 == a2);
        }
        
        if (SDL_MUSTLOCK(original_output)) SDL_UnlockSurface(original_output);
        if (SDL_MUSTLOCK(crtp_output)) SDL_UnlockSurface(crtp_output);
    }
    
    SUBCASE("EPX scaling with small synthetic image") {
        // Create a small test pattern
        SDL_Surface* small_input = SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_RGBA8888);
        REQUIRE(small_input != nullptr);
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> input(small_input, SDL_FreeSurface);
        
        // Fill with a pattern
        if (SDL_MUSTLOCK(small_input)) SDL_LockSurface(small_input);
        
        Uint32* pixels = (Uint32*)small_input->pixels;
        pixels[0] = SDL_MapRGBA(small_input->format, 255, 0, 0, 255);   // Red
        pixels[1] = SDL_MapRGBA(small_input->format, 0, 255, 0, 255);   // Green
        pixels[2] = SDL_MapRGBA(small_input->format, 0, 0, 255, 255);   // Blue
        pixels[3] = SDL_MapRGBA(small_input->format, 255, 255, 0, 255); // Yellow
        
        if (SDL_MUSTLOCK(small_input)) SDL_UnlockSurface(small_input);
        
        // Scale using original EPX
        input_image<uvec3> original_input(small_input);
        SDL_Surface* original_output = scaleEpx(original_input);
        REQUIRE(original_output != nullptr);
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> original_result(original_output, SDL_FreeSurface);
        
        // Scale using CRTP EPX
        SDLInputImage input_img(small_input);
        auto output_img = scaleEpx<SDLInputImage, SDLOutputImage>(input_img);
        SDL_Surface* crtp_output = output_img.get_surface();
        
        // Compare results
        CHECK(compareSurfaces(original_output, crtp_output));
        
        // Verify all pixels match exactly
        if (SDL_MUSTLOCK(original_output)) SDL_LockSurface(original_output);
        if (SDL_MUSTLOCK(crtp_output)) SDL_LockSurface(crtp_output);
        
        Uint32* orig_pixels = (Uint32*)original_output->pixels;
        Uint32* crtp_pixels = (Uint32*)crtp_output->pixels;
        
        for (int i = 0; i < 16; ++i) {
            CHECK(orig_pixels[i] == crtp_pixels[i]);
        }
        
        if (SDL_MUSTLOCK(original_output)) SDL_UnlockSurface(original_output);
        if (SDL_MUSTLOCK(crtp_output)) SDL_UnlockSurface(crtp_output);
    }
    
    SDL_Quit();
}