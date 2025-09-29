# Example of how to use the scaler library in your CMake project

# ===========================================================================
# Method 1: Using FetchContent (RECOMMENDED - no installation needed)
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

include(FetchContent)

# Fetch from GitHub
FetchContent_Declare(
    scaler
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG main  # or specific version tag like v1.0.0
    GIT_SHALLOW TRUE  # For faster clone
)

# Optional: Set build options before making available
set(SCALER_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(SCALER_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(scaler)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Method 2: Using find_package (after installation)
# ===========================================================================
# First install the library:
#   git clone https://github.com/yourusername/scaler.git
#   cd scaler
#   cmake -B build -DCMAKE_INSTALL_PREFIX=/path/to/install
#   cmake --build build
#   cmake --install build
#
# Then in your project:
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

# If installed in non-standard location
# list(APPEND CMAKE_PREFIX_PATH /path/to/install)

find_package(scaler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# Check which features are available
if(SCALER_HAS_OPENGL)
    message(STATUS "Scaler GPU support is available")
    target_compile_definitions(my_app PRIVATE HAS_GPU_SCALING)
endif()

if(SCALER_HAS_SDL)
    message(STATUS "Scaler SDL support is available (version ${SCALER_SDL_VERSION})")
endif()

# ===========================================================================
# Method 3: Using add_subdirectory (with local copy or git submodule)
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

# For git submodule:
#   git submodule add https://github.com/yourusername/scaler.git external/scaler
#   git submodule update --init --recursive

# Disable tests and examples
set(SCALER_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Add the library
add_subdirectory(external/scaler)  # or wherever you placed it

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Method 4: Using CPM (CMake Package Manager)
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

# Download CPM.cmake
include(cmake/CPM.cmake)  # or include directly from URL

CPMAddPackage(
    NAME scaler
    GITHUB_REPOSITORY yourusername/scaler
    VERSION 1.0.0
    OPTIONS
        "SCALER_BUILD_TEST OFF"
        "BUILD_EXAMPLES OFF"
)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Example Application Code
# ===========================================================================
# main.cpp:
#
# #include <scaler/unified_scaler.hh>
# #include <scaler/algorithm_capabilities.hh>
# #include <scaler/sdl/sdl_image.hh>  // If SDL is available
#
# // For GPU scaling (if OpenGL is available):
# #include <scaler/gpu/unified_gpu_scaler.hh>
#
# int main() {
#     // Query available algorithms
#     auto algorithms = scaler::algorithm_capabilities::get_all_algorithms();
#
#     for (auto algo : algorithms) {
#         const auto& info = scaler::algorithm_capabilities::get_info(algo);
#         std::cout << info.name << " - " << info.description << "\n";
#     }
#
#     // CPU scaling example
#     MyImage input = LoadImage("input.png");
#     auto output = scaler::unified_scaler<MyImage, MyImage>::scale(
#         input, scaler::algorithm::EPX, 2.0f
#     );
#
#     #ifdef HAS_GPU_SCALING
#     // GPU scaling example
#     scaler::gpu::unified_gpu_scaler gpu_scaler;
#     GLuint scaled_texture = gpu_scaler.scale_texture(
#         input_texture, width, height, scaler::algorithm::xBR, 2.0f
#     );
#     #endif
#
#     return 0;
# }

# ===========================================================================
# Advanced: Conditional Features
# ===========================================================================
# You can check what features the library was built with:

if(TARGET scaler::scaler)
    # Get compile definitions from the target
    get_target_property(SCALER_DEFINITIONS scaler::scaler INTERFACE_COMPILE_DEFINITIONS)

    # Check for specific features
    if("SCALER_HAS_OPENGL" IN_LIST SCALER_DEFINITIONS)
        message(STATUS "OpenGL support available")
    endif()

    if("SCALER_HAS_SDL2" IN_LIST SCALER_DEFINITIONS OR
       "SCALER_HAS_SDL3" IN_LIST SCALER_DEFINITIONS)
        message(STATUS "SDL support available")
    endif()
endif()

# ===========================================================================
# Troubleshooting
# ===========================================================================
#
# If you get "Could not find package":
#   - Make sure scaler is installed or use FetchContent
#   - Check CMAKE_PREFIX_PATH includes the install location
#   - Use -Dscaler_DIR=/path/to/install/lib/cmake/scaler
#
# If you get linking errors:
#   - Make sure SDL/OpenGL/GLEW are available if the library was built with them
#   - Check that you're linking with scaler::scaler (not just scaler)
#
# For minimal dependencies:
#   - The core CPU algorithms are header-only and require no dependencies
#   - SDL is optional for SDL image support
#   - OpenGL/GLEW are optional for GPU acceleration