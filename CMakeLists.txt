# =============================================================================
# scaler - High-performance pixel art scaling algorithms
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(scaler
    VERSION 0.1.0
    DESCRIPTION "High-performance pixel art scaling algorithms"
    LANGUAGES CXX C
)

# =============================================================================
# Neutrino CMake Integration
# =============================================================================

include(FetchContent)

if(NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(
        neutrino_cmake
        GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake")
    list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
endif()

include(NeutrinoInit)

# =============================================================================
# Options
# =============================================================================

# Note: scaler is header-only, so we don't use neutrino_define_library_options
neutrino_is_top_level(_scaler_is_top_level)
if(_scaler_is_top_level)
    option(NEUTRINO_SCALER_BUILD_TESTS "Build unit tests" ON)
    option(NEUTRINO_SCALER_BUILD_BENCHMARKS "Build benchmarks" ON)
    option(NEUTRINO_SCALER_BUILD_EXAMPLES "Build example applications" ON)
else()
    option(NEUTRINO_SCALER_BUILD_TESTS "Build unit tests" OFF)
    option(NEUTRINO_SCALER_BUILD_BENCHMARKS "Build benchmarks" OFF)
    option(NEUTRINO_SCALER_BUILD_EXAMPLES "Build example applications" OFF)
endif()

option(NEUTRINO_SCALER_STRICT_WARNINGS "Treat warnings as errors" ON)

# =============================================================================
# C++ Standard
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Output directories
# =============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# Dependencies
# =============================================================================

# Find SDL - prefer SDL3 if both are available
find_package(SDL3 QUIET)
find_package(SDL2 QUIET)

# Find OpenGL for GPU acceleration
find_package(OpenGL QUIET)
find_package(GLEW QUIET)

if(SDL3_FOUND)
    message(STATUS "Found SDL3 - using SDL3 for image operations")
    set(SCALER_USE_SDL3 ON)
    set(SCALER_SDL_TARGET SDL3::SDL3)
    set(SCALER_SDL_VERSION 3)
elseif(SDL2_FOUND)
    message(STATUS "Found SDL2 - using SDL2 for image operations")
    set(SCALER_USE_SDL2 ON)
    set(SCALER_SDL_TARGET SDL2::SDL2)
    set(SCALER_SDL_VERSION 2)
else()
    message(STATUS "No SDL found - SDL image operations will not be available")
    set(SCALER_NO_SDL ON)
endif()

# =============================================================================
# Compiler Warnings
# =============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(SCALER_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wformat=2
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wsign-conversion
        -Wundef
        -Wunused
        -Wzero-as-null-pointer-constant
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        list(APPEND SCALER_WARNING_FLAGS
            -Wlogical-op
            -Wuseless-cast
            -Wstringop-overflow
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND SCALER_WARNING_FLAGS
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-newline-eof
            -Wno-switch-default
            -Wno-switch-enum
            -Wno-missing-prototypes
            -Wno-unsafe-buffer-usage
            -Wno-float-equal
            -Wno-exit-time-destructors
            -Wno-covered-switch-default
            -Wno-implicit-int-float-conversion
            -Wno-double-promotion
            -Wno-unused-template
        )
    endif()

    if(NEUTRINO_SCALER_STRICT_WARNINGS)
        list(APPEND SCALER_WARNING_FLAGS -Werror)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(SCALER_WARNING_FLAGS
        /W4
        /permissive-
        /Zc:__cplusplus
    )
    if(NEUTRINO_SCALER_STRICT_WARNINGS)
        list(APPEND SCALER_WARNING_FLAGS /WX)
    endif()
endif()

# Windows-specific definitions
if(WIN32)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# =============================================================================
# Main Library (Header-only)
# =============================================================================

set(SCALER_PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_library(scaler INTERFACE
    ${SCALER_PROJECT_ROOT}/include/scaler/algorithm.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/algorithm_capabilities.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scaler_common.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/vec3.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/image_base.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_compat.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_image.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_scalers.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/epx.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/eagle.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/2xsai.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/xbr.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/hq2x.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/omniscale.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scale2x_sfx.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scale3x_sfx.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/opengl_utils.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/shader_cache.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/algorithm_traits.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/algorithm_traits_impl.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/shader_source.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/opengl_texture_scaler.hh
    ${SCALER_PROJECT_ROOT}/include/scaler/gpu/sdl/sdl_texture_adapter.hh
)

# Create aliases
add_library(scaler::scaler ALIAS scaler)
add_library(neutrino::scaler ALIAS scaler)

target_include_directories(scaler INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add SDL dependency if available
if(NOT SCALER_NO_SDL)
    target_link_libraries(scaler INTERFACE ${SCALER_SDL_TARGET})
    target_compile_definitions(scaler INTERFACE SCALER_SDL_VERSION=${SCALER_SDL_VERSION})
    if(SCALER_USE_SDL3)
        target_compile_definitions(scaler INTERFACE SCALER_HAS_SDL3)
    elseif(SCALER_USE_SDL2)
        target_compile_definitions(scaler INTERFACE SCALER_HAS_SDL2)
    endif()
endif()

# Add OpenGL and GLEW dependencies if available
if(OpenGL_FOUND)
    target_link_libraries(scaler INTERFACE OpenGL::GL)
    target_compile_definitions(scaler INTERFACE SCALER_HAS_OPENGL)
    if(APPLE)
        target_compile_definitions(scaler INTERFACE GL_SILENCE_DEPRECATION)
    endif()
    if(GLEW_FOUND)
        target_link_libraries(scaler INTERFACE GLEW::GLEW)
        target_compile_definitions(scaler INTERFACE SCALER_HAS_GLEW)
    endif()
endif()

# =============================================================================
# Unit Tests
# =============================================================================

if(NEUTRINO_SCALER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# =============================================================================
# Benchmarks
# =============================================================================

if(NEUTRINO_SCALER_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# =============================================================================
# Examples
# =============================================================================

if(NEUTRINO_SCALER_BUILD_EXAMPLES)
    add_subdirectory(examples/scaler_cli)

    # Build GPU scaler example if OpenGL is available
    if(OpenGL_FOUND AND GLEW_FOUND AND NOT SCALER_NO_SDL)
        add_subdirectory(examples/gpu_scaler)
    endif()
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY include/scaler
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hh"
)

# Install the library target
install(TARGETS scaler
    EXPORT scalerTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install export file
install(EXPORT scalerTargets
    FILE scalerTargets.cmake
    NAMESPACE scaler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scaler
)

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/scalerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Configure the config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/scalerConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/scalerConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scaler
)

# Install the config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/scalerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/scalerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scaler
)

# Export for build tree
export(EXPORT scalerTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/scalerTargets.cmake"
    NAMESPACE scaler::
)

# =============================================================================
# Summary
# =============================================================================

neutrino_is_top_level(_scaler_is_top_level)
if(_scaler_is_top_level)
    message(STATUS "")
    message(STATUS "scaler ${PROJECT_VERSION} Configuration:")
    message(STATUS "  C++ Standard:     17")
    message(STATUS "  Build tests:      ${NEUTRINO_SCALER_BUILD_TESTS}")
    message(STATUS "  Build benchmarks: ${NEUTRINO_SCALER_BUILD_BENCHMARKS}")
    message(STATUS "  Build examples:   ${NEUTRINO_SCALER_BUILD_EXAMPLES}")
    message(STATUS "  Strict warnings:  ${NEUTRINO_SCALER_STRICT_WARNINGS}")
    if(SCALER_SDL_VERSION)
        message(STATUS "  SDL:              ${SCALER_SDL_VERSION}")
    else()
        message(STATUS "  SDL:              Not found")
    endif()
    message(STATUS "  OpenGL:           ${OpenGL_FOUND}")
    message(STATUS "  GLEW:             ${GLEW_FOUND}")
    message(STATUS "")
endif()
