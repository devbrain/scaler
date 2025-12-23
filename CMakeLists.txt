cmake_minimum_required(VERSION 3.20)
project(scaler
        VERSION 0.1.0
        LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL - prefer SDL3 if both are available
find_package(SDL3 QUIET)
find_package(SDL2 QUIET)

# Find OpenGL for GPU acceleration
find_package(OpenGL)
find_package(GLEW)

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

# Include standard install directories
include(GNUInstallDirs)

# Set default build type if not specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo" "Performance")
endif ()

# Custom Performance build type
set(CMAKE_CXX_FLAGS_PERFORMANCE "-O3 -march=native -mtune=native -DNDEBUG"
    CACHE STRING "Flags used by the C++ compiler during performance builds.")
set(CMAKE_C_FLAGS_PERFORMANCE "-O3 -march=native -mtune=native -DNDEBUG"
    CACHE STRING "Flags used by the C compiler during performance builds.")
set(CMAKE_EXE_LINKER_FLAGS_PERFORMANCE ""
    CACHE STRING "Flags used for linking binaries during performance builds.")
set(CMAKE_SHARED_LINKER_FLAGS_PERFORMANCE ""
    CACHE STRING "Flags used by the shared libraries linker during performance builds.")

mark_as_advanced(
    CMAKE_CXX_FLAGS_PERFORMANCE
    CMAKE_C_FLAGS_PERFORMANCE
    CMAKE_EXE_LINKER_FLAGS_PERFORMANCE
    CMAKE_SHARED_LINKER_FLAGS_PERFORMANCE
)

option(SCALER_BUILD_TEST "Build unittests for scaler" ON)
option(SCALER_BUILD_BENCHMARK "Build benchmarks for scaler" ON)
option(SCALER_BUILD_TOOLS "Build command-line tools" ON)
option(SCALER_STRICT_WARNINGS "Treat warnings as errors" ON)

# Set visibility settings for shared libraries

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(SCALER_WARNING_FLAGS
            -Wall           # All standard warnings
            -Wextra         # Extra warnings
            -Wpedantic      # Strict ISO C++ compliance
            -Wcast-align    # Warn about potential performance problem casts
            -Wcast-qual     # Warn about casts that remove qualifiers
            -Wconversion    # Warn about type conversions that may lose data
            -Wformat=2      # Additional format string warnings
            -Wnon-virtual-dtor  # Warn about non-virtual destructors
            -Wold-style-cast    # Warn about C-style casts
            -Woverloaded-virtual # Warn about overloaded virtual functions
            -Wshadow        # Warn about variable shadowing
            -Wsign-conversion   # Warn about sign conversions
            -Wundef         # Warn about undefined identifiers in #if
            -Wunused        # Warn about unused entities
            -Wzero-as-null-pointer-constant # Warn about using 0 as nullptr
    )

    # GCC-specific flags
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        list(APPEND SCALER_WARNING_FLAGS
                -Wlogical-op    # Warn about logical operations being used where bitwise were probably wanted
                -Wuseless-cast  # Warn about useless casts
                -Wstringop-overflow
        )
    endif ()

    # Clang-specific flags
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND SCALER_WARNING_FLAGS
                -Wno-c++98-compat           # Don't warn about C++11/14/17 features
                -Wno-c++98-compat-pedantic  # Don't warn about C++98 incompatibilities in pedantic mode
                -Wno-newline-eof            # Don't require newline at end of file
                -Wno-switch-default         # Don't require default in switch (enums are exhaustive)
                -Wno-switch-enum            # Don't require all enum values in switch (use default)
                -Wno-missing-prototypes     # Not applicable in C++
                -Wno-unsafe-buffer-usage    # Pointer arithmetic is normal for low-level image code
                -Wno-float-equal            # Float comparisons are intentional in vector math
                -Wno-exit-time-destructors  # Static initialization is normal in C++
                -Wno-covered-switch-default # Default case is defensive programming
                -Wno-implicit-int-float-conversion  # Intentional in graphics code
                -Wno-double-promotion       # Float to double in doctest macros is fine
                -Wno-missing-noreturn       # Functions that throw don't need noreturn attribute
        )
    endif ()

    # Treat warnings as errors
    if (SCALER_STRICT_WARNINGS)
        list(APPEND SCALER_WARNING_FLAGS -Werror)
    endif ()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(SCALER_WARNING_FLAGS
            /W4             # Highest warning level
            /permissive-    # Strict standard conformance
            /Zc:__cplusplus # Report correct __cplusplus value
    )
    # Treat warnings as errors
    if (SCALER_STRICT_WARNINGS)
        list(APPEND SCALER_WARNING_FLAGS /WX)
    endif ()
endif ()

# Windows-specific definitions (applies to both MSVC and Clang-CL)
if (WIN32)
    # Prevent Windows.h from defining min/max macros
    add_compile_definitions(NOMINMAX)
    # Suppress deprecation warnings for standard C functions like getenv
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

# Sanitizer flags
if (SCALER_ENABLE_SANITIZERS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(SCALER_SANITIZER_FLAGS
                -fsanitize=address
                -fsanitize=undefined
                -fno-omit-frame-pointer
        )
        list(APPEND SCALER_WARNING_FLAGS ${SCALER_SANITIZER_FLAGS})

        # Link flags for sanitizers
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined")
    else ()
        message(WARNING "Sanitizers are only supported with GCC and Clang")
    endif ()
endif ()


# Output binary to predictable location.
set(BINARY_OUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(LIB_OUT_DIR ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_OUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${LIB_OUT_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_OUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${LIB_OUT_DIR})
# ===========================================================================
set(SCALER_PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(ext)

# ==========================================================================

add_library(scaler INTERFACE
        # Core utilities
        ${SCALER_PROJECT_ROOT}/include/scaler/algorithm.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/algorithm_capabilities.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scaler_common.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/vec3.hh
        # CRTP framework-agnostic interface
        ${SCALER_PROJECT_ROOT}/include/scaler/image_base.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_compat.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_image.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/sdl/sdl_scalers.hh
        # Scaling algorithms (CRTP-based)
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/epx.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/eagle.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/2xsai.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/xbr.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/hq2x.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/omniscale.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scale2x_sfx.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/cpu/scale3x_sfx.hh
        # GPU utilities (new)
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/opengl_utils.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/shader_cache.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/algorithm_traits.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/algorithm_traits_impl.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/shader_source.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/opengl_texture_scaler.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/gpu/sdl/sdl_texture_adapter.hh
)

# Create alias for consistent usage
add_library(scaler::scaler ALIAS scaler)

target_include_directories(scaler INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
    # Silence OpenGL deprecation warnings on macOS
    if(APPLE)
        target_compile_definitions(scaler INTERFACE GL_SILENCE_DEPRECATION)
    endif()
    if(GLEW_FOUND)
        target_link_libraries(scaler INTERFACE GLEW::GLEW)
        target_compile_definitions(scaler INTERFACE SCALER_HAS_GLEW)
    endif()
endif()

if (SCALER_BUILD_TEST)
    enable_testing()
    add_subdirectory(unittest)
endif()

if (SCALER_BUILD_BENCHMARK)
    add_subdirectory(benchmark)
endif()

# ===========================================================================
# Installation and Packaging
# ===========================================================================

# Install headers
install(DIRECTORY include/scaler
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hh"
)

# Install the library target
install(TARGETS scaler
    EXPORT scalerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install export file
install(EXPORT scalerTargets
    FILE scalerTargets.cmake
    NAMESPACE scaler::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scaler
)

# Generate the config file that includes the exports
include(CMakePackageConfigHelpers)

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

# Register package in user's package registry
export(PACKAGE scaler)

# ===========================================================================
# CPack Configuration
# ===========================================================================

set(CPACK_PACKAGE_NAME "scaler")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Scaler Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance pixel art scaling algorithms")
set(CPACK_PACKAGE_DESCRIPTION "A header-only C++ library providing efficient implementations of popular pixel art scaling algorithms")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Generator-specific settings
set(CPACK_GENERATOR "TGZ;ZIP")
if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl3-dev | libsdl2-dev")
endif()

if(WIN32)
    list(APPEND CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Scaler Library")
    set(CPACK_NSIS_PACKAGE_NAME "Scaler")
endif()

# Source package settings
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    /.git/
    /build/
    /cmake-build-.*/
    /.idea/
    /.vscode/
    /\\\\.DS_Store
    /.*\\\\.swp
)

include(CPack)

# Build examples
option(BUILD_EXAMPLES "Build example applications" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples/scaler_cli)

    # Build GPU scaler example if OpenGL is available
    if(OpenGL_FOUND AND GLEW_FOUND AND NOT SCALER_NO_SDL)
        add_subdirectory(examples/gpu_scaler)
    endif()
endif()




