# Benchmark executable
add_executable(benchmark_scalers
    benchmark_scalers.cc
)

target_link_libraries(benchmark_scalers
    PRIVATE
    scaler
)

target_compile_options(benchmark_scalers PRIVATE ${SCALER_WARNING_FLAGS})

# Profiling build options
option(SCALER_ENABLE_PROFILING "Enable profiling with gprof" OFF)
option(SCALER_ENABLE_VALGRIND "Enable valgrind-friendly build" OFF)

# Gprof profiling
if(SCALER_ENABLE_PROFILING)
    message(STATUS "Profiling enabled (gprof)")
    target_compile_options(benchmark_scalers PRIVATE -pg -g)
    target_link_options(benchmark_scalers PRIVATE -pg)
endif()

# Valgrind-friendly build
if(SCALER_ENABLE_VALGRIND)
    message(STATUS "Valgrind-friendly build enabled")
    target_compile_options(benchmark_scalers PRIVATE -g -O1)
    target_compile_definitions(benchmark_scalers PRIVATE VALGRIND_BUILD)
endif()

# Performance build configuration
if(CMAKE_BUILD_TYPE STREQUAL "Performance")
    message(STATUS "Performance build configuration")
    target_compile_options(benchmark_scalers PRIVATE
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -finline-functions
        -fomit-frame-pointer
    )
    
    # Link-time optimization
    set_property(TARGET benchmark_scalers PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Add custom targets for profiling
add_custom_target(profile-gprof
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers -q
    COMMAND gprof ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers gmon.out > gprof_report.txt
    COMMAND echo "Profiling report saved to gprof_report.txt"
    DEPENDS benchmark_scalers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running gprof profiling"
)

add_custom_target(profile-valgrind-cache
    COMMAND valgrind --tool=cachegrind --cachegrind-out-file=cachegrind.out 
            ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers -q
    COMMAND echo "Cache profiling complete. Run 'cg_annotate cachegrind.out' to view results"
    DEPENDS benchmark_scalers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running valgrind cachegrind"
)

add_custom_target(profile-valgrind-callgrind
    COMMAND valgrind --tool=callgrind --callgrind-out-file=callgrind.out 
            ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers -q
    COMMAND echo "Call profiling complete. Use KCachegrind or 'callgrind_annotate callgrind.out' to view"
    DEPENDS benchmark_scalers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running valgrind callgrind"
)

add_custom_target(profile-valgrind-memory
    COMMAND valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all 
            --track-origins=yes --verbose --log-file=memcheck.log
            ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers -q
    COMMAND echo "Memory profiling complete. Check memcheck.log for results"
    DEPENDS benchmark_scalers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running valgrind memcheck"
)

add_custom_target(profile-valgrind-massif
    COMMAND valgrind --tool=massif --massif-out-file=massif.out 
            ${CMAKE_CURRENT_BINARY_DIR}/benchmark_scalers -q
    COMMAND ms_print massif.out > massif_report.txt
    COMMAND echo "Heap profiling complete. Check massif_report.txt"
    DEPENDS benchmark_scalers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running valgrind massif (heap profiler)"
)