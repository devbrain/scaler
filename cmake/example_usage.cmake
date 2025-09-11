# Example of how to use the scaler library in your CMake project

# ===========================================================================
# Method 1: Using find_package (after installation)
# ===========================================================================
# First install the library:
#   cmake -B build -DCMAKE_INSTALL_PREFIX=/path/to/install
#   cmake --build build
#   cmake --install build
#
# Then in your project:
cmake_minimum_required(VERSION 3.20)
project(MyProject)

find_package(scaler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Method 2: Using FetchContent (no installation needed)
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject)

include(FetchContent)

FetchContent_Declare(
    scaler
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG main  # or specific version tag
)

FetchContent_MakeAvailable(scaler)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Method 3: Using add_subdirectory (with local copy)
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject)

# Assuming scaler is in a subdirectory or ../scaler
add_subdirectory(../scaler scaler_build)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)

# ===========================================================================
# Method 4: Using ExternalProject_Add
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(MyProject)

include(ExternalProject)

ExternalProject_Add(
    scaler_external
    GIT_REPOSITORY https://github.com/yourusername/scaler.git
    GIT_TAG main
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/scaler_install
    BUILD_COMMAND ""
    INSTALL_COMMAND cmake --install . --prefix ${CMAKE_BINARY_DIR}/scaler_install
)

# Add the install directory to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/scaler_install)

# Find the installed package
find_package(scaler REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE scaler::scaler)
add_dependencies(my_app scaler_external)