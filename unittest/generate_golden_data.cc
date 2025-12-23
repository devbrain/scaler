// Program to generate golden data for all scaling algorithms
// Bypass SDL_main on Windows - this is a utility program
#define SDL_MAIN_HANDLED
#include <scaler/sdl/sdl_compat.hh>
#include <scaler/sdl/sdl_image.hh>
#include <../include/scaler/cpu/epx.hh>
#include <scaler/cpu/eagle.hh>
#include <../include/scaler/cpu/2xsai.hh>
#include <../include/scaler/cpu/xbr.hh>
#include <../include/scaler/cpu/hq2x.hh>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>

#include "data/rotozoom_bmp.h"

using namespace scaler;

struct PixelData {
    uint8_t r, g, b, a;
};

// Helper to extract pixel data from a surface
std::vector<PixelData> extractPixelData(SDL_Surface* surface) {
    std::vector<PixelData> data;
    data.reserve(static_cast<size_t>(surface->w) * static_cast<size_t>(surface->h));

    // Lock surface if needed
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            auto* pixels = static_cast<Uint32*>(surface->pixels);
            Uint32 pixel = pixels[y * surface->w + x];

            PixelData pd;
            SDL_GetRGBA(pixel, surface->format, &pd.r, &pd.g, &pd.b, &pd.a);
            data.push_back(pd);
        }
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    return data;
}

// Helper to write golden data as C header
void writeGoldenData(const std::string& algorithm_name,
                     size_t width, size_t height,
                     const std::vector<PixelData>& data) {
    std::string filename = "golden_" + algorithm_name + ".h";
    std::ofstream file(filename);

    // Convert to uppercase for define
    std::string upper_name = algorithm_name;
    for (auto& c : upper_name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    file << "#pragma once\n";
    file << "// Golden data for " << algorithm_name << " algorithm\n";
    file << "// Generated from rotozoom.bmp\n\n";

    file << "const int GOLDEN_" << upper_name << "_WIDTH = " << width << ";\n";
    file << "const int GOLDEN_" << upper_name << "_HEIGHT = " << height << ";\n\n";

    // Write as RGBA array
    file << "const unsigned char GOLDEN_" << upper_name << "_DATA[] = {\n";

    for (size_t i = 0; i < data.size(); ++i) {
        if (i % 4 == 0) file << "    ";

        file << "0x" << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>(data[i].r) << ", ";
        file << "0x" << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>(data[i].g) << ", ";
        file << "0x" << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>(data[i].b) << ", ";
        file << "0x" << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>(data[i].a);

        if (i < data.size() - 1) file << ", ";
        if ((i + 1) % 4 == 0 || i == data.size() - 1) file << "\n";
    }

    file << "};\n\n";
    file << "const size_t GOLDEN_" << upper_name << "_SIZE = sizeof(GOLDEN_"
         << upper_name << "_DATA);\n";

    std::cout << "Generated " << filename << " (" << width << "x" << height
              << ", " << data.size() << " pixels)\n";
}

// Also generate a small test pattern for comprehensive testing
void generateTestPattern() {
    // Create a 4x4 test pattern
    SDL_Surface* pattern = SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_RGBA8888);
    
    if (SDL_MUSTLOCK(pattern)) SDL_LockSurface(pattern);

    auto* pixels = static_cast<Uint32*>(pattern->pixels);
    
    // Create a pattern with various colors and patterns
    // Row 0: Red gradient
    pixels[0] = SDL_MapRGBA(pattern->format, 255, 0, 0, 255);
    pixels[1] = SDL_MapRGBA(pattern->format, 192, 0, 0, 255);
    pixels[2] = SDL_MapRGBA(pattern->format, 128, 0, 0, 255);
    pixels[3] = SDL_MapRGBA(pattern->format, 64, 0, 0, 255);
    
    // Row 1: Green gradient
    pixels[4] = SDL_MapRGBA(pattern->format, 0, 255, 0, 255);
    pixels[5] = SDL_MapRGBA(pattern->format, 0, 192, 0, 255);
    pixels[6] = SDL_MapRGBA(pattern->format, 0, 128, 0, 255);
    pixels[7] = SDL_MapRGBA(pattern->format, 0, 64, 0, 255);
    
    // Row 2: Blue gradient
    pixels[8] = SDL_MapRGBA(pattern->format, 0, 0, 255, 255);
    pixels[9] = SDL_MapRGBA(pattern->format, 0, 0, 192, 255);
    pixels[10] = SDL_MapRGBA(pattern->format, 0, 0, 128, 255);
    pixels[11] = SDL_MapRGBA(pattern->format, 0, 0, 64, 255);
    
    // Row 3: Mixed colors
    pixels[12] = SDL_MapRGBA(pattern->format, 255, 255, 0, 255);  // Yellow
    pixels[13] = SDL_MapRGBA(pattern->format, 255, 0, 255, 255);  // Magenta
    pixels[14] = SDL_MapRGBA(pattern->format, 0, 255, 255, 255);  // Cyan
    pixels[15] = SDL_MapRGBA(pattern->format, 255, 255, 255, 255); // White
    
    if (SDL_MUSTLOCK(pattern)) SDL_UnlockSurface(pattern);
    
    // Write the test pattern source
    auto patternData = extractPixelData(pattern);
    writeGoldenData("test_pattern_source", 4, 4, patternData);
    
    // Generate golden data for each algorithm with test pattern
    sdl_input_image input(pattern);
    
    // EPX
    {
        auto output = scale_epx<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("test_pattern_epx", output.width(), output.height(), data);
    }
    
    // Eagle
    {
        auto output = scale_eagle<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("test_pattern_eagle", output.width(), output.height(), data);
    }
    
    // 2xSaI
    {
        auto output = scale_2x_sai<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("test_pattern_2xsai", output.width(), output.height(), data);
    }
    
    // XBR
    {
        auto output = scale_xbr<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("test_pattern_xbr", output.width(), output.height(), data);
    }
    
    // HQ2x
    {
        auto output = scale_hq2x<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("test_pattern_hq2x", output.width(), output.height(), data);
    }
    
    SDL_FreeSurface(pattern);
}

int main() {
    // Tell SDL we're handling main ourselves
    SDL_SetMainReady();

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Load the embedded BMP
    SDL_IOStream* io = SDL_IOFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    if (!io) {
        std::cerr << "Failed to create IO stream" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    SDL_Surface* surface = SDL_LoadBMP_IO(io, true);
    if (!surface) {
        std::cerr << "Failed to load BMP: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Convert to RGBA for consistency
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    
    if (!rgba_surface) {
        std::cerr << "Failed to convert surface format" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::cout << "Loaded image: " << rgba_surface->w << "x" << rgba_surface->h << std::endl;
    
    // Create input image
    sdl_input_image input(rgba_surface);
    
    // Generate golden data for each algorithm
    std::cout << "\nGenerating golden data for full image...\n";
    
    // EPX
    {
        auto output = scale_epx<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("epx", output.width(), output.height(), data);
    }
    
    // Eagle
    {
        auto output = scale_eagle<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("eagle", output.width(), output.height(), data);
    }
    
    // 2xSaI
    {
        auto output = scale_2x_sai<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("2xsai", output.width(), output.height(), data);
    }
    
    // XBR
    {
        auto output = scale_xbr<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("xbr", output.width(), output.height(), data);
    }
    
    // HQ2x
    {
        auto output = scale_hq2x<sdl_input_image, sdl_output_image>(input);
        auto data = extractPixelData(output.get_surface());
        writeGoldenData("hq2x", output.width(), output.height(), data);
    }
    
    // Also generate test pattern golden data
    std::cout << "\nGenerating golden data for test patterns...\n";
    generateTestPattern();
    
    SDL_FreeSurface(rgba_surface);
    SDL_Quit();
    
    std::cout << "\nGolden data generation complete!\n";
    return 0;
}