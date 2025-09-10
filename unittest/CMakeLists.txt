
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
        # Original vs CRTP comparison tests
        test_original_vs_crtp.cc
        # Golden data tests
        test_golden_data.cc
)

# Include directories

    # Use internal static library for testing internals
    target_link_libraries(scaler_unittest
            PRIVATE
            doctest::doctest
            scaler
    )

# Golden data generator executable
add_executable(generate_golden_data
    generate_golden_data.cc
)

target_link_libraries(generate_golden_data
    PRIVATE
    scaler
)
