# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++ header-only image scaling library that implements various pixel art scaling algorithms including:
- 2xSAI
- Eagle
- EPX
- HQ2x
- XBR

The library uses SDL3 for image handling and provides a simple interface for upscaling images.

## Build Commands

### Build the project
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build with tests
```bash
cmake -B build -DSCALER_BUILD_TEST=ON
cmake --build build
```

### Run tests
```bash
./build/bin/scaler_unittest
```

### Build with debug mode
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Build with sanitizers (for debugging memory issues)
```bash
cmake -B build -DSCALER_ENABLE_SANITIZERS=ON
cmake --build build
```

## Architecture

### Header-Only Library Structure
- **include/scaler/**: All implementation headers
  - `scaler_common.hh`: Common utilities (color conversion, interpolation)
  - `vec3.hh`: Vector math utilities
  - **CRTP Framework-Agnostic Interface:**
    - `image_base.hh`: CRTP base classes for input/output images
    - `sdl_image.hh`: SDL3 implementation of CRTP interfaces
    - `sdl_scalers.hh`: Convenience functions for SDL users
  - **Scaling Algorithms:**
    - Original SDL-specific: `2xsai.hh`, `eagle.hh`, `epx.hh`, `hq2x.hh`, `xbr.hh`, `image.hh`
    - CRTP-based (framework agnostic): `epx_crtp.hh` (template-based, works with any backend)

### Build System
- CMake-based build (minimum version 3.20)
- Uses FetchContent to automatically download doctest for testing
- Configurable warnings and sanitizers for different compilers (GCC, Clang, MSVC)
- Interface library design - no compilation needed, just include headers

### Testing
- Uses doctest framework (fetched automatically)
- Test executable built in `build/bin/scaler_unittest`
- Main test runner in `unittest/test_main.cc`

### Dependencies
- SDL3 (optional - only needed for SDL backend)
- doctest (for testing, fetched automatically)
- C++17 standard required

## CRTP Framework Design

The library now supports a framework-agnostic design using CRTP (Curiously Recurring Template Pattern):

- **InputImageBase/OutputImageBase**: Base classes providing common interface
- **Zero runtime overhead**: All polymorphism resolved at compile time
- **Custom backends**: Users can implement their own image classes
- **SDL3 implementation**: Provided as reference and for backward compatibility

Example custom backend:
```cpp
class MyImage : public InputImageBase<MyImage>, 
                public OutputImageBase<MyImage> {
    // Implement width_impl(), height_impl(), get_pixel_impl(), set_pixel_impl()
};
```