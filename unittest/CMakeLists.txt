
# Create test executable
add_executable(scaler_unittest
        # Main test runner
        test_main.cc
        # CRTP interface tests
        test_crtp.cc
        # Comprehensive scaler tests
        test_scalers_comprehensive.cc
        # Algorithm correctness tests
        test_algorithm_correctness.cc
        # XBR and HQ2x tests
        test_xbr_hq2x.cc
        # SDL interface tests
        test_sdl_interface.cc
        # Golden data tests
        test_golden_data.cc
        # Sliding window buffer tests
        test_sliding_window_buffer.cc
        # Scale algorithm tests
        test_scale_algorithms.cc
        # OmniScale golden data tests
        test_omniscale_golden.cc
        # HQ3x golden data tests
        test_hq3x_golden.cc
        # HQ3x exact implementation golden data tests
        test_hq3x_exact_golden.cc
        # Unified scaler interface tests
        test_unified_scaler.cc
        # Bilinear and trilinear tests
        test_bilinear_trilinear.cc
)

# Add GPU tests if OpenGL is available
if(OpenGL_FOUND)
    target_sources(scaler_unittest PRIVATE
        test_gpu_simple.cc
        test_gpu_epx.cc
        test_gpu_algorithms_fidelity.cc
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

# Golden data generator executable
add_executable(generate_golden_data
        generate_golden_data.cc
)

target_link_libraries(generate_golden_data
        PRIVATE
        scaler
)
