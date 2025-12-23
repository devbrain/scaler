#pragma once

#include <cstddef>  // For size_t, ptrdiff_t

/**
 * Portable warning suppression macros for GCC, Clang, and MSVC
 *
 * Usage:
 *   SCALER_DISABLE_WARNING_PUSH
 *   SCALER_DISABLE_WARNING_FLOAT_EQUAL
 *   // ... code with float comparisons ...
 *   SCALER_DISABLE_WARNING_POP
 */

// Compiler detection
#if defined(_MSC_VER)
    #define SCALER_COMPILER_MSVC
#elif defined(__clang__)
    #define SCALER_COMPILER_CLANG
#elif defined(__GNUC__)
    #define SCALER_COMPILER_GCC
#endif

// Warning push/pop macros
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_DISABLE_WARNING_PUSH __pragma(warning(push))
    #define SCALER_DISABLE_WARNING_POP __pragma(warning(pop))
    #define SCALER_DISABLE_WARNING(warningNumber) __pragma(warning(disable: warningNumber))

    // MSVC specific warning numbers
    #define SCALER_DISABLE_WARNING_FLOAT_EQUAL SCALER_DISABLE_WARNING(4018)
    #define SCALER_DISABLE_WARNING_SIGN_CONVERSION
    #define SCALER_DISABLE_WARNING_OLD_STYLE_CAST
    #define SCALER_DISABLE_WARNING_UNUSED_PARAMETER SCALER_DISABLE_WARNING(4100)
    #define SCALER_DISABLE_WARNING_CONVERSION SCALER_DISABLE_WARNING(4244)

#elif defined(SCALER_COMPILER_CLANG)
    #define SCALER_DISABLE_WARNING_PUSH _Pragma("clang diagnostic push")
    #define SCALER_DISABLE_WARNING_POP _Pragma("clang diagnostic pop")
    #define SCALER_DISABLE_WARNING_FLOAT_EQUAL _Pragma("clang diagnostic ignored \"-Wfloat-equal\"")
    #define SCALER_DISABLE_WARNING_SIGN_CONVERSION _Pragma("clang diagnostic ignored \"-Wsign-conversion\"")
    #define SCALER_DISABLE_WARNING_OLD_STYLE_CAST _Pragma("clang diagnostic ignored \"-Wold-style-cast\"")
    #define SCALER_DISABLE_WARNING_UNUSED_PARAMETER _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")
    #define SCALER_DISABLE_WARNING_CONVERSION _Pragma("clang diagnostic ignored \"-Wconversion\"")

#elif defined(SCALER_COMPILER_GCC)
    #define SCALER_DISABLE_WARNING_PUSH _Pragma("GCC diagnostic push")
    #define SCALER_DISABLE_WARNING_POP _Pragma("GCC diagnostic pop")
    #define SCALER_DISABLE_WARNING_FLOAT_EQUAL _Pragma("GCC diagnostic ignored \"-Wfloat-equal\"")
    #define SCALER_DISABLE_WARNING_SIGN_CONVERSION _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
    #define SCALER_DISABLE_WARNING_OLD_STYLE_CAST _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")
    #define SCALER_DISABLE_WARNING_UNUSED_PARAMETER _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
    #define SCALER_DISABLE_WARNING_CONVERSION _Pragma("GCC diagnostic ignored \"-Wconversion\"")

#else
    // Unknown compiler - define as no-ops
    #define SCALER_DISABLE_WARNING_PUSH
    #define SCALER_DISABLE_WARNING_POP
    #define SCALER_DISABLE_WARNING_FLOAT_EQUAL
    #define SCALER_DISABLE_WARNING_SIGN_CONVERSION
    #define SCALER_DISABLE_WARNING_OLD_STYLE_CAST
    #define SCALER_DISABLE_WARNING_UNUSED_PARAMETER
    #define SCALER_DISABLE_WARNING_CONVERSION
#endif

/**
 * Helper macro for float comparisons that we've verified are safe
 * These are comparing exact constants, not computed values
 */
#define SCALER_SAFE_FLOAT_COMPARE(a, op, b) \
    SCALER_DISABLE_WARNING_PUSH \
    SCALER_DISABLE_WARNING_FLOAT_EQUAL \
    ((a) op (b)) \
    SCALER_DISABLE_WARNING_POP

/**
 * Macro for checking exact scale factors
 * We know these are always passed as exact values (2.0f, 3.0f, 4.0f)
 */
#define SCALER_IS_EXACT_SCALE(scale, value) \
    SCALER_DISABLE_WARNING_PUSH \
    SCALER_DISABLE_WARNING_FLOAT_EQUAL \
    ((scale) == (value)) \
    SCALER_DISABLE_WARNING_POP

/**
 * OpenGL conversion helpers
 * These macros safely convert between standard types and OpenGL types,
 * suppressing warnings about sign and size conversions that are inherent
 * to the OpenGL C API design.
 */

// Convert size_t to GLsizei (OpenGL uses signed int for sizes)
#define SCALER_SIZE_TO_GLSIZEI(size) \
    (static_cast<GLsizei>(size))

// Convert GLsizei to size_t (when reading from OpenGL)
#define SCALER_GLSIZEI_TO_SIZE(glsize) \
    (static_cast<size_t>(glsize))

// Convert GLint to GLuint (for framebuffer binding)
#define SCALER_GLINT_TO_GLUINT(glint) \
    (static_cast<GLuint>(glint))

// Convert GLint to size_t (for buffer sizes)
#define SCALER_GLINT_TO_SIZE(glint) \
    (static_cast<size_t>(glint))

// Convert coord_t to int (for modulo operations)
#define SCALER_COORD_TO_INT(coord) \
    (static_cast<int>(coord))

// Convert size_t to float (for graphics calculations)
#define SCALER_SIZE_TO_FLOAT(size) \
    (static_cast<float>(size))

/**
 * Disable all warnings for third-party headers
 * Use this to wrap includes of external libraries we don't control
 */
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_DISABLE_ALL_WARNINGS_PUSH \
        __pragma(warning(push, 0))
    #define SCALER_DISABLE_ALL_WARNINGS_POP \
        __pragma(warning(pop))
#elif defined(SCALER_COMPILER_CLANG)
    #define SCALER_DISABLE_ALL_WARNINGS_PUSH \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Weverything\"")
    #define SCALER_DISABLE_ALL_WARNINGS_POP \
        _Pragma("clang diagnostic pop")
#elif defined(SCALER_COMPILER_GCC)
    #define SCALER_DISABLE_ALL_WARNINGS_PUSH \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wall\"") \
        _Pragma("GCC diagnostic ignored \"-Wextra\"") \
        _Pragma("GCC diagnostic ignored \"-Wpedantic\"") \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"") \
        _Pragma("GCC diagnostic ignored \"-Wcast-qual\"") \
        _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"") \
        _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"") \
        _Pragma("GCC diagnostic ignored \"-Wzero-as-null-pointer-constant\"")
    #define SCALER_DISABLE_ALL_WARNINGS_POP \
        _Pragma("GCC diagnostic pop")
#else
    #define SCALER_DISABLE_ALL_WARNINGS_PUSH
    #define SCALER_DISABLE_ALL_WARNINGS_POP
#endif
