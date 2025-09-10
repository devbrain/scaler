include(FetchContent)

set(FETCHCONTENT_QUIET off)

# Save current warning flags
set(SAVED_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(SAVED_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})

# Disable warnings for third-party libraries
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W0")
endif()

# Also set properties to suppress warnings in a more modern way
add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
)

# Add doctest for testing
if(SCALER_BUILD_TEST)
    add_subdirectory(doctest)
endif()




