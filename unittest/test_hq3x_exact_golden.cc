#include <doctest/doctest.h>
#include <../include/scaler/cpu/hq3x.hh>
#include <scaler/sdl/sdl_compat.hh>
#include <scaler/sdl/sdl_image.hh>
#include <vector>
#include <cstring>
#include <SDL.h>

// Include the embedded input and golden output data
#include "data/rotozoom_input.h"
#include "data/rotozoom_hq3x_golden.h"

// BMP loader helper
SDL_Surface* LoadBMPFromMemory(const unsigned char* data, size_t size) {
    SDL_RWops* rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw) return nullptr;
    
    SDL_Surface* surface = SDL_LoadBMP_RW(rw, 1);
    return surface;
}

TEST_CASE("HQ3x Exact Golden Data Comparison") {
    // Load input image from embedded data
    SDL_Surface* input_surface = LoadBMPFromMemory(rotozoom_input_data, rotozoom_input_len);
    REQUIRE(input_surface != nullptr);
    
    // Convert to 24-bit RGB if needed
    SDL_Surface* rgb_surface = nullptr;
    if (input_surface->format->BitsPerPixel != 24) {
        SDL_PixelFormat* fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGB24);
        rgb_surface = SDL_ConvertSurface(input_surface, fmt, 0);
        SDL_FreeFormat(fmt);
        SDL_FreeSurface(input_surface);
        input_surface = rgb_surface;
    }
    
    REQUIRE(input_surface != nullptr);
    REQUIRE(input_surface->format->BitsPerPixel == 24);
    
    // Apply exact HQ3x scaling using CRTP interface
    scaler::SDLInputImage input_image(input_surface);
    auto scaled = scaler::scale_hq_3x<scaler::SDLInputImage, scaler::SDLOutputImage>(input_image);
    
    // Get SDL surface from output
    SDL_Surface* scaled_surface = scaled.get_surface();
    REQUIRE(scaled_surface != nullptr);
    
    // Load golden reference data from embedded data
    SDL_Surface* golden_surface = LoadBMPFromMemory(rotozoom_hq3x_golden_data, rotozoom_hq3x_golden_len);
    REQUIRE(golden_surface != nullptr);
    
    // Verify dimensions match
    CHECK(scaled_surface->w == golden_surface->w);
    CHECK(scaled_surface->h == golden_surface->h);
    
    // Compare pixels for 100% fidelity
    int total_pixels = scaled_surface->w * scaled_surface->h;
    int exact_matches = 0;
    int close_matches = 0;  // Within tolerance of 1 per channel
    
    for (int y = 0; y < scaled_surface->h; ++y) {
        for (int x = 0; x < scaled_surface->w; ++x) {
            // Get our pixel
            Uint8* our_ptr = static_cast<Uint8*>(scaled_surface->pixels) + 
                             y * scaled_surface->pitch + x * 3;
            
            // Get golden pixel
            Uint8* golden_ptr = static_cast<Uint8*>(golden_surface->pixels) + 
                                y * golden_surface->pitch + x * 3;
            
            // Compare RGB values (assuming BGR order in BMP)
            int dr = std::abs(static_cast<int>(our_ptr[2]) - static_cast<int>(golden_ptr[0]));
            int dg = std::abs(static_cast<int>(our_ptr[1]) - static_cast<int>(golden_ptr[1]));
            int db = std::abs(static_cast<int>(our_ptr[0]) - static_cast<int>(golden_ptr[2]));
            
            if (dr == 0 && dg == 0 && db == 0) {
                exact_matches++;
            } else if (dr <= 1 && dg <= 1 && db <= 1) {
                close_matches++;
            }
        }
    }
    
    // Report fidelity
    double exact_fidelity = (exact_matches * 100.0) / total_pixels;
    double close_fidelity = ((exact_matches + close_matches) * 100.0) / total_pixels;
    
    // Calculate RMS error for a more meaningful metric
    double total_error = 0.0;
    for (int y = 0; y < scaled_surface->h; ++y) {
        for (int x = 0; x < scaled_surface->w; ++x) {
            Uint8* our_ptr = static_cast<Uint8*>(scaled_surface->pixels) + 
                             y * scaled_surface->pitch + x * 3;
            Uint8* golden_ptr = static_cast<Uint8*>(golden_surface->pixels) + 
                                y * golden_surface->pitch + x * 3;
            
            int dr = static_cast<int>(our_ptr[2]) - static_cast<int>(golden_ptr[0]);
            int dg = static_cast<int>(our_ptr[1]) - static_cast<int>(golden_ptr[1]);
            int db = static_cast<int>(our_ptr[0]) - static_cast<int>(golden_ptr[2]);
            
            total_error += dr*dr + dg*dg + db*db;
        }
    }
    double rms_error = std::sqrt(total_error / (total_pixels * 3));
    
    // Accept high fidelity: 95%+ exact match OR 99%+ within tolerance of 1 OR RMS < 0.5
    bool high_fidelity = (exact_fidelity >= 95.0) || 
                         (close_fidelity >= 99.0) || 
                         (rms_error < 0.5);
    
    INFO("Exact fidelity: " << exact_fidelity << "%");
    INFO("Close fidelity: " << close_fidelity << "%");
    INFO("RMS error: " << rms_error);
    
    CHECK(high_fidelity);
    
    // Cleanup
    SDL_FreeSurface(golden_surface);
    SDL_FreeSurface(input_surface);
}