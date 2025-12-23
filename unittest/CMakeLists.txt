
# Create test executable
add_executable(scaler_unittest
        # Main test runner
        test_main.cc
        # Unified algorithm tests (replaces multiple old test files)
        test_unified_algorithms.cc
        # Unified scaler interface tests
        test_unified_scaler.cc
        # Preallocated output tests
        test_unified_preallocated.cc
        # SDL interface tests
        test_sdl_interface.cc
        # Golden data regression tests
        test_golden_data.cc
        test_omniscale_golden.cc
        test_hq3x_golden.cc
        test_hq3x_exact_golden.cc
        # Implementation-specific tests
        test_sliding_window_buffer.cc
        # Interpolation algorithm tests
        test_bilinear_trilinear.cc
)

# Add GPU tests if OpenGL is available
if(OpenGL_FOUND)
    target_sources(scaler_unittest PRIVATE
        test_platform_config.cc
        test_unified_gpu.cc
        test_unified_cpu_gpu.cc
    )
endif()

# Include directories

# Use internal static library for testing internals
target_link_libraries(scaler_unittest
        PRIVATE
        doctest::doctest
        scaler
)

# Link OpenGL if available
if(OpenGL_FOUND)
    target_link_libraries(scaler_unittest PRIVATE OpenGL::GL)

    # Link GLEW on non-Apple platforms
    if(NOT APPLE)
        find_package(GLEW REQUIRED)
        target_link_libraries(scaler_unittest PRIVATE GLEW::GLEW)
    endif()
endif()

target_compile_options(scaler_unittest PRIVATE ${SCALER_WARNING_FLAGS})

# Register test with CTest
add_test(NAME scaler_unittest COMMAND scaler_unittest)

# Golden data generator executable
add_executable(generate_golden_data
        generate_golden_data.cc
)

target_link_libraries(generate_golden_data
        PRIVATE
        scaler
)

target_compile_options(generate_golden_data PRIVATE ${SCALER_WARNING_FLAGS})
