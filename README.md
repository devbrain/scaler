# Scaler

A high-performance C++ library providing unified CPU and GPU implementations of pixel art scaling algorithms.

## 🆕 Unified Architecture
The library now features a completely unified interface for both CPU and GPU scaling:
- **Single API** for all algorithms across CPU and GPU backends
- **Algorithm database** with capability discovery
- **Type-safe template interface** with compile-time validation
- **Zero-cost abstractions** using CRTP design
- **Automatic scale factor inference** from dimensions


## Overview

Scaler provides efficient implementations of popular pixel art and image scaling algorithms with a unified interface that works seamlessly across CPU and GPU. The library features framework-agnostic design, comprehensive algorithm support, and optimal performance through cache-optimized implementations.

## Features

- **Unified CPU/GPU Interface** - Same API for both CPU and GPU implementations
- **Algorithm Database** - Runtime capability discovery and algorithm information
- **Header-only Core** - No compilation required for CPU algorithms
- **GPU Acceleration** - OpenGL-based GPU scaling with shader caching
- **Framework-agnostic** - Works with any image backend through CRTP interfaces
- **High Performance** - Cache-optimized CPU code and efficient GPU shaders
- **Multiple Algorithms** - 13+ algorithms including EPX, HQ, xBR, and OmniScale
- **SDL Integration** - Optional SDL2/SDL3 backend support
- **Modern C++17** - Template-based design with compile-time safety
- **Cross-platform** - Linux, macOS, and Windows support

## Supported Algorithms

| Algorithm | CPU Scales | GPU Scales | Description |
|-----------|------------|------------|-------------|
| Nearest | Any | Any | Nearest neighbor (pixelated) |
| Bilinear | Any | Any | Smooth bilinear interpolation |
| Trilinear | Any | - | Two-pass bilinear scaling |
| EPX/AdvMAME | 2x, 3x, 4x | 2x, 3x, 4x | Fast pixel art scaling |
| Eagle | 2x | 2x | Sharp edges for pixel art |
| Scale2x/3x | 2x, 3x | 2x, 3x | Eric's Pixel Expansion |
| ScaleFX | 2x, 3x | - | Enhanced Scale algorithm |
| Super2xSaI | 2x | 2x | Smooth anti-aliased scaling |
| HQ2x/3x/4x | 2x, 3x, 4x | 2x, 3x, 4x | High quality with edge detection |
| AAScale | 2x, 3x | - | Anti-aliased scaling |
| xBR | 2x, 3x, 4x | 2x, 3x, 4x | Advanced edge interpolation |
| OmniScale | 2x | 2x | Advanced pattern recognition |

## Quick Start

### Installation

```bash
git clone https://github.com/yourusername/scaler.git
cd scaler
```

### Basic Usage - Unified Interface

```cpp
#include <scaler/unified_scaler.hh>
#include <scaler/algorithm_capabilities.hh>

// Query available algorithms
auto algorithms = scaler::algorithm_capabilities::get_all_algorithms();
for (auto algo : algorithms) {
    const auto& info = scaler::algorithm_capabilities::get_info(algo);
    std::cout << info.name << " - " << info.description << "\n";
}

// Scale using unified interface (CPU)
MyImage input = LoadImage("input.png");
auto output = scaler::unified_scaler<MyImage, MyImage>::scale(
    input, scaler::algorithm::EPX, 2.0f
);

// Scale into preallocated buffer (scale inferred from dimensions)
MyImage output2(input.width() * 3, input.height() * 3);
scaler::unified_scaler<MyImage, MyImage>::scale(
    input, output2, scaler::algorithm::HQ
);
```

### GPU Scaling

```cpp
#include <scaler/gpu/unified_gpu_scaler.hh>
#include <GL/glew.h>

// Initialize OpenGL context first
glewInit();

// Create GPU scaler
scaler::gpu::unified_gpu_scaler scaler;

// Scale texture
GLuint scaled_texture = scaler.scale_texture(
    input_texture, width, height,
    scaler::algorithm::xBR, 2.0f
);

// Or scale into existing texture
scaler.scale_texture_into(
    input_texture, output_texture,
    width, height, scaler::algorithm::HQ
);
```

### Using Algorithm Database

```cpp
#include <scaler/algorithm_capabilities.hh>

// Check if algorithm supports a scale
if (scaler::algorithm_capabilities::is_cpu_scale_supported(
    scaler::algorithm::EPX, 2.0f)) {
    // Scale is supported
}

// Get supported scales for an algorithm
auto scales = scaler::algorithm_capabilities::get_cpu_supported_scales(
    scaler::algorithm::HQ
);

// Find algorithms that support specific scale
auto algos = scaler::algorithm_capabilities::get_cpu_algorithms_for_scale(3.0f);
```

### SDL Integration

```cpp
#include <scaler/sdl/sdl_image.hh>
#include <scaler/unified_scaler.hh>

// Wrap SDL surface
SDL_Surface* surface = SDL_LoadBMP("input.bmp");
scaler::sdl_input_image input(surface);

// Scale using unified interface
scaler::sdl_output_image output(
    input.width() * 2, input.height() * 2, surface
);
scaler::unified_scaler<scaler::sdl_input_image,
                       scaler::sdl_output_image>::scale(
    input, output, scaler::algorithm::Eagle
);

SDL_Surface* scaled = output.release();
```

## Examples

The repository includes several example applications:

### CLI Scaler Tool
```bash
# Build the CLI tool
cmake -B build -DSCALER_BUILD_EXAMPLES=ON
cmake --build build

# Scale an image
./build/bin/scaler_cli input.png output.png -a xBR -s 2

# List available algorithms
./build/bin/scaler_cli --list
```

### GPU Scaler Demo
Interactive demo with ImGui interface for testing GPU scaling algorithms:
```bash
./build/bin/gpu_scaler
```

## Architecture

### Unified Design
- **Algorithm Enum** - Single enumeration for all algorithms
- **Capability Database** - Runtime queryable algorithm properties
- **Template Interface** - Type-safe scaling with compile-time validation
- **Static Dispatch** - Zero-cost abstractions via templates

### CPU Implementation
- **CRTP Base Classes** - `input_image_base` and `output_image_base`
- **Sliding Window Buffers** - Cache-optimized processing
- **Header-only** - No compilation required

### GPU Implementation
- **OpenGL Core** - Pure OpenGL with GLSL shaders
- **Shader Cache** - Compiled shaders cached for performance
- **Texture Management** - Efficient texture creation and reuse
- **Batch Processing** - Process multiple textures efficiently

## Building

```bash
# Basic build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# With tests
cmake -B build -DSCALER_BUILD_TEST=ON
cmake --build build
ctest --test-dir build

# With examples and benchmarks
cmake -B build \
    -DSCALER_BUILD_EXAMPLES=ON \
    -DSCALER_BUILD_BENCHMARK=ON
cmake --build build
```

## Requirements

- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake 3.20+** for building
- **OpenGL 3.3+** for GPU scaling (optional)
- **SDL2/SDL3** for SDL integration (optional)

## Project Structure

```
scaler/
├── include/scaler/
│   ├── algorithm.hh              # Algorithm enumeration
│   ├── algorithm_capabilities.hh # Capability database
│   ├── unified_scaler.hh         # CPU unified interface
│   ├── cpu/                      # CPU algorithm implementations
│   │   ├── epx.hh
│   │   ├── hq2x.hh
│   │   ├── xbr.hh
│   │   └── ...
│   ├── gpu/                      # GPU implementation
│   │   ├── unified_gpu_scaler.hh
│   │   ├── opengl_texture_scaler.hh
│   │   └── shader_cache.hh
│   └── sdl/                      # SDL integration
│       └── sdl_image.hh
├── examples/                     # Example applications
│   ├── scaler_cli/               # Command-line tool
│   └── gpu_scaler/               # GPU demo with ImGui
├── unittest/                     # Comprehensive tests
└── benchmark/                    # Performance benchmarks
```

## Performance

The library is optimized for maximum performance:
- **CPU**: Cache-friendly algorithms, SIMD-ready layouts, sliding window buffers
- **GPU**: Optimized shaders, texture caching, minimal state changes
- **Benchmarks**: 100+ MP/s on modern CPUs, 1000+ MP/s on GPUs

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Algorithm implementations based on research from the emulation and pixel art communities
- GLSL shader implementations optimized for modern GPUs
- Test framework using Google Test
- Inspired by similar scaling libraries like hqx and xBR

## References

- [EPX/Scale2x](https://en.wikipedia.org/wiki/Pixel-art_scaling_algorithms#EPX/Scale2×/AdvMAME2×)
- [HQx Algorithm](https://en.wikipedia.org/wiki/Hqx)
- [xBR Algorithm](https://forums.libretro.com/t/xbr-algorithm-tutorial/123)
- [OmniScale](https://github.com/libretro/glsl-shaders/blob/master/omniscale)