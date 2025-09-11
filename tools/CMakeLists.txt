cmake_minimum_required(VERSION 3.14)

# Fetch stb_image headers
include(FetchContent)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

message(STATUS "Fetching stb headers...")
FetchContent_MakeAvailable(stb)

# Create stb interface library
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# Create the CLI tool executable
add_executable(scaler_cli scaler_cli.cc)

# Set C++17 standard for filesystem support
target_compile_features(scaler_cli PRIVATE cxx_std_17)

# Link dependencies
target_link_libraries(scaler_cli
    PRIVATE
    scaler
    stb
)

# Add filesystem library for older compilers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(scaler_cli PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(scaler_cli PRIVATE c++fs)
endif()

# Set output directory
set_target_properties(scaler_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation rules (optional)
install(TARGETS scaler_cli
    RUNTIME DESTINATION bin
)