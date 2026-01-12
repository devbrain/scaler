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

# GLEW should be available from parent (via neutrino_fetch_GLEW)
if(NOT TARGET GLEW::GLEW)
    message(FATAL_ERROR "GLEW::GLEW target not found. GLEW is required for gpu_scaler example.")
endif()

# SDL3 should be available from parent (via neutrino_fetch_sdl3)
if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3::SDL3 target not found. Ensure SCALER_WITH_SDL is ON.")
endif()
set(SCALER_SDL_TARGET SDL3::SDL3)

# Create ImGui library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Add SDL3 backend for ImGui
target_sources(imgui_lib PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
target_compile_definitions(imgui_lib PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)

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
neutrino_target_warnings(gpu_scaler)

# Install target
install(TARGETS gpu_scaler
    RUNTIME DESTINATION bin
)