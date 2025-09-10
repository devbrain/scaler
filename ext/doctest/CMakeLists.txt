cmake_minimum_required(VERSION 3.20)

# Try to find doctest or fetch it
find_package(doctest QUIET)

if (NOT doctest_FOUND)
    message(STATUS "doctest not found, fetching from GitHub...")

    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    # Allow FetchContent_Populate to be used
    if (POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif ()

    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")

    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/onqtam/doctest.git
            GIT_TAG v2.4.12
            GIT_PROGRESS TRUE
    )

    # Fetch the content first
    FetchContent_GetProperties(doctest)
    if(NOT doctest_POPULATED)
        FetchContent_Populate(doctest)
        
        # Patch the CMakeLists.txt to fix deprecation warning
        file(READ ${doctest_SOURCE_DIR}/CMakeLists.txt DOCTEST_CMAKE_CONTENT)
        # Replace any version less than 3.10 with 3.10
        string(REGEX REPLACE "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+\\)" 
                             "cmake_minimum_required(VERSION 3.10)" 
                             DOCTEST_CMAKE_CONTENT "${DOCTEST_CMAKE_CONTENT}")
        file(WRITE ${doctest_SOURCE_DIR}/CMakeLists.txt "${DOCTEST_CMAKE_CONTENT}")
        
        # Now add the subdirectory
        add_subdirectory(${doctest_SOURCE_DIR} ${doctest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
endif ()

# Export doctest for use in parent project
if (doctest_FOUND OR TARGET doctest::doctest)
    message(STATUS "doctest available for testing")
endif()