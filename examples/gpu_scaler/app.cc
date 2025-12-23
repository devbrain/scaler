#include "app.hh"
#include "gui.hh"
#include "image_loader.hh"

#include <scaler/sdl/sdl_compat.hh>
#include <scaler/gpu/opengl_texture_scaler.hh>
#include <scaler/algorithm_capabilities.hh>

#include <GL/glew.h>
#include <iostream>

namespace gpu_scaler_app {

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init(int window_width, int window_height) {
    window_width_ = window_width;
    window_height_ = window_height;

    // Initialize SDL
    if (!init_sdl(window_width, window_height)) {
        return false;
    }

    // Initialize OpenGL
    if (!init_opengl()) {
        return false;
    }

    // Create GUI
    gui_ = std::make_unique<GUI>(this, window_, gl_context_);
    if (!gui_->init()) {
        std::cerr << "Failed to initialize GUI\n";
        return false;
    }

    // Sync the initial algorithm with GUI's selection
    current_algorithm_ = gui_->get_selected_algorithm();

    // Create image loader and load embedded image
    image_loader_ = std::make_unique<ImageLoader>();
    if (!image_loader_->load_embedded_image()) {
        std::cerr << "Failed to load embedded image\n";
        return false;
    }

    // Create texture from loaded image
    original_texture_ = image_loader_->create_texture();
    if (original_texture_ == 0) {
        std::cerr << "Failed to create texture from image\n";
        return false;
    }

    // Initial update
    update_scaled_texture();

    return true;
}

bool App::init_sdl(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
    window_ = SDL_CreateWindow(
        "GPU Scaler Demo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Create OpenGL context
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Enable VSync
    SDL_GL_SetSwapInterval(1);

    return true;
}

bool App::init_opengl() const {
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glew_error = glewInit();
    if (glew_error != GLEW_OK) {
        std::cerr << "GLEW init failed: " << glewGetErrorString(glew_error) << "\n";
        return false;
    }

    // Clear any GL errors from GLEW init
    while (glGetError() != GL_NO_ERROR) {}

    // Set up OpenGL state
    glViewport(0, 0, window_width_, window_height_);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    return true;
}

void App::run() {
    while (running_) {
        process_events();

        // Start ImGui frame
        gpu_scaler_app::GUI::new_frame();

        // Update scaled texture if needed
        if (needs_update_) {
            update_scaled_texture();
            needs_update_ = false;
        }

        // Clear
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw GUI
        gui_->draw();

        // Render ImGui
        gpu_scaler_app::GUI::render();

        // Swap buffers
        SDL_GL_SwapWindow(window_);
    }
}

void App::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let ImGui process events
        gpu_scaler_app::GUI::process_event(&event);

        // Handle quit
        if (event.type == SDL_QUIT) {
            running_ = false;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_width_ = event.window.data1;
                window_height_ = event.window.data2;
                glViewport(0, 0, window_width_, window_height_);
            }
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running_ = false;
            }
        }
    }
}

void App::shutdown() {
    // Guard against multiple shutdowns
    if (shutdown_called_) {
        return;
    }
    shutdown_called_ = true;

    // Delete textures
    if (original_texture_ != 0) {
        glDeleteTextures(1, &original_texture_);
        original_texture_ = 0;
    }
    if (scaled_texture_ != 0) {
        glDeleteTextures(1, &scaled_texture_);
        scaled_texture_ = 0;
    }

    // Shutdown GUI first (before destroying GL context)
    if (gui_) {
        gui_->shutdown();
        gui_.reset();
    }

    // Reset image loader
    image_loader_.reset();

    // Cleanup SDL
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::set_algorithm(scaler::algorithm algo) {
    if (current_algorithm_ != algo) {
        current_algorithm_ = algo;
        needs_update_ = true;
    }
}

void App::set_scale(float scale) {
    if (current_scale_ != scale) {
        current_scale_ = scale;
        needs_update_ = true;
    }
}

void App::update_scaled_texture() {
    if (!image_loader_ || !image_loader_->has_image()) {
        return;
    }

    // Delete old scaled texture
    if (scaled_texture_ != 0) {
        glDeleteTextures(1, &scaled_texture_);
        scaled_texture_ = 0;
    }

    // Check if algorithm supports GPU scaling
    const auto& info = scaler::algorithm_capabilities::get_info(current_algorithm_);
    if (!info.gpu_accelerated) {
        std::cerr << "Algorithm does not support GPU scaling\n";
        return;
    }

    // Create texture scaler
    scaler::gpu::opengl_texture_scaler scaler;

    // Calculate output dimensions
    const int src_width = image_loader_->get_width();
    const int src_height = image_loader_->get_height();
    const int dst_width = static_cast<int>(static_cast<float>(src_width) * current_scale_);
    const int dst_height = static_cast<int>(static_cast<float>(src_height) * current_scale_);

    // Create output texture
    glGenTextures(1, &scaled_texture_);
    glBindTexture(GL_TEXTURE_2D, scaled_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, dst_width, dst_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Perform scaling
    try {
        const auto& algo_info = scaler::algorithm_capabilities::get_info(current_algorithm_);
        std::cout << "Scaling with " << algo_info.name
                  << " (id=" << static_cast<int>(current_algorithm_) << ")"
                  << " from " << src_width << "x" << src_height
                  << " to " << dst_width << "x" << dst_height << "\n";

        scaler.scale_texture_to_texture(original_texture_,
                                        src_width, src_height,
                                        scaled_texture_,
                                        dst_width, dst_height,
                                        current_algorithm_);

        std::cout << "Scaling successful\n";
    } catch (const std::exception& e) {
        std::cerr << "Scaling failed: " << e.what() << "\n";
        glDeleteTextures(1, &scaled_texture_);
        scaled_texture_ = 0;
    }
}

void App::render() {
    // The actual rendering is done by ImGui in the GUI class
    // This method is here for potential future use
}

} // namespace gpu_scaler_app