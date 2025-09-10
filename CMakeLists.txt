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
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

option(SCALER_BUILD_TEST "Build unittests for scaler" ON)

# Set visibility settings for shared libraries

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(SCALER_WARNING_FLAGS
            -Wall           # All standard warnings
            -Wextra         # Extra warnings
            -Wpedantic      # Strict ISO C++ compliance
            -Wcast-align    # Warn about potential performance problem casts
            -Wcast-qual     # Warn about casts that remove qualifiers
            -Wconversion    # Warn about type conversions that may lose data
            -Wdouble-promotion  # Warn about float to double promotion
            -Wformat=2      # Additional format string warnings
            -Wnon-virtual-dtor  # Warn about non-virtual destructors
            -Wold-style-cast    # Warn about C-style casts
            -Woverloaded-virtual # Warn about overloaded virtual functions
            -Wshadow        # Warn about variable shadowing
            -Wsign-conversion   # Warn about sign conversions
            -Wundef         # Warn about undefined identifiers in #if
            -Wunused        # Warn about unused entities
            -Wzero-as-null-pointer-constant # Warn about using 0 as nullptr
            -Wstringop-overflow
    )

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        list(APPEND SCALER_WARNING_FLAGS
                -Wlogical-op    # Warn about logical operations being used where bitwise were probably wanted
                -Wuseless-cast  # Warn about useless casts
        )
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(SCALER_WARNING_FLAGS
            /W4             # Highest warning level
            /permissive-    # Strict standard conformance
            /Zc:__cplusplus # Report correct __cplusplus value
    )
endif ()

# Treat warnings as errors in CI/Release builds (optional)
# list(APPEND SCALER_WARNING_FLAGS -Werror)

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
        ${SCALER_PROJECT_ROOT}/include/scaler/2xsai.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/eagle.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/epx.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/hq2x.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/image.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/scaler_common.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/vec3.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/xbr.hh
        # CRTP framework-agnostic interface
        ${SCALER_PROJECT_ROOT}/include/scaler/image_base.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/sdl_image.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/sdl_scalers.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/epx_crtp.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/eagle_crtp.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/2xsai_crtp.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/xbr_crtp.hh
        ${SCALER_PROJECT_ROOT}/include/scaler/hq2x_crtp.hh
)

target_include_directories(scaler INTERFACE ${SCALER_PROJECT_ROOT}/include)

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

if (SCALER_BUILD_TEST)
    add_subdirectory(unittest)
endif()




