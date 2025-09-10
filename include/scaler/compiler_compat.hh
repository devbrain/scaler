#pragma once

// Portable compiler compatibility macros for MSVC, GCC, Clang, and Emscripten

// Compiler detection
#if defined(_MSC_VER)
    #define SCALER_COMPILER_MSVC
#elif defined(__EMSCRIPTEN__)
    #define SCALER_COMPILER_EMSCRIPTEN
    #define SCALER_COMPILER_CLANG  // Emscripten is based on Clang
#elif defined(__clang__)
    #define SCALER_COMPILER_CLANG
#elif defined(__GNUC__)
    #define SCALER_COMPILER_GCC
#endif

// Force inline macro
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_FORCE_INLINE __forceinline
#elif defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define SCALER_FORCE_INLINE inline
#endif

// Hot function attribute (frequently called)
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_HOT __attribute__((hot))
#else
    #define SCALER_HOT
#endif

// Cold function attribute (rarely called)
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_COLD __attribute__((cold))
#else
    #define SCALER_COLD
#endif

// Flatten attribute (aggressive inlining of called functions)
#if defined(SCALER_COMPILER_GCC) || (defined(SCALER_COMPILER_CLANG) && !defined(SCALER_COMPILER_EMSCRIPTEN))
    #define SCALER_FLATTEN __attribute__((flatten))
#else
    #define SCALER_FLATTEN
#endif

// Pure function (no side effects, result depends only on arguments)
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_PURE __attribute__((pure))
#else
    #define SCALER_PURE
#endif

// Const function (pure + doesn't access memory)
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_CONST __attribute__((const))
#else
    #define SCALER_CONST
#endif

// Likely/unlikely branch prediction hints
#if __cplusplus >= 202002L
    // C++20 has standard [[likely]]/[[unlikely]]
    #define SCALER_LIKELY(x) (x) [[likely]]
    #define SCALER_UNLIKELY(x) (x) [[unlikely]]
#elif defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_LIKELY(x) __builtin_expect(!!(x), 1)
    #define SCALER_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define SCALER_LIKELY(x) (x)
    #define SCALER_UNLIKELY(x) (x)
#endif

// Restrict pointer aliasing
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_RESTRICT __restrict
#elif defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_RESTRICT __restrict__
#else
    #define SCALER_RESTRICT
#endif

// Prefetch hint
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)
#else
    #define SCALER_PREFETCH(addr, rw, locality) ((void)0)
#endif

// No-inline directive
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_NOINLINE __declspec(noinline)
#elif defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_NOINLINE __attribute__((noinline))
#else
    #define SCALER_NOINLINE
#endif

// Alignment specification
#if defined(SCALER_COMPILER_MSVC)
    #define SCALER_ALIGN(n) __declspec(align(n))
#elif defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_ALIGN(n) __attribute__((aligned(n)))
#else
    #define SCALER_ALIGN(n)
#endif

// Unused parameter
#define SCALER_UNUSED(x) ((void)(x))

// Assume aligned pointer
#if defined(SCALER_COMPILER_GCC) || defined(SCALER_COMPILER_CLANG)
    #define SCALER_ASSUME_ALIGNED(ptr, align) __builtin_assume_aligned(ptr, align)
#else
    #define SCALER_ASSUME_ALIGNED(ptr, align) (ptr)
#endif