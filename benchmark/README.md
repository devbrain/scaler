# Scaler Library Benchmark Suite

## Overview

Comprehensive benchmarking and profiling tools for the scaler library's pixel art scaling algorithms.

## Quick Start

### Build
```bash
# Standard build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target benchmark_scalers

# Performance build (optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Performance
cmake --build build --target benchmark_scalers

# Profiling build (for gprof)
cmake -B build -DSCALER_ENABLE_PROFILING=ON
cmake --build build --target benchmark_scalers
```

### Run Benchmarks
```bash
# Quick benchmark (fewer iterations)
./build/bin/benchmark_scalers --quick

# Full benchmark
./build/bin/benchmark_scalers

# Verbose output with detailed timings
./build/bin/benchmark_scalers --verbose

# Save results to CSV
./build/bin/benchmark_scalers --csv

# Include memory analysis
./build/bin/benchmark_scalers --memory

# Save performance baseline
./build/bin/benchmark_scalers --save-baseline

# Compare against baseline (detect regressions)
./build/bin/benchmark_scalers --compare-baseline

# Use custom baseline file
./build/bin/benchmark_scalers --compare-baseline --baseline-file my_baseline.json
```

## Profiling

### Using the profile.sh Script
```bash
cd benchmark

# Run gprof profiling
./profile.sh gprof --quick

# Run valgrind cachegrind (cache analysis)
./profile.sh cachegrind --quick

# Run valgrind callgrind (call graph)
./profile.sh callgrind --quick

# Run valgrind memcheck (memory leaks)
./profile.sh memcheck --quick

# Run valgrind massif (heap profiling)
./profile.sh massif --quick

# Clean all profiling outputs
./profile.sh clean
```

### Manual Profiling

#### Gprof
```bash
# Build with profiling
cmake -B build -DSCALER_ENABLE_PROFILING=ON
cmake --build build --target benchmark_scalers

# Run and generate gmon.out
./build/bin/benchmark_scalers --quick

# Generate report
gprof ./build/bin/benchmark_scalers gmon.out > gprof_report.txt
```

#### Valgrind Cachegrind (Cache Performance)
```bash
valgrind --tool=cachegrind \
         --cachegrind-out-file=cachegrind.out \
         ./build/bin/benchmark_scalers --quick

# View results
cg_annotate cachegrind.out
```

#### Valgrind Callgrind (Call Graph)
```bash
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         ./build/bin/benchmark_scalers --quick

# View with KCachegrind (GUI)
kcachegrind callgrind.out

# Or text report
callgrind_annotate callgrind.out
```

#### Valgrind Memcheck (Memory Leaks)
```bash
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/bin/benchmark_scalers --quick
```

#### Valgrind Massif (Heap Profiling)
```bash
valgrind --tool=massif \
         --massif-out-file=massif.out \
         ./build/bin/benchmark_scalers --quick

# View results
ms_print massif.out
```

## Benchmark Results

The benchmark tests each algorithm with various image sizes and patterns:

### Test Patterns
- **Solid**: Uniform color (best case for cache)
- **Random**: Random pixels (worst case for prediction)
- **Gradient**: Smooth gradients (typical case)
- **Checkerboard**: High contrast patterns (edge detection test)

### Image Sizes
- 32x32 (Tiny)
- 64x64 (Small)
- 256x256 (Medium)
- 512x512 (Large)
- 1024x768 (HD Ready)
- 1920x1080 (Full HD)

### Metrics
- **Mean Time**: Average processing time in milliseconds
- **StdDev**: Standard deviation of timings
- **Min/Max**: Best and worst case timings
- **Throughput**: Megapixels per second

## Baseline Comparison

The benchmark tool includes a baseline comparison feature to detect performance regressions:

### Creating a Baseline
```bash
# Run benchmarks and save as baseline
./build/bin/benchmark_scalers --save-baseline

# Creates benchmark/baseline.json with current performance data
```

### Comparing Against Baseline
```bash
# Compare current performance against baseline
./build/bin/benchmark_scalers --compare-baseline

# Shows performance changes:
# - Green: >5% improvement
# - Red: >10% regression (marked as [REGRESSION])
# - Only significant changes (>5%) are shown by default
```

### Baseline File Format
The baseline is stored as JSON with:
- System information (CPU, build type, date)
- Performance metrics for each test configuration
- Mean, stddev, and min times for each algorithm

### CI/CD Integration
Use baseline comparison in CI to catch performance regressions:
```bash
# In CI pipeline:
./build/bin/benchmark_scalers --quick --compare-baseline
if [ $? -ne 0 ]; then
    echo "Performance regression detected!"
    exit 1
fi
```

## Performance Tips

### Optimizing for Speed
1. Use Performance build type: `-DCMAKE_BUILD_TYPE=Performance`
2. Enable Link-Time Optimization (LTO)
3. Consider SIMD implementations for critical paths

### Memory Optimization
1. Process images in tiles to improve cache locality
2. Use smaller pixel formats when possible
3. Implement streaming for large images

### Profiling Workflow
1. Start with `benchmark_scalers` to identify slow algorithms
2. Use `cachegrind` to analyze cache performance
3. Use `callgrind` to find hot functions
4. Use `gprof` for detailed timing analysis
5. Use `memcheck` to ensure no memory leaks
6. Use `massif` to analyze memory usage patterns

## Typical Results (Intel i7, Release build)

| Algorithm | 256x256 | 512x512 | 1920x1080 |
|-----------|---------|---------|-----------|
| EPX       | ~4ms    | ~17ms   | ~170ms    |
| Eagle     | ~5ms    | ~24ms   | ~240ms    |
| 2xSaI     | ~9ms    | ~35ms   | ~350ms    |
| HQ2x      | ~14ms   | ~51ms   | ~500ms    |
| XBR       | ~37ms   | ~130ms  | ~1300ms   |

*Note: Actual performance varies based on CPU, compiler optimizations, and image content.*

## CSV Output Format

When using `--csv`, results are saved with the following columns:
- Pattern: Test pattern used (random/gradient/checkerboard/solid)
- Width, Height: Input image dimensions
- Algorithm: Scaling algorithm name
- Mean_ms: Average time in milliseconds
- StdDev_ms: Standard deviation
- Min_ms, Max_ms: Range of timings
- Throughput_MPps: Megapixels per second

## Troubleshooting

### "Command not found" for valgrind/gprof
Install the required tools:
```bash
# Ubuntu/Debian
sudo apt-get install valgrind gprof

# Fedora/RHEL
sudo dnf install valgrind binutils

# macOS
brew install valgrind
```

### Profiling build is slow
This is normal. Profiling builds include debug symbols and instrumentation.
Use Release or Performance builds for actual benchmarking.

### Out of memory with large images
Reduce test image sizes or run with fewer iterations using `--quick`.

## Contributing

When adding new algorithms:
1. Add benchmark test in `benchmark_scalers.cc`
2. Run profiling to ensure no performance regression
3. Document expected performance characteristics