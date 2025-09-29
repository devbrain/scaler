# Example of using scaler library with FetchContent
# Add this to your project's CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(MyProject LANGUAGES CXX)

# Include FetchContent module
include(FetchContent)

# Option 1: Fetch from GitHub (when repository is public)
FetchContent_Declare(
    scaler
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG        main  # or specific version tag like v1.0.0
)

# Option 2: Fetch from local directory (for development)
# FetchContent_Declare(
#     scaler
#     SOURCE_DIR /path/to/local/scaler
# )

# Option 3: Fetch from URL
# FetchContent_Declare(
#     scaler
#     URL https://github.com/yourusername/scaler/archive/v1.0.0.tar.gz
#     URL_HASH SHA256=<hash>
# )

# Make the content available
FetchContent_MakeAvailable(scaler)

# Now you can use the library in your targets
add_executable(my_app main.cpp)

# Link against scaler library
target_link_libraries(my_app PRIVATE scaler::scaler)

# The library automatically handles:
# - Include directories
# - SDL dependencies (if available)
# - OpenGL/GLEW dependencies (if available)
# - Compile definitions

# Example usage in your code:
# #include <scaler/unified_scaler.hh>
# #include <scaler/algorithm_capabilities.hh>
#
# // For GPU scaling (if OpenGL is available):
# #include <scaler/gpu/unified_gpu_scaler.hh>