#include <doctest/doctest.h>
#include <scaler/sdl_image.hh>
#include <scaler/sdl_scalers.hh>
#include <scaler/epx.hh>
#include <scaler/eagle.hh>
#include <scaler/2xsai.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>
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

TEST_CASE("SDL Interface Tests") {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        FAIL("SDL initialization failed: " << SDL_GetError());
    }
    
    SUBCASE("Direct CRTP usage vs SDL convenience functions") {
        // Load the test BMP image from embedded data
        auto input_surface = loadTestImage();
        
        REQUIRE(input_surface != nullptr);
        
        // Convert to 32-bit RGBA format for consistency
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(input_surface.get(), SDL_PIXELFORMAT_RGBA8888, 0);
        REQUIRE(converted != nullptr);
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> input_32bit(converted, SDL_FreeSurface);
        
        // Test EPX: Direct CRTP vs convenience function
        {
            // Direct CRTP usage
            SDLInputImage input_img(input_32bit.get());
            auto output_crtp = scaleEpx<SDLInputImage, SDLOutputImage>(input_img);
            
            // Convenience function
            SDL_Surface* output_conv = scaleEpxSDL(input_32bit.get());
            REQUIRE(output_conv != nullptr);
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> conv_result(output_conv, SDL_FreeSurface);
            
            // Compare results
            CHECK(compareSurfaces(output_crtp.get_surface(), output_conv));
        }
        
        // Test Eagle: Direct CRTP vs convenience function
        {
            SDLInputImage input_img(input_32bit.get());
            auto output_crtp = scaleEagle<SDLInputImage, SDLOutputImage>(input_img);
            
            SDL_Surface* output_conv = scaleEagleSDL(input_32bit.get());
            REQUIRE(output_conv != nullptr);
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> conv_result(output_conv, SDL_FreeSurface);
            
            CHECK(compareSurfaces(output_crtp.get_surface(), output_conv));
        }
        
        // Test 2xSaI: Direct CRTP vs convenience function
        {
            SDLInputImage input_img(input_32bit.get());
            auto output_crtp = scale2xSaI<SDLInputImage, SDLOutputImage>(input_img);
            
            SDL_Surface* output_conv = scale2xSaISDL(input_32bit.get());
            REQUIRE(output_conv != nullptr);
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> conv_result(output_conv, SDL_FreeSurface);
            
            CHECK(compareSurfaces(output_crtp.get_surface(), output_conv));
        }
        
        // Test XBR: Direct CRTP vs convenience function
        {
            SDLInputImage input_img(input_32bit.get());
            auto output_crtp = scaleXbr<SDLInputImage, SDLOutputImage>(input_img);
            
            SDL_Surface* output_conv = scaleXbrSDL(input_32bit.get());
            REQUIRE(output_conv != nullptr);
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> conv_result(output_conv, SDL_FreeSurface);
            
            CHECK(compareSurfaces(output_crtp.get_surface(), output_conv));
        }
        
        // Test HQ2x: Direct CRTP vs convenience function
        {
            SDLInputImage input_img(input_32bit.get());
            auto output_crtp = scaleHq2x<SDLInputImage, SDLOutputImage>(input_img);
            
            SDL_Surface* output_conv = scaleHq2xSDL(input_32bit.get());
            REQUIRE(output_conv != nullptr);
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> conv_result(output_conv, SDL_FreeSurface);
            
            CHECK(compareSurfaces(output_crtp.get_surface(), output_conv));
        }
    }
    
    SUBCASE("Small synthetic image scaling") {
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
        
        // Test all algorithms produce 4x4 output
        {
            SDL_Surface* epx_output = scaleEpxSDL(small_input);
            CHECK(epx_output != nullptr);
            CHECK(epx_output->w == 4);
            CHECK(epx_output->h == 4);
            SDL_FreeSurface(epx_output);
        }
        
        {
            SDL_Surface* eagle_output = scaleEagleSDL(small_input);
            CHECK(eagle_output != nullptr);
            CHECK(eagle_output->w == 4);
            CHECK(eagle_output->h == 4);
            SDL_FreeSurface(eagle_output);
        }
        
        {
            SDL_Surface* sai_output = scale2xSaISDL(small_input);
            CHECK(sai_output != nullptr);
            CHECK(sai_output->w == 4);
            CHECK(sai_output->h == 4);
            SDL_FreeSurface(sai_output);
        }
        
        {
            SDL_Surface* xbr_output = scaleXbrSDL(small_input);
            CHECK(xbr_output != nullptr);
            CHECK(xbr_output->w == 4);
            CHECK(xbr_output->h == 4);
            SDL_FreeSurface(xbr_output);
        }
        
        {
            SDL_Surface* hq2x_output = scaleHq2xSDL(small_input);
            CHECK(hq2x_output != nullptr);
            CHECK(hq2x_output->w == 4);
            CHECK(hq2x_output->h == 4);
            SDL_FreeSurface(hq2x_output);
        }
    }
    
    SDL_Quit();
}