#include <scaler/sdl/sdl_image.hh>
#include <scaler/epx.hh>
#include <scaler/eagle.hh>
#include <scaler/2xsai.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>
#include <scaler/scale2x_sfx.hh>
#include <scaler/scale3x.hh>
#include <scaler/scale3x_sfx.hh>
#include <scaler/omniscale.hh>
#include <scaler/sdl/sdl_scalers.hh>

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
#include <iomanip>
#include <unistd.h>  // For isatty

// Include embedded test image
#include "../unittest/rotozoom_bmp.h"
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
    SDL_IOStream* io = SDL_IOFromConstMem(data_rotozoom_bmp, data_rotozoom_bmp_len);
    if (!io) {
        std::cerr << "Failed to create IO stream for rotozoom image" << std::endl;
        return nullptr;
    }
    
    SDL_Surface* surface = SDL_LoadBMP_IO(io, true); // true = close IO after load
    if (!surface) {
        std::cerr << "Failed to load rotozoom BMP: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    
    // Convert to RGBA for consistency with other test images
    SDL_Surface* rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(surface);
    
    return rgba_surface;
}

// Test image generator
SDL_Surface* create_test_image(int width, int height, const std::string& pattern) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    
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
                pixels[y * width + x] = SDL_MapRGBA(surface->format, r, g, b, 255);
            }
        }
    } else if (pattern == "checkerboard") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool is_white = ((x / 8) + (y / 8)) % 2 == 0;
                Uint8 color = is_white ? 255 : 0;
                pixels[y * width + x] = SDL_MapRGBA(surface->format, color, color, color, 255);
            }
        }
    } else if (pattern == "solid") {
        Uint32 color = SDL_MapRGBA(surface->format, 128, 128, 128, 255);
        for (int i = 0; i < width * height; ++i) {
            pixels[i] = color;
        }
    }
    
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    
    return surface;
}

// Benchmark runner for a single algorithm
template<typename ScaleFunc>
BenchmarkStats benchmark_algorithm(
    const std::string& name,
    SDL_Surface* input,
    ScaleFunc scale_func,
    int warmup_runs,
    int bench_runs,
    bool verbose = false) {
    
    BenchmarkStats stats;
    Timer timer;
    
    // Warmup runs
    if (verbose) std::cout << "  Warming up " << name << "..." << std::flush;
    for (int i = 0; i < warmup_runs; ++i) {
        SDL_Surface* output = scale_func(input);
        SDL_FreeSurface(output);
    }
    if (verbose) std::cout << " done" << std::endl;
    
    // Benchmark runs
    if (verbose) std::cout << "  Benchmarking " << name << "..." << std::flush;
    for (int i = 0; i < bench_runs; ++i) {
        timer.start();
        SDL_Surface* output = scale_func(input);
        double elapsed = timer.elapsed_ms();
        SDL_FreeSurface(output);
        
        stats.add_sample(elapsed);
        
        if (verbose && (i + 1) % 10 == 0) {
            std::cout << "." << std::flush;
        }
    }
    if (verbose) std::cout << " done" << std::endl;
    
    return stats;
}

// Benchmark all algorithms with a given image
void benchmark_all_algorithms(SDL_Surface* input, const std::string& description,
                             int warmup_runs = 5, int bench_runs = 50) {
    std::cout << "\n=== Benchmark: " << description << " ===" << std::endl;
    std::cout << "Image size: " << input->w << "x" << input->h 
              << " (" << (input->w * input->h / 1000000.0) << " MP)" << std::endl;
    std::cout << "Warmup runs: " << warmup_runs << ", Benchmark runs: " << bench_runs << std::endl;
    
    std::map<std::string, BenchmarkStats> results;
    
    // Benchmark each algorithm
    results["2xSaI"] = benchmark_algorithm("2xSaI", input, scale2xSaISDL, warmup_runs, bench_runs);
    results["EPX"] = benchmark_algorithm("EPX", input, scaleEpxSDL, warmup_runs, bench_runs);
    results["Eagle"] = benchmark_algorithm("Eagle", input, scaleEagleSDL, warmup_runs, bench_runs);
    results["HQ2x"] = benchmark_algorithm("HQ2x", input, scaleHq2xSDL, warmup_runs, bench_runs);
    results["XBR"] = benchmark_algorithm("XBR", input, scaleXbrSDL, warmup_runs, bench_runs);
    results["Scale2xSFX"] = benchmark_algorithm("Scale2xSFX", input, scaleScale2xSFXSDL, warmup_runs, bench_runs);
    results["Scale3x"] = benchmark_algorithm("Scale3x", input, scaleScale3xSDL, warmup_runs, bench_runs);
    results["Scale3xSFX"] = benchmark_algorithm("Scale3xSFX", input, scaleScale3xSFXSDL, warmup_runs, bench_runs);
    results["OmniScale2x"] = benchmark_algorithm("OmniScale2x", input, scaleOmniScale2xSDL, warmup_runs, bench_runs);
    results["OmniScale3x"] = benchmark_algorithm("OmniScale3x", input, scaleOmniScale3xSDL, warmup_runs, bench_runs);
    
    // Print results table
    std::cout << "\nResults:\n";
    std::cout << std::setw(10) << "Algorithm" 
              << std::setw(12) << "Mean (ms)"
              << std::setw(12) << "StdDev (ms)"
              << std::setw(12) << "Min (ms)"
              << std::setw(12) << "Max (ms)"
              << std::setw(15) << "Throughput"
              << std::endl;
    std::cout << std::string(71, '-') << std::endl;
    
    for (const auto& [name, stats] : results) {
        std::cout << std::setw(10) << name
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.mean()
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.stddev()
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.min_time
                  << std::setw(12) << std::fixed << std::setprecision(3) << stats.max_time
                  << std::setw(12) << std::fixed << std::setprecision(2) 
                  << stats.throughput_mpps(input->w, input->h) << " MP/s"
                  << std::endl;
    }
    
    // Find fastest algorithm
    auto fastest = std::min_element(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second.mean() < b.second.mean(); });
    
    std::cout << "\nFastest: " << fastest->first << " (" 
              << std::fixed << std::setprecision(3) << fastest->second.mean() << " ms)\n";
    
    // Print relative performance
    std::cout << "\nRelative Performance (vs EPX):\n";
    double epx_time = results["EPX"].mean();
    for (const auto& [name, stats] : results) {
        double relative = epx_time / stats.mean();
        std::cout << std::setw(10) << name << ": " 
                  << std::fixed << std::setprecision(2) << relative << "x"
                  << (relative > 1.0 ? " faster" : " slower") << std::endl;
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
    writer.add_string("version", "1.0");
    writer.add_string("date", date_stream.str());
    writer.add_string("build_type", "Release");  // TODO: Get from CMake
    
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
    
    bool first_config = true;
    for (const auto& [pattern, width, height, results] : all_results) {
        std::string config_key = pattern + "_" + std::to_string(width) + "x" + std::to_string(height);
        
        if (!first_config) {
            writer.json << ",";
        }
        writer.begin_nested_object(config_key);
        
        for (const auto& [algo, stats] : results) {
            writer.add_nested_number(algo + "_mean", stats.mean());
            writer.add_nested_number(algo + "_stddev", stats.stddev());
            writer.add_nested_number(algo + "_min", stats.min_time);
        }
        
        writer.end_nested_object();
        first_config = false;
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

// Compare current results with baseline
void compare_with_baseline(const std::string& baseline_file,
                          const std::vector<std::tuple<std::string, int, int, std::map<std::string, BenchmarkStats>>>& all_results) {
    auto baseline = load_baseline(baseline_file);
    
    if (baseline.empty()) {
        std::cout << "\nNo baseline data available for comparison." << std::endl;
        std::cout << "Run with --save-baseline to create a baseline." << std::endl;
        return;
    }
    
    std::cout << "\n=== Baseline Comparison ===" << std::endl;
    std::cout << "Comparing with baseline from: " << baseline_file << std::endl;
    std::cout << std::setw(30) << "Test" 
              << std::setw(15) << "Algorithm"
              << std::setw(12) << "Baseline"
              << std::setw(12) << "Current"
              << std::setw(12) << "Change %"
              << std::setw(12) << "Status" << std::endl;
    std::cout << std::string(93, '-') << std::endl;
    
    int regressions = 0;
    int improvements = 0;
    
    for (const auto& [pattern, width, height, results] : all_results) {
        std::string config = pattern + "_" + std::to_string(width) + "x" + std::to_string(height);
        
        for (const auto& [algo, stats] : results) {
            std::string key = algo + "_mean";
            
            auto it = baseline.find(key);
            if (it != baseline.end()) {
                double baseline_time = it->second;
                double current_time = stats.mean();
                double percent_change = ((current_time - baseline_time) / baseline_time) * 100.0;
                bool is_regression = percent_change > 10.0;
                
                std::cout << std::setw(30) << config
                          << std::setw(15) << algo
                          << std::setw(12) << std::fixed << std::setprecision(2) << baseline_time
                          << std::setw(12) << std::fixed << std::setprecision(2) << current_time
                          << std::setw(11) << std::fixed << std::setprecision(1) << percent_change << "%";
                
                if (is_regression) {
                    std::cout << std::setw(12) << "REGRESSION";
                    regressions++;
                } else if (percent_change < -10.0) {
                    std::cout << std::setw(12) << "IMPROVED";
                    improvements++;
                } else {
                    std::cout << std::setw(12) << "OK";
                }
                std::cout << std::endl;
            }
        }
    }
    
    std::cout << "\nSummary: " << improvements << " improvements, " 
              << regressions << " regressions" << std::endl;
    
    if (regressions > 0) {
        std::cout << "WARNING: Performance regressions detected!" << std::endl;
    }
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
        else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -v, --verbose         Verbose output\n"
                      << "  -q, --quick           Quick benchmark (fewer runs)\n"
                      << "  -m, --memory          Include memory analysis\n"
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
            benchmark_all_algorithms(test_image, config.description, warmup_runs, bench_runs);
        } else {
            std::cout << "\nBenchmarking: " << config.description << std::endl;
            
            results["2xSaI"] = benchmark_algorithm("2xSaI", test_image, scale2xSaISDL, warmup_runs, bench_runs, verbose);
            results["EPX"] = benchmark_algorithm("EPX", test_image, scaleEpxSDL, warmup_runs, bench_runs, verbose);
            results["Eagle"] = benchmark_algorithm("Eagle", test_image, scaleEagleSDL, warmup_runs, bench_runs, verbose);
            results["HQ2x"] = benchmark_algorithm("HQ2x", test_image, scaleHq2xSDL, warmup_runs, bench_runs, verbose);
            results["XBR"] = benchmark_algorithm("XBR", test_image, scaleXbrSDL, warmup_runs, bench_runs, verbose);
            results["Scale2xSFX"] = benchmark_algorithm("Scale2xSFX", test_image, scaleScale2xSFXSDL, warmup_runs, bench_runs, verbose);
            results["Scale3x"] = benchmark_algorithm("Scale3x", test_image, scaleScale3xSDL, warmup_runs, bench_runs, verbose);
            results["Scale3xSFX"] = benchmark_algorithm("Scale3xSFX", test_image, scaleScale3xSFXSDL, warmup_runs, bench_runs, verbose);
            results["OmniScale2x"] = benchmark_algorithm("OmniScale2x", test_image, scaleOmniScale2xSDL, warmup_runs, bench_runs, verbose);
            results["OmniScale3x"] = benchmark_algorithm("OmniScale3x", test_image, scaleOmniScale3xSDL, warmup_runs, bench_runs, verbose);
            
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
        
        SDL_FreeSurface(test_image);
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