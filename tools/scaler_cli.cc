#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>

// STB Image implementation
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Scaler includes
#include <scaler/compiler_compat.hh>
#include <scaler/image_base.hh>
#include <scaler/vec3.hh>
#include <scaler/epx.hh>
#include <scaler/eagle.hh>
#include <scaler/2xsai.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>
#include <scaler/omniscale.hh>
#include <scaler/scale2x_sfx.hh>
#include <scaler/scale3x.hh>
#include <scaler/scale3x_sfx.hh>

namespace fs = std::filesystem;
using namespace scaler;
// STB Image adapter for the scaler library
class STBInputImage : public InputImageBase<STBInputImage, uvec3> {
private:
    unsigned char* data;
    int w, h, channels;
    
public:
    STBInputImage(const std::string& filename) {
        data = stbi_load(filename.c_str(), &w, &h, &channels, 3); // Force RGB
        if (!data) {
            throw std::runtime_error("Failed to load image: " + filename);
        }
    }
    
    ~STBInputImage() {
        if (data) {
            stbi_image_free(data);
        }
    }
    
    int width_impl() const { return w; }
    int height_impl() const { return h; }
    
    uvec3 get_pixel_impl(int x, int y) const {
        int idx = (y * w + x) * 3;
        return uvec3(data[idx], data[idx + 1], data[idx + 2]);
    }
};

class STBOutputImage : public OutputImageBase<STBOutputImage, uvec3> {
private:
    std::vector<unsigned char> data;
    int w, h;
    
public:
    // Constructor for standalone creation
    STBOutputImage(int width, int height) : w(width), h(height) {
        data.resize(width * height * 3);
    }
    
    // Constructor with source image reference (required by scalers)
    template<typename InputImage>
    STBOutputImage(int width, int height, const InputImage&) : w(width), h(height) {
        data.resize(width * height * 3);
    }
    
    int width_impl() const { return w; }
    int height_impl() const { return h; }
    
    void set_pixel_impl(int x, int y, const uvec3& color) {
        int idx = (y * w + x) * 3;
        data[idx] = color.x;
        data[idx + 1] = color.y;
        data[idx + 2] = color.z;
    }
    
    bool save(const std::string& filename) const {
        std::string ext = fs::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".png") {
            return stbi_write_png(filename.c_str(), w, h, 3, data.data(), w * 3) != 0;
        } else if (ext == ".jpg" || ext == ".jpeg") {
            return stbi_write_jpg(filename.c_str(), w, h, 3, data.data(), 95) != 0;
        } else if (ext == ".bmp") {
            return stbi_write_bmp(filename.c_str(), w, h, 3, data.data()) != 0;
        } else if (ext == ".tga") {
            return stbi_write_tga(filename.c_str(), w, h, 3, data.data()) != 0;
        }
        return false;
    }
};

// Algorithm registry
enum class Algorithm {
    EPX,
    Eagle,
    TwoXSaI,
    XBR,
    HQ2x,
    OmniScale2x,
    OmniScale3x,
    Scale2xSFX,
    Scale3x,
    Scale3xSFX
};

struct AlgorithmInfo {
    Algorithm algo;
    int scale_factor;
    std::string description;
};

static const std::map<std::string, AlgorithmInfo> algorithms = {
    {"epx", {Algorithm::EPX, 2, "EPX/Scale2x - Fast and simple"}},
    {"eagle", {Algorithm::Eagle, 2, "Eagle - Good for low-res pixel art"}},
    {"2xsai", {Algorithm::TwoXSaI, 2, "2xSaI - Smooth diagonal lines"}},
    {"xbr", {Algorithm::XBR, 2, "XBR - Advanced edge detection"}},
    {"hq2x", {Algorithm::HQ2x, 2, "HQ2x - High quality, slower"}},
    {"omniscale2x", {Algorithm::OmniScale2x, 2, "OmniScale 2x - Modern pattern-based"}},
    {"omniscale3x", {Algorithm::OmniScale3x, 3, "OmniScale 3x - Modern pattern-based"}},
    {"scale2xsfx", {Algorithm::Scale2xSFX, 2, "Scale2x SFX - Enhanced Scale2x"}},
    {"scale3x", {Algorithm::Scale3x, 3, "Scale3x - 3x version of EPX"}},
    {"scale3xsfx", {Algorithm::Scale3xSFX, 3, "Scale3x SFX - Enhanced Scale3x"}}
};

STBOutputImage applyAlgorithm(const STBInputImage& input, Algorithm algo) {
    switch (algo) {
        case Algorithm::EPX:
            return scaleEpx<STBInputImage, STBOutputImage>(input);
        case Algorithm::Eagle:
            return scaleEagle<STBInputImage, STBOutputImage>(input);
        case Algorithm::TwoXSaI:
            return scale2xSaI<STBInputImage, STBOutputImage>(input);
        case Algorithm::XBR:
            return scaleXbr<STBInputImage, STBOutputImage>(input);
        case Algorithm::HQ2x:
            return scaleHq2x<STBInputImage, STBOutputImage>(input);
        case Algorithm::OmniScale2x:
            return scaleOmniScale2x<STBInputImage, STBOutputImage>(input);
        case Algorithm::OmniScale3x:
            return scaleOmniScale3x<STBInputImage, STBOutputImage>(input);
        case Algorithm::Scale2xSFX:
            return scaleScale2xSFX<STBInputImage, STBOutputImage>(input);
        case Algorithm::Scale3x:
            return scaleScale3x<STBInputImage, STBOutputImage>(input);
        case Algorithm::Scale3xSFX:
            return scaleScale3xSFX<STBInputImage, STBOutputImage>(input);
        default:
            throw std::runtime_error("Unknown algorithm");
    }
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options] <input_image>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -a, --algorithm <name>  Scaling algorithm to use (default: epx)\n";
    std::cout << "  -o, --output <file>     Output file (default: <input>_<algorithm>.<ext>)\n";
    std::cout << "  -l, --list              List available algorithms\n";
    std::cout << "  -h, --help              Show this help message\n\n";
    std::cout << "Available algorithms:\n";
    for (const auto& [name, info] : algorithms) {
        std::cout << "  " << name << " (" << info.scale_factor << "x) - " 
                  << info.description << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string input_file;
    std::string output_file;
    std::string algorithm_name = "epx";
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--list") {
            std::cout << "Available algorithms:\n";
            for (const auto& [name, info] : algorithms) {
                std::cout << "  " << name << " (" << info.scale_factor << "x) - " 
                          << info.description << "\n";
            }
            return 0;
        } else if ((arg == "-a" || arg == "--algorithm") && i + 1 < argc) {
            algorithm_name = argv[++i];
            std::transform(algorithm_name.begin(), algorithm_name.end(), 
                          algorithm_name.begin(), ::tolower);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Check if algorithm exists
    auto algo_it = algorithms.find(algorithm_name);
    if (algo_it == algorithms.end()) {
        std::cerr << "Error: Unknown algorithm '" << algorithm_name << "'\n";
        std::cerr << "Use -l to list available algorithms\n";
        return 1;
    }
    
    // Generate output filename if not specified
    if (output_file.empty()) {
        fs::path input_path(input_file);
        std::string stem = input_path.stem().string();
        std::string ext = input_path.extension().string();
        output_file = stem + "_" + algorithm_name + ext;
    }
    
    try {
        // Load input image
        std::cout << "Loading: " << input_file << "\n";
        STBInputImage input(input_file);
        std::cout << "Image size: " << input.width() << "x" << input.height() << "\n";
        
        // Apply algorithm
        std::cout << "Applying " << algorithm_name << " (" 
                  << algo_it->second.scale_factor << "x scaling)...\n";
        auto output = applyAlgorithm(input, algo_it->second.algo);
        
        // Save output
        std::cout << "Saving: " << output_file << " (" 
                  << output.width() << "x" << output.height() << ")\n";
        if (!output.save(output_file)) {
            std::cerr << "Error: Failed to save output image\n";
            return 1;
        }
        
        std::cout << "Success!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}