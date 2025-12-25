cmake_minimum_required(VERSION 3.14)

# Fetch stb_image
include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(stb)

# Create the CLI executable
add_executable(scaler_cli
    scaler_cli.cc
    stb_image_wrapper.hh
)

# Include directories
target_include_directories(scaler_cli PRIVATE
    ${stb_SOURCE_DIR}  # For stb headers
    ${CMAKE_SOURCE_DIR}/include  # For scaler headers
)

# Link with the scaler library (CPU only)
target_link_libraries(scaler_cli PRIVATE
    scaler
)

# Set C++ standard
target_compile_features(scaler_cli PRIVATE cxx_std_17)

# Add compile options
neutrino_target_warnings(scaler_cli)

# Install target
install(TARGETS scaler_cli
    RUNTIME DESTINATION bin
)