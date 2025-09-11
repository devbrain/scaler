# Scaler

A high-performance, header-only C++ library for pixel art scaling algorithms.

## Overview

Scaler provides efficient implementations of popular pixel art scaling algorithms including 2xSAI, Eagle, EPX, HQ2x, HQ3x, XBR, and Omniscale. The library features a framework-agnostic design using CRTP (Curiously Recurring Template Pattern) with zero runtime overhead.

## Features

- **Header-only library** - No compilation required, just include and use
- **Framework-agnostic** - Works with any image backend through CRTP interfaces
- **High performance** - Cache-optimized with sliding window buffers
- **Multiple algorithms** - 2xSAI, Eagle, EPX/AdvMAME, HQ2x, HQ3x, XBR, Omniscale, Scale2x/3x
- **SDL integration** - Optional SDL2/SDL3 backend provided
- **Comprehensive testing** - Extensive unit tests with golden data validation
- **Cross-platform** - Works on Linux, macOS, and Windows
- **Modern C++** - Requires C++17 or later

## Supported Algorithms

| Algorithm | Scale Factor | Description |
|-----------|--------------|-------------|
| 2xSAI | 2x | Smooth anti-aliased scaling |
| Eagle | 2x | Sharp pixel art scaling |
| EPX/AdvMAME | 2x | Simple and fast scaling |
| HQ2x | 2x | High quality with edge detection |
| HQ3x | 3x | High quality 3x scaling |
| XBR | 2x | Advanced edge-directed interpolation |
| Omniscale | 2x-6x | Multi-factor scaling algorithm |
| Scale2x/3x | 2x/3x | Fast integer scaling |
| Scale2x-SFX | 2x | Improved Scale2x variant |
| Scale3x-SFX | 3x | Improved Scale3x variant |

## Quick Start

### Installation

Clone the repository:
```bash
git clone https://github.com/yourusername/scaler.git
cd scaler
```

### Integration into Your Project

#### Method 1: Using FetchContent (Recommended)

Add to your CMakeLists.txt:
```cmake
include(FetchContent)
FetchContent_Declare(
    scaler
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG main
)
FetchContent_MakeAvailable(scaler)

target_link_libraries(your_target PRIVATE scaler::scaler)
```

#### Method 2: Using find_package

First install the library:
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/your/install/path
cmake --build build
cmake --install build
```

Then in your CMakeLists.txt:
```cmake
find_package(scaler REQUIRED)
target_link_libraries(your_target PRIVATE scaler::scaler)
```

#### Method 3: Using add_subdirectory

```cmake
add_subdirectory(path/to/scaler)
target_link_libraries(your_target PRIVATE scaler::scaler)
```

### Building (Optional)

The library is header-only, but you can build tests and tools:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Basic Usage

#### Using the Framework-Agnostic Interface

```cpp
#include <scaler/epx.hh>
#include <scaler/image_base.hh>

// Define your image class
class MyImage : public scaler::InputImageBase<MyImage, RGB>,
                public scaler::OutputImageBase<MyImage, RGB> {
    // Implement required methods
    size_t width_impl() const;
    size_t height_impl() const;
    RGB get_pixel_impl(size_t x, size_t y) const;
    void set_pixel_impl(size_t x, size_t y, const RGB& pixel);
};

// Scale an image
MyImage input = LoadImage("input.png");
auto output = scaler::scaleEpx<MyImage, MyImage>(input);
```

#### Using the SDL Backend

```cpp
#include <scaler/sdl/sdl_scalers.hh>
#include <SDL.h>

SDL_Surface* input = SDL_LoadBMP("input.bmp");
SDL_Surface* scaled = scaler::scaleEpxSDL(input);
// Use scaled surface...
SDL_FreeSurface(scaled);
```

## Architecture

The library uses a CRTP-based design for maximum flexibility and performance:

- **InputImageBase/OutputImageBase** - Base classes providing the interface
- **Algorithm headers** - Each algorithm in its own header (e.g., `epx.hh`, `xbr.hh`)
- **SDL integration** - Optional SDL backend in `sdl/` subdirectory
- **Optimizations** - Cache-friendly sliding window buffers for efficient processing

## Building with Options

```bash
# Build with tests
cmake -B build -DSCALER_BUILD_TEST=ON
cmake --build build

# Run tests
./build/bin/scaler_unittest

# Build with sanitizers (debug)
cmake -B build -DSCALER_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Build benchmarks
cmake -B build -DSCALER_BUILD_BENCHMARK=ON
cmake --build build
./build/bin/benchmark_scalers
```

## Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.20+ (for building tests/tools)
- SDL3 (optional, for SDL backend)

## Project Structure

```
scaler/
├── include/scaler/        # Header-only library
│   ├── image_base.hh      # CRTP base classes
│   ├── 2xsai.hh           # 2xSAI algorithm
│   ├── eagle.hh           # Eagle algorithm
│   ├── epx.hh             # EPX algorithm
│   ├── hq2x.hh            # HQ2x algorithm
│   ├── hq3x.hh            # HQ3x algorithm
│   ├── xbr.hh             # XBR algorithm
│   ├── omniscale.hh       # Omniscale algorithm
│   └── sdl/               # SDL integration
│       ├── sdl_image.hh   # SDL image wrapper
│       └── sdl_scalers.hh # Convenience functions
├── unittest/              # Unit tests
├── benchmark/             # Performance benchmarks
└── tools/                 # Command-line tools
```

## Performance

The library is optimized for performance with:
- Cache-friendly sliding window buffers
- Minimal memory allocations
- SIMD-friendly data layouts
- Zero-cost CRTP abstractions

Benchmark results on typical hardware show processing speeds of 100+ megapixels/second for most algorithms.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Algorithm implementations based on research from the emulation and pixel art communities
- Test images from various open-source projects
- Inspired by similar scaling libraries like hqx and xBR

## References

- [2xSAI](http://vdnoort.home.xs4all.nl/emulation/2xsai/)
- [HQx](https://en.wikipedia.org/wiki/Hqx)
- [XBR Algorithm](https://forums.libretro.com/t/xbr-algorithm-tutorial/123)
- [Scale2x](https://www.scale2x.it/)