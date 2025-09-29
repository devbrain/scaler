# CMake Integration Guide

This document describes how to integrate the scaler library into your CMake-based project.

## Quick Start with FetchContent (Recommended)

The easiest way to use the scaler library is with CMake's FetchContent module. No installation required!

```cmake
cmake_minimum_required(VERSION 3.20)
project(YourProject LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    scaler
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG main  # or use a specific version tag
)
FetchContent_MakeAvailable(scaler)

# Now link against the library
add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE scaler::scaler)
```

## Alternative Integration Methods

### Method 1: Using find_package

First install the library:
```bash
git clone https://github.com/yourusername/scaler.git
cd scaler
cmake -B build -DCMAKE_INSTALL_PREFIX=/your/install/path
cmake --build build
cmake --install build
```

Then in your CMakeLists.txt:
```cmake
find_package(scaler REQUIRED)
target_link_libraries(your_app PRIVATE scaler::scaler)
```

### Method 2: As a Git Submodule

```bash
git submodule add https://github.com/yourusername/scaler.git external/scaler
```

In CMakeLists.txt:
```cmake
add_subdirectory(external/scaler)
target_link_libraries(your_app PRIVATE scaler::scaler)
```

## Build Options

When using FetchContent or add_subdirectory, you can control build options:

```cmake
set(SCALER_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(SCALER_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(scaler)
```

## Checking Available Features

The library configuration provides variables to check which features are available:

```cmake
if(SCALER_HAS_OPENGL)
    message(STATUS "GPU scaling is available")
    target_compile_definitions(your_app PRIVATE HAS_GPU_SCALING)
endif()

if(SCALER_HAS_SDL)
    message(STATUS "SDL support is available (version ${SCALER_SDL_VERSION})")
endif()
```

## Image Class Requirements

Your image class needs to implement these methods for CPU scaling:

```cpp
class YourImage {
public:
    using pixel_type = scaler::vec3<uint8_t>;  // or your pixel type

    // Required constructors
    YourImage(size_t width, size_t height);
    YourImage(size_t width, size_t height, const YourImage& source);

    // Required methods
    size_t width() const;
    size_t height() const;
    pixel_type get_pixel(size_t x, size_t y) const;
    void set_pixel(size_t x, size_t y, const pixel_type& p);

    // Optional but recommended for some algorithms
    pixel_type safe_access(int x, int y) const;  // with boundary clamping
};
```

## Example Usage

```cpp
#include <scaler/unified_scaler.hh>
#include <scaler/algorithm_capabilities.hh>

// Query available algorithms
auto algorithms = scaler::algorithm_capabilities::get_all_algorithms();

// Scale an image
YourImage input = LoadImage("input.png");
auto output = scaler::unified_scaler<YourImage, YourImage>::scale(
    input, scaler::algorithm::EPX, 2.0f
);

// Scale into preallocated buffer
YourImage output2(input.width() * 3, input.height() * 3);
scaler::unified_scaler<YourImage, YourImage>::scale(
    input, output2, scaler::algorithm::HQ
);
```

## GPU Scaling

If OpenGL is available:

```cpp
#include <scaler/gpu/unified_gpu_scaler.hh>

scaler::gpu::unified_gpu_scaler gpu_scaler;
GLuint scaled = gpu_scaler.scale_texture(
    input_texture, width, height,
    scaler::algorithm::xBR, 2.0f
);
```

## Troubleshooting

### "Could not find package" Error
- Use FetchContent instead of find_package
- Or ensure CMAKE_PREFIX_PATH includes the install location
- Or use `-Dscaler_DIR=/path/to/install/lib/cmake/scaler`

### Linking Errors
- Ensure SDL/OpenGL/GLEW are available if the library was built with them
- Link with `scaler::scaler` (not just `scaler`)

### Compilation Errors
- Ensure your image class has all required methods
- The third constructor parameter is needed by some algorithms
- safe_access method is required by HQ and some other algorithms

## Dependencies

- **Core CPU algorithms**: Header-only, no dependencies required
- **SDL support**: Optional, for SDL image wrapper classes
- **OpenGL/GLEW**: Optional, for GPU acceleration

## CMake Minimum Version

The library requires CMake 3.20 or later. For older CMake versions, consider using the library as a git submodule with manual configuration.