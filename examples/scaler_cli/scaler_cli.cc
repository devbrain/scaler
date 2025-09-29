#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <chrono>

#include "stb_image_wrapper.hh"
#include <scaler/unified_scaler.hh>
#include <scaler/algorithm_capabilities.hh>

using namespace scaler;

/**
 * Command-line image scaler using the unified scaler interface
 *
 * Usage: scaler_cli <input> <output> [options]
 *
 * Options:
 *   -a, --algorithm <name>  Scaling algorithm (default: bilinear)
 *   -s, --scale <factor>    Scale factor (default: 2.0)
 *   -l, --list              List available algorithms
 *   -i, --info              Show information about algorithms
 *   -q, --quality <1-100>   JPEG output quality (default: 95)
 *   -h, --help              Show this help message
 */

struct Options {
    std::string input_file;
    std::string output_file;
    algorithm algo = algorithm::Bilinear;
    float scale_factor = 2.0f;
    int jpeg_quality = 95;
    bool list_algorithms = false;
    bool show_info = false;
};

// Convert string to lowercase
std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

// Parse algorithm name from string
algorithm parse_algorithm(const std::string& name) {
    std::string lower_name = to_lower(name);

    if (lower_name == "nearest" || lower_name == "nn") {
        return algorithm::Nearest;
    } else if (lower_name == "bilinear") {
        return algorithm::Bilinear;
    } else if (lower_name == "trilinear") {
        return algorithm::Trilinear;
    } else if (lower_name == "epx") {
        return algorithm::EPX;
    } else if (lower_name == "eagle") {
        return algorithm::Eagle;
    } else if (lower_name == "scale" || lower_name == "scale2x" || lower_name == "scale3x") {
        return algorithm::Scale;
    } else if (lower_name == "scalefx" || lower_name == "sfx") {
        return algorithm::ScaleSFX;
    } else if (lower_name == "2xsai" || lower_name == "super2xsai") {
        return algorithm::Super2xSaI;
    } else if (lower_name == "hq" || lower_name == "hq2x" || lower_name == "hq3x" || lower_name == "hq4x") {
        return algorithm::HQ;
    } else if (lower_name == "aascale" || lower_name == "aa") {
        return algorithm::AAScale;
    } else if (lower_name == "xbr") {
        return algorithm::xBR;
    } else if (lower_name == "omniscale") {
        return algorithm::OmniScale;
    }

    throw std::runtime_error("Unknown algorithm: " + name);
}

// Print help message
void print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " <input> <output> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -a, --algorithm <name>  Scaling algorithm (default: bilinear)\n";
    std::cout << "  -s, --scale <factor>    Scale factor (default: 2.0)\n";
    std::cout << "  -l, --list              List available algorithms\n";
    std::cout << "  -i, --info              Show information about algorithms\n";
    std::cout << "  -q, --quality <1-100>   JPEG output quality (default: 95)\n";
    std::cout << "  -h, --help              Show this help message\n\n";
    std::cout << "Supported algorithms:\n";
    std::cout << "  nearest    - Nearest neighbor (fast, pixelated)\n";
    std::cout << "  bilinear   - Bilinear interpolation (smooth)\n";
    std::cout << "  trilinear  - Trilinear interpolation (smoother)\n";
    std::cout << "  epx        - EPX/Scale2x (pixel art, 2x only)\n";
    std::cout << "  eagle      - Eagle (pixel art, 2x only)\n";
    std::cout << "  scale      - Scale2x/3x (pixel art)\n";
    std::cout << "  scalefx    - ScaleFX (pixel art)\n";
    std::cout << "  2xsai      - Super 2xSaI (smooth 2D, 2x only)\n";
    std::cout << "  hq         - HQ2x/3x/4x (high quality)\n";
    std::cout << "  aascale    - AAScale (anti-aliased)\n";
    std::cout << "  xbr        - xBR (advanced edge detection)\n";
    std::cout << "  omniscale  - OmniScale (AI-based, 1-6x)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " input.png output.png -a epx\n";
    std::cout << "  " << program_name << " input.jpg output.jpg -s 3.5 -a bilinear\n";
    std::cout << "  " << program_name << " input.png output.jpg -a hq -s 4 -q 90\n";
}

// List all available algorithms and their supported scales
void list_algorithms() {
    std::cout << "Available Scaling Algorithms:\n";
    std::cout << "==============================\n\n";

    auto all_algos = algorithm_capabilities::get_all_algorithms();

    for (auto algo : all_algos) {
        const auto& info = algorithm_capabilities::get_info(algo);
        std::cout << std::setw(12) << std::left << info.name << " - ";

        if (info.cpu_arbitrary_scale) {
            std::cout << "Arbitrary scaling (1x - unlimited)";
        } else {
            std::cout << "Fixed scales: ";
            for (size_t i = 0; i < info.cpu_supported_scales.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << info.cpu_supported_scales[i] << "x";
            }
        }

        std::cout << "\n";
        std::cout << std::setw(14) << " " << info.description << "\n\n";
    }
}

// Show detailed information about algorithms
void show_algorithm_info() {
    std::cout << "Algorithm Information:\n";
    std::cout << "=====================\n\n";

    std::cout << "Categories:\n\n";

    std::cout << "1. Interpolation (smooth, arbitrary scaling):\n";
    std::cout << "   - Nearest: Fast but pixelated\n";
    std::cout << "   - Bilinear: Smooth interpolation\n";
    std::cout << "   - Trilinear: Higher quality interpolation\n\n";

    std::cout << "2. Pixel Art (preserves sharp edges, fixed scales):\n";
    std::cout << "   - EPX/Eagle: Simple 2x algorithms\n";
    std::cout << "   - Scale2x/3x: AdvMAME scalers\n";
    std::cout << "   - HQ2x/3x/4x: High quality pixel art scaling\n\n";

    std::cout << "3. Advanced (pattern recognition, multiple scales):\n";
    std::cout << "   - xBR: Advanced edge detection (2-6x)\n";
    std::cout << "   - OmniScale: Modern AI-based (1-6x)\n";
    std::cout << "   - AAScale: Anti-aliased scaling\n\n";

    std::cout << "Recommendations:\n";
    std::cout << "- Photographs: bilinear or trilinear\n";
    std::cout << "- Pixel art: epx, hq, or xbr\n";
    std::cout << "- General purpose: bilinear or omniscale\n";
    std::cout << "- Fast preview: nearest\n";
}

// Parse command-line arguments
Options parse_arguments(int argc, char* argv[]) {
    Options opts;

    // Check for help first
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            std::exit(0);
        }
    }

    // Quick scan for list/info options which don't need files
    bool has_list_or_info = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-l" || arg == "--list" || arg == "-i" || arg == "--info") {
            has_list_or_info = true;
            break;
        }
    }

    // Only check for minimum arguments if not listing/showing info
    if (!has_list_or_info && argc < 3) {
        throw std::runtime_error("Not enough arguments");
    }

    // First two positional arguments are input and output
    int pos_count = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-l" || arg == "--list") {
            opts.list_algorithms = true;
        } else if (arg == "-i" || arg == "--info") {
            opts.show_info = true;
        } else if (arg == "-a" || arg == "--algorithm") {
            if (++i >= argc) {
                throw std::runtime_error("Missing algorithm name");
            }
            opts.algo = parse_algorithm(argv[i]);
        } else if (arg == "-s" || arg == "--scale") {
            if (++i >= argc) {
                throw std::runtime_error("Missing scale factor");
            }
            opts.scale_factor = std::stof(argv[i]);
            if (opts.scale_factor <= 0) {
                throw std::runtime_error("Scale factor must be positive");
            }
        } else if (arg == "-q" || arg == "--quality") {
            if (++i >= argc) {
                throw std::runtime_error("Missing quality value");
            }
            opts.jpeg_quality = std::stoi(argv[i]);
            if (opts.jpeg_quality < 1 || opts.jpeg_quality > 100) {
                throw std::runtime_error("Quality must be between 1 and 100");
            }
        } else if (arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        } else {
            // Positional argument
            if (pos_count == 0) {
                opts.input_file = arg;
            } else if (pos_count == 1) {
                opts.output_file = arg;
            } else {
                throw std::runtime_error("Too many positional arguments");
            }
            pos_count++;
        }
    }

    // Check if we just want to list algorithms
    if (opts.list_algorithms || opts.show_info) {
        return opts;
    }

    // Otherwise, we need input and output files
    if (opts.input_file.empty() || opts.output_file.empty()) {
        throw std::runtime_error("Input and output files are required");
    }

    return opts;
}

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments
        Options opts = parse_arguments(argc, argv);

        // Handle listing/info modes
        if (opts.list_algorithms) {
            list_algorithms();
            return 0;
        }

        if (opts.show_info) {
            show_algorithm_info();
            return 0;
        }

        // Load input image
        std::cout << "Loading image: " << opts.input_file << "\n";
        stb_image input(opts.input_file.c_str());

        std::cout << "Input size: " << input.width() << "x" << input.height()
                  << " (" << input.channels() << " channels)\n";

        // Check if the algorithm supports the requested scale
        if (!scaler_capabilities::is_scale_supported(opts.algo, opts.scale_factor)) {
            std::cerr << "Error: Algorithm '"
                      << scaler_capabilities::get_algorithm_name(opts.algo)
                      << "' does not support scale factor " << opts.scale_factor << "\n";
            std::cerr << "Supported scales: ";

            auto scales = scaler_capabilities::get_supported_scales(opts.algo);
            for (size_t i = 0; i < scales.size(); ++i) {
                if (i > 0) std::cerr << ", ";
                std::cerr << scales[i] << "x";
            }
            std::cerr << "\n";
            return 1;
        }

        // Perform scaling
        std::cout << "Scaling with " << scaler_capabilities::get_algorithm_name(opts.algo)
                  << " at " << opts.scale_factor << "x...\n";

        auto start = std::chrono::high_resolution_clock::now();

        auto output = unified_scaler<stb_image, stb_image>::scale(
            input, opts.algo, opts.scale_factor
        );

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Scaling completed in " << duration.count() << " ms\n";
        std::cout << "Output size: " << output.width() << "x" << output.height() << "\n";

        // Save output image
        std::cout << "Saving image: " << opts.output_file << "\n";
        if (!output.save(opts.output_file.c_str(), opts.jpeg_quality)) {
            std::cerr << "Error: Failed to save output image\n";
            return 1;
        }

        std::cout << "Success!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Try '" << argv[0] << " --help' for usage information\n";
        return 1;
    }
}