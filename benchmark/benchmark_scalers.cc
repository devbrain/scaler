#include <scaler/sdl/sdl_image.hh>
#include <scaler/unified_scaler.hh>
#include <scaler/algorithm_capabilities.hh>
#include <scaler/sdl/sdl_compat.hh>

#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <random>
#include <cstring>
#include <sstream>
#include <cmath>
#include <memory>
#include <ctime>
#include <functional>
#include <unistd.h>  // For isatty

// Include embedded test image
#include "../unittest/data/rotozoom_bmp.h"

using namespace scaler;

// Simple JSON writer for baseline
class BaselineWriter {
public:
    std::ostringstream json;
    bool first_element = true;
    void begin() {
        json << "{\n";
        first_element = true;
    }

    void end() {
        json << "\n}";
    }

    void add_string(const std::string& key, const std::string& value) {
        if (!first_element) json << ",\n";
        json << "  \"" << key << "\": \"" << value << "\"";
        first_element = false;
    }

    void add_number(const std::string& key, double value) {
        if (!first_element) json << ",\n";
        json << "  \"" << key << "\": " << std::fixed << std::setprecision(3) << value;
        first_element = false;
    }

    void begin_object(const std::string& key) {
        if (!first_element) json << ",\n";
        json << "  \"" << key << "\": {\n";
        first_element = true;
    }

    void end_object() {
        json << "\n  }";
        first_element = false;
    }

    void begin_nested_object(const std::string& key) {
        if (!first_element) json << ",\n";
        json << "    \"" << key << "\": {\n";
        first_element = true;
    }

    void end_nested_object() {
        json << "\n    }";
    }

    void add_nested_number(const std::string& key, double value) {
        if (!first_element) json << ",\n";
        json << "      \"" << key << "\": " << std::fixed << std::setprecision(3) << value;
        first_element = false;
    }

    std::string str() const { return json.str(); }
};

// Baseline comparison
struct BaselineComparison {
    std::string test_name;
    double baseline_time;
    double current_time;
    double change_percent;
    bool regression;
};

// Timing utilities
class Timer {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<double, std::milli>;

    TimePoint start_time;

public:
    void start() {
        start_time = Clock::now();
    }

    double elapsed_ms() const {
        auto end_time = Clock::now();
        Duration diff = end_time - start_time;
        return diff.count();
    }
};

// Statistics tracker
struct BenchmarkStats {
    double min_time = std::numeric_limits<double>::max();
    double max_time = 0.0;
    double total_time = 0.0;
    double total_time_sq = 0.0;  // For standard deviation
    int runs = 0;

    void add_sample(double time_ms) {
        min_time = std::min(min_time, time_ms);
        max_time = std::max(max_time, time_ms);
        total_time += time_ms;
        total_time_sq += time_ms * time_ms;
        runs++;
    }

    double mean() const {
        return runs > 0 ? total_time / runs : 0.0;
    }

    double stddev() const {
        if (runs <= 1) return 0.0;
        double mean_val = mean();
        return std::sqrt((total_time_sq / runs) - (mean_val * mean_val));
    }

    double throughput_mpps(int width, int height) const {
        // Megapixels per second
        double pixels = width * height;
        double mean_ms = mean();
        if (mean_ms <= 0) return 0.0;
        return (pixels / 1000000.0) / (mean_ms / 1000.0);
    }
};

// Load embedded rotozoom image
SDL_Surface* load_rotozoom_image() {
#if SCALER_SDL_VERSION == 3
    SDL_IOStream* io = SDL_IOFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    if (!io) {
        std::cerr << "Failed to create IO stream for rotozoom image" << std::endl;
        return nullptr;
    }

    SDL_Surface* surface = SDL_LoadBMP_IO(io, true); // true = close IO after load
#else
    SDL_RWops* rw = SDL_RWFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    if (!rw) {
        std::cerr << "Failed to create RWops for rotozoom image" << std::endl;
        return nullptr;
    }

    SDL_Surface* surface = SDL_LoadBMP_RW(rw, 1); // 1 = free RW after load
#endif

    if (!surface) {
        std::cerr << "Failed to load rotozoom BMP: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Convert to RGBA for consistency with other test images
#if SCALER_SDL_VERSION == 3
    SDL_Surface* rgba_surface = SDL_ConvertSurface(surface, SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32));
#else
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
#endif
    SDL_DestroySurface(surface);

    return rgba_surface;
}

// Test image generator
SDL_Surface* create_test_image(int width, int height, const std::string& pattern) {
#if SCALER_SDL_VERSION == 3
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
#else
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
#endif

    if (!surface) return nullptr;

    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

    Uint32* pixels = (Uint32*)surface->pixels;
    std::mt19937 rng(42);  // Fixed seed for reproducibility

    if (pattern == "random") {
        std::uniform_int_distribution<Uint32> dist(0, 0xFFFFFFFF);
        for (int i = 0; i < width * height; ++i) {
            pixels[i] = dist(rng);
        }
    } else if (pattern == "gradient") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Uint8 r = (x * 255) / width;
                Uint8 g = (y * 255) / height;
                Uint8 b = ((x + y) * 255) / (width + height);
#if SCALER_SDL_VERSION == 3
                pixels[y * width + x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format),
                                                    nullptr, r, g, b, 255);
#else
                pixels[y * width + x] = SDL_MapRGBA(surface->format, r, g, b, 255);
#endif
            }
        }
    } else if (pattern == "checkerboard") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool is_white = ((x / 8) + (y / 8)) % 2 == 0;
                Uint8 color = is_white ? 255 : 0;
#if SCALER_SDL_VERSION == 3
                pixels[y * width + x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format),
                                                    nullptr, color, color, color, 255);
#else
                pixels[y * width + x] = SDL_MapRGBA(surface->format, color, color, color, 255);
#endif
            }
        }
    } else if (pattern == "solid") {
#if SCALER_SDL_VERSION == 3
        Uint32 color = SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format),
                                   nullptr, 128, 128, 128, 255);
#else
        Uint32 color = SDL_MapRGBA(surface->format, 128, 128, 128, 255);
#endif
        for (int i = 0; i < width * height; ++i) {
            pixels[i] = color;
        }
    }

    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    return surface;
}

// Benchmark runner for a single algorithm using unified scaler
BenchmarkStats benchmark_algorithm(
    const std::string& name,
    algorithm algo,
    SDL_Surface* input,
    int warmup_runs,
    int bench_runs,
    bool verbose = false) {

    BenchmarkStats stats;
    Timer timer;

    // Create unified scaler
    scaler::unified_scaler<SDLInputImage, SDLOutputImage> scaler;

    // Wrap input surface
    SDLInputImage input_img(input);

    // Calculate output dimensions for 2x scaling
    size_t out_width = input_img.width() * 2;
    size_t out_height = input_img.height() * 2;

    // Warmup runs
    if (verbose) std::cout << "  Warming up " << name << "..." << std::flush;
    for (int i = 0; i < warmup_runs; ++i) {
        SDLOutputImage output_img(out_width, out_height, input);
        scaler.scale(input_img, output_img, algo);
        // Free the output surface
        SDL_Surface* surf = output_img.release();
        if (surf) SDL_DestroySurface(surf);
    }
    if (verbose) std::cout << " done" << std::endl;

    // Benchmark runs
    if (verbose) std::cout << "  Benchmarking " << name << "..." << std::flush;
    for (int i = 0; i < bench_runs; ++i) {
        SDLOutputImage output_img(out_width, out_height, input);
        timer.start();
        scaler.scale(input_img, output_img, algo);
        double elapsed = timer.elapsed_ms();

        // Free the output surface
        SDL_Surface* surf = output_img.release();
        if (surf) SDL_DestroySurface(surf);

        stats.add_sample(elapsed);

        if (verbose && (i + 1) % 10 == 0) {
            std::cout << "." << std::flush;
        }
    }
    if (verbose) std::cout << " done" << std::endl;

    return stats;
}

// Get all CPU-capable algorithms from the database
std::vector<std::pair<algorithm, algorithm_capabilities::algorithm_info>> get_cpu_algorithms() {
    std::vector<std::pair<algorithm, algorithm_capabilities::algorithm_info>> cpu_algos;

    auto all_algos = algorithm_capabilities::get_all_algorithms();
    for (auto algo : all_algos) {
        const auto& info = algorithm_capabilities::get_info(algo);
        if (!info.cpu_supported_scales.empty() || info.cpu_arbitrary_scale) {
            cpu_algos.push_back({algo, info});
        }
    }

    return cpu_algos;
}

// Benchmark all algorithms with a given image
void benchmark_all_algorithms(SDL_Surface* input, const std::string& description,
                             int warmup_runs = 5, int bench_runs = 50,
                             const std::string& filter_algorithm = "",
                             bool verbose = false) {
    std::cout << "\n=== Benchmark: " << description << " ===" << std::endl;
    std::cout << "Image size: " << input->w << "x" << input->h
              << " (" << (input->w * input->h / 1000000.0) << " MP)" << std::endl;
    std::cout << "Warmup runs: " << warmup_runs << ", Benchmark runs: " << bench_runs << std::endl;

    std::map<std::string, BenchmarkStats> results;

    // Get CPU algorithms from database
    auto cpu_algos = get_cpu_algorithms();

    // Benchmark each algorithm
    for (const auto& [algo, info] : cpu_algos) {
        // Skip if filter is set and doesn't match
        if (!filter_algorithm.empty() && info.name != filter_algorithm) continue;

        // Skip algorithms that don't support the required scale
        // For benchmarking, we'll test 2x scaling primarily
        bool supports_2x = false;
        for (int scale : info.cpu_supported_scales) {
            if (scale == 2) {
                supports_2x = true;
                break;
            }
        }

        if (!supports_2x && !info.cpu_arbitrary_scale) {
            if (verbose) {
                std::cout << "  Skipping " << info.name << " (doesn't support 2x scaling)" << std::endl;
            }
            continue;
        }

        results[info.name] = benchmark_algorithm(info.name, algo, input, warmup_runs, bench_runs, verbose);
    }

    // Print results table
    std::cout << "\nResults:\n";
    std::cout << std::setw(15) << "Algorithm"
              << std::setw(12) << "Mean (ms)"
              << std::setw(12) << "StdDev (ms)"
              << std::setw(12) << "Min (ms)"
              << std::setw(12) << "Max (ms)"
              << std::setw(15) << "Throughput"
              << std::endl;
    std::cout << std::string(76, '-') << std::endl;

    for (const auto& [name, stats] : results) {
        std::cout << std::setw(15) << name
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.mean()
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.stddev()
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.min_time
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.max_time
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << stats.throughput_mpps(input->w, input->h) << " MP/s"
                  << std::endl;
    }

    // Find fastest algorithm
    if (!results.empty()) {
        auto fastest = std::min_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second.mean() < b.second.mean(); });

        std::cout << "\nFastest: " << fastest->first << " ("
                  << std::fixed << std::setprecision(3) << fastest->second.mean() << " ms)\n";

        // Print relative performance (if EPX exists)
        auto epx_it = results.find("EPX");
        if (epx_it != results.end()) {
            std::cout << "\nRelative Performance (vs EPX):\n";
            double epx_time = epx_it->second.mean();
            for (const auto& [name, stats] : results) {
                double relative = epx_time / stats.mean();
                std::cout << std::setw(15) << name << ": "
                          << std::fixed << std::setprecision(2) << relative << "x"
                          << (relative > 1.0 ? " faster" : " slower") << std::endl;
            }
        }
    }
}

// Memory usage benchmark
void benchmark_memory_usage(SDL_Surface* input) {
    std::cout << "\n=== Memory Usage Analysis ===" << std::endl;
    std::cout << "Input image: " << input->w << "x" << input->h << std::endl;

    size_t input_size = input->w * input->h * 4;  // RGBA
    size_t output_size = (input->w * 2) * (input->h * 2) * 4;  // 2x scaling

    std::cout << "Input memory: " << (input_size / 1024.0) << " KB" << std::endl;
    std::cout << "Output memory: " << (output_size / 1024.0) << " KB" << std::endl;
    std::cout << "Total working set: " << ((input_size + output_size) / 1024.0) << " KB" << std::endl;

    // Estimate cache usage
    std::cout << "\nCache Analysis:" << std::endl;
    std::cout << "L1 cache (32 KB): " << ((input_size + output_size) > 32768 ? "Will exceed" : "Fits") << std::endl;
    std::cout << "L2 cache (256 KB): " << ((input_size + output_size) > 262144 ? "Will exceed" : "Fits") << std::endl;
    std::cout << "L3 cache (8 MB): " << ((input_size + output_size) > 8388608 ? "Will exceed" : "Fits") << std::endl;
}

// Save baseline to JSON file
void save_baseline_json(const std::string& filename,
                  const std::vector<std::tuple<std::string, int, int, std::map<std::string, BenchmarkStats>>>& all_results) {
    BaselineWriter writer;
    writer.begin();

    // Add metadata
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream date_stream;
    date_stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    writer.add_string("description", "Performance baseline for scaler library");
    writer.add_string("version", "2.0");
    writer.add_string("date", date_stream.str());
#ifdef CMAKE_BUILD_TYPE
    writer.add_string("build_type", CMAKE_BUILD_TYPE);
#else
    writer.add_string("build_type", "Unknown");
#endif

    // Get CPU info (Linux specific)
    std::string cpu_info = "Unknown";
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    cpu_info = line.substr(colon + 2);
                    break;
                }
            }
        }
    }
    writer.add_string("cpu_info", cpu_info);

    // Add benchmark results
    writer.begin_object("benchmarks");

    for (const auto& [pattern, width, height, results] : all_results) {
        std::string config_key = pattern + "_" + std::to_string(width) + "x" + std::to_string(height);

        writer.begin_nested_object(config_key);

        for (const auto& [algo, stats] : results) {
            writer.add_nested_number(algo + "_mean", stats.mean());
            writer.add_nested_number(algo + "_stddev", stats.stddev());
            writer.add_nested_number(algo + "_min", stats.min_time);
        }

        writer.end_nested_object();
    }

    writer.end_object();
    writer.end();

    // Write to file
    std::ofstream file(filename);
    file << writer.str();
    file.close();

    std::cout << "\nBaseline saved to " << filename << std::endl;
}

// Load baseline from JSON (simplified parser)
std::map<std::string, double> load_baseline(const std::string& filename) {
    std::map<std::string, double> baseline_times;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open baseline file: " << filename << std::endl;
        return baseline_times;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Find the benchmarks section
    size_t benchmarks_start = content.find("\"benchmarks\"");
    if (benchmarks_start == std::string::npos) return baseline_times;

    // Find each configuration block within benchmarks
    size_t pos = benchmarks_start;

    // Pattern: "config_name": { ... algorithm data ... }
    while (pos < content.length()) {
        // Find next configuration name
        size_t quote_start = content.find('\"', pos);
        if (quote_start == std::string::npos) break;

        size_t quote_end = content.find('\"', quote_start + 1);
        if (quote_end == std::string::npos) break;

        std::string potential_config = content.substr(quote_start + 1, quote_end - quote_start - 1);

        // Skip if not a configuration (must have underscore and 'x' for dimensions)
        if (potential_config.find('_') == std::string::npos ||
            potential_config.find('x') == std::string::npos) {
            pos = quote_end + 1;
            continue;
        }

        // Found a configuration, now find its block
        size_t block_start = content.find('{', quote_end);
        if (block_start == std::string::npos) break;

        size_t block_end = content.find('}', block_start);
        if (block_end == std::string::npos) break;

        // Extract all algorithm means within this block
        size_t algo_pos = block_start;
        while (algo_pos < block_end) {
            size_t mean_pos = content.find("_mean\"", algo_pos);
            if (mean_pos == std::string::npos || mean_pos >= block_end) break;

            // Get algorithm name
            size_t algo_start = content.rfind('\"', mean_pos - 1);
            if (algo_start == std::string::npos || algo_start < block_start) break;

            std::string algo_name = content.substr(algo_start + 1, mean_pos - algo_start - 1);

            // Get value
            size_t colon_pos = content.find(':', mean_pos);
            if (colon_pos == std::string::npos || colon_pos >= block_end) break;

            size_t value_end = content.find_first_of(",}", colon_pos);
            if (value_end == std::string::npos) value_end = block_end;

            std::string value_str = content.substr(colon_pos + 1, value_end - colon_pos - 1);
            // Trim whitespace
            value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
            value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);

            try {
                double value = std::stod(value_str);
                std::string full_key = potential_config + "_" + algo_name + "_mean";
                baseline_times[full_key] = value;
            } catch (...) {
                // Ignore parse errors
            }

            algo_pos = value_end;
        }

        pos = block_end + 1;
    }

    return baseline_times;
}

// Get comparison result for a single test
BaselineComparison get_baseline_comparison(const std::string& pattern, int width, int height,
                                          const std::string& algo, const BenchmarkStats& stats,
                                          const std::map<std::string, double>& baseline) {
    BaselineComparison result;
    std::string config = pattern + "_" + std::to_string(width) + "x" + std::to_string(height);
    result.test_name = config + "_" + algo;
    result.current_time = stats.mean();

    // Look for this specific test in baseline with full configuration key
    std::string key = config + "_" + algo + "_mean";
    auto it = baseline.find(key);

    if (it != baseline.end()) {
        result.baseline_time = it->second;
        result.change_percent = ((result.current_time - result.baseline_time) / result.baseline_time) * 100.0;
        result.regression = result.change_percent > 10.0;  // More than 10% slower is a regression
    } else {
        result.baseline_time = 0;
        result.change_percent = 0;
        result.regression = false;
    }

    return result;
}

// CSV output for analysis
void output_csv(const std::string& filename,
                const std::vector<std::tuple<std::string, int, int, std::map<std::string, BenchmarkStats>>>& all_results) {
    std::ofstream csv(filename);

    // Header
    csv << "Pattern,Width,Height,Algorithm,Mean_ms,StdDev_ms,Min_ms,Max_ms,Throughput_MPps\n";

    // Data
    for (const auto& [pattern, width, height, results] : all_results) {
        for (const auto& [algo, stats] : results) {
            csv << pattern << ","
                << width << ","
                << height << ","
                << algo << ","
                << stats.mean() << ","
                << stats.stddev() << ","
                << stats.min_time << ","
                << stats.max_time << ","
                << stats.throughput_mpps(width, height) << "\n";
        }
    }

    csv.close();
    std::cout << "\nResults saved to " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    bool verbose = false;
    bool quick = false;
    bool memory_analysis = false;
    bool save_csv = false;
    bool save_baseline = false;
    bool compare_baseline = false;
    std::string baseline_file = "benchmark/baseline.json";
    std::string filter_algorithm = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg == "-q" || arg == "--quick") quick = true;
        else if (arg == "-m" || arg == "--memory") memory_analysis = true;
        else if (arg == "--csv") save_csv = true;
        else if (arg == "--save-baseline") save_baseline = true;
        else if (arg == "--compare-baseline") compare_baseline = true;
        else if (arg == "--baseline-file" && i + 1 < argc) {
            baseline_file = argv[++i];
        }
        else if ((arg == "-f" || arg == "--filter") && i + 1 < argc) {
            filter_algorithm = argv[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -v, --verbose         Verbose output\n"
                      << "  -q, --quick           Quick benchmark (fewer runs)\n"
                      << "  -m, --memory          Include memory analysis\n"
                      << "  -f, --filter ALGO     Run only specified algorithm (e.g., HQ3x)\n"
                      << "  --csv                 Save results to CSV file\n"
                      << "  --save-baseline       Save results as baseline\n"
                      << "  --compare-baseline    Compare with baseline\n"
                      << "  --baseline-file FILE  Baseline file (default: benchmark/baseline.json)\n"
                      << "  -h, --help            Show this help\n";
            return 0;
        }
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Benchmark parameters
    int warmup_runs = quick ? 2 : 5;
    int bench_runs = quick ? 10 : 50;

    std::cout << "=== Scaler Library Performance Benchmark ===" << std::endl;
    std::cout << "Mode: " << (quick ? "Quick" : "Full") << std::endl;
    std::cout << "Using algorithm database and CPU unified scaler" << std::endl;

    // List available algorithms
    auto cpu_algos = get_cpu_algorithms();
    std::cout << "\nAvailable CPU algorithms: ";
    for (const auto& [algo, info] : cpu_algos) {
        std::cout << info.name << " ";
    }
    std::cout << std::endl;

    // Test configurations
    struct TestConfig {
        int width, height;
        std::string pattern;
        std::string description;
        bool is_real_image;
    };

    std::vector<TestConfig> configs;

    // Always include rotozoom as the first test (real image)
    configs.push_back({256, 200, "rotozoom", "Rotozoom (Real Image 256x200)", true});

    if (quick) {
        // Add synthetic patterns
        configs.push_back({64, 64, "random", "Small Random (64x64)", false});
        configs.push_back({256, 256, "gradient", "Medium Gradient (256x256)", false});
        configs.push_back({512, 512, "checkerboard", "Large Checkerboard (512x512)", false});
    } else {
        // Add synthetic patterns
        configs.push_back({32, 32, "solid", "Tiny Solid (32x32)", false});
        configs.push_back({64, 64, "random", "Small Random (64x64)", false});
        configs.push_back({128, 128, "gradient", "Small Gradient (128x128)", false});
        configs.push_back({256, 256, "checkerboard", "Medium Checkerboard (256x256)", false});
        configs.push_back({512, 512, "random", "Large Random (512x512)", false});
        configs.push_back({1024, 768, "gradient", "HD Ready (1024x768)", false});
        configs.push_back({1920, 1080, "checkerboard", "Full HD (1920x1080)", false});
    }

    // Store all results for CSV output
    std::vector<std::tuple<std::string, int, int, std::map<std::string, BenchmarkStats>>> all_results;

    // Run benchmarks
    for (const auto& config : configs) {
        SDL_Surface* test_image = nullptr;

        if (config.is_real_image && config.pattern == "rotozoom") {
            test_image = load_rotozoom_image();
        } else {
            test_image = create_test_image(config.width, config.height, config.pattern);
        }

        if (!test_image) {
            std::cerr << "Failed to create/load test image: " << config.description << std::endl;
            continue;
        }

        // Run benchmark
        std::map<std::string, BenchmarkStats> results;

        if (verbose) {
            benchmark_all_algorithms(test_image, config.description, warmup_runs, bench_runs, filter_algorithm, verbose);
        } else {
            std::cout << "\nBenchmarking: " << config.description << std::endl;

            // Benchmark CPU algorithms
            for (const auto& [algo, info] : cpu_algos) {
                // Skip if filter is set and doesn't match
                if (!filter_algorithm.empty() && info.name != filter_algorithm) continue;

                // Check if algorithm supports 2x scaling
                bool supports_2x = false;
                for (int scale : info.cpu_supported_scales) {
                    if (scale == 2) {
                        supports_2x = true;
                        break;
                    }
                }

                if (!supports_2x && !info.cpu_arbitrary_scale) continue;

                results[info.name] = benchmark_algorithm(info.name, algo, test_image, warmup_runs, bench_runs, verbose);
            }

            // Print summary
            std::cout << "  Results: ";
            for (const auto& [name, stats] : results) {
                std::cout << name << "=" << std::fixed << std::setprecision(2)
                          << stats.mean() << "ms ";
            }
            std::cout << std::endl;
        }

        if (memory_analysis && config.width == 256) {
            benchmark_memory_usage(test_image);
        }

        all_results.emplace_back(config.pattern, test_image->w, test_image->h, results);

        SDL_DestroySurface(test_image);
    }

    // Save CSV if requested
    if (save_csv) {
        output_csv("benchmark_results.csv", all_results);
    }

    // Save baseline if requested
    if (save_baseline) {
        save_baseline_json(baseline_file, all_results);
    }

    // Compare with baseline if requested
    if (compare_baseline) {
        auto baseline = load_baseline(baseline_file);
        if (!baseline.empty()) {
            std::cout << "\n=== Baseline Comparison ===" << std::endl;

            bool regression_found = false;
            for (const auto& [pattern, width, height, results] : all_results) {
                for (const auto& [algo, stats] : results) {
                    auto comparison = get_baseline_comparison(pattern, width, height, algo, stats, baseline);

                    // Color coding based on change
                    const char* color = "";
                    const char* reset = "";
                    if (isatty(fileno(stdout))) {
                        if (comparison.regression) {
                            color = "\033[31m"; // Red
                            reset = "\033[0m";
                        } else if (comparison.change_percent < -5.0) {
                            color = "\033[32m"; // Green
                            reset = "\033[0m";
                        }
                    }

                    if (std::abs(comparison.change_percent) > 5.0 || verbose) {
                        std::cout << color;
                        std::cout << comparison.test_name << ": ";
                        printf("%.2f ms -> %.2f ms (%+.1f%%)",
                               comparison.baseline_time, comparison.current_time, comparison.change_percent);
                        if (comparison.regression) {
                            std::cout << " [REGRESSION]";
                            regression_found = true;
                        }
                        std::cout << reset << std::endl;
                    }
                }
            }

            if (regression_found) {
                std::cout << "\n⚠️  Performance regressions detected!" << std::endl;
            } else {
                std::cout << "\n✅ No performance regressions detected" << std::endl;
            }
        }
    }

    // Performance summary
    std::cout << "\n=== Performance Summary ===" << std::endl;
    std::cout << "Benchmark completed successfully" << std::endl;
    std::cout << "Run with -v for detailed output" << std::endl;
    std::cout << "Run with --csv to save results" << std::endl;
    std::cout << "Run with -m for memory analysis" << std::endl;
    std::cout << "Run with --save-baseline to save as baseline" << std::endl;
    std::cout << "Run with --compare-baseline to compare with baseline" << std::endl;

    SDL_Quit();
    return 0;
}