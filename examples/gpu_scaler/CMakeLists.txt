cmake_minimum_required(VERSION 3.14)
project(gpu_scaler_example)

# Fetch ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Check for SDL
if(NOT DEFINED SCALER_SDL_TARGET)
    # If not defined from parent, find SDL ourselves
    find_package(SDL3 QUIET)
    if(SDL3_FOUND)
        set(SCALER_SDL_TARGET SDL3::SDL3)
        set(SCALER_SDL_VERSION 3)
    else()
        find_package(SDL2 REQUIRED)
        set(SCALER_SDL_TARGET SDL2::SDL2)
        set(SCALER_SDL_VERSION 2)
    endif()
endif()

# Create ImGui library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Add SDL backend based on version
if(SCALER_SDL_VERSION EQUAL 3)
    target_sources(imgui_lib PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    )
    target_compile_definitions(imgui_lib PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_GLEW
        SCALER_SDL_VERSION=3
    )
else()
    target_sources(imgui_lib PRIVATE
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    )
    target_compile_definitions(imgui_lib PUBLIC
        IMGUI_IMPL_OPENGL_LOADER_GLEW
        SCALER_SDL_VERSION=2
    )
endif()

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC
    ${SCALER_SDL_TARGET}
    OpenGL::GL
    GLEW::GLEW
)

# Create the GPU scaler executable
add_executable(gpu_scaler
    main.cc
    app.cc
    app.hh
    gui.cc
    gui.hh
    image_loader.cc
    image_loader.hh
    rotozoom_bmp.h
)

# Include directories
target_include_directories(gpu_scaler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(gpu_scaler PRIVATE
    scaler::scaler
    imgui_lib
    ${SCALER_SDL_TARGET}
    OpenGL::GL
    GLEW::GLEW
)

# Set C++ standard
target_compile_features(gpu_scaler PRIVATE cxx_std_17)

# Add compile options
target_compile_options(gpu_scaler PRIVATE ${SCALER_WARNING_FLAGS})

# Install target
install(TARGETS gpu_scaler
    RUNTIME DESTINATION bin
)