#pragma once

#include <memory>
#include <string>
#include <scaler/algorithm.hh>

// Forward declarations
struct SDL_Window;
typedef void* SDL_GLContext;

namespace gpu_scaler_app {

class GUI;
class ImageLoader;

/**
 * Main application class for the GPU scaler demo
 * Manages the SDL window, OpenGL context, and coordinates between components
 */
class App {
public:
    App();
    ~App();

    // Initialize the application
    bool init(int window_width = 1280, int window_height = 720);

    // Run the main application loop
    void run();

    // Cleanup resources
    void shutdown();

    // Get current scaling parameters
    [[nodiscard]] scaler::algorithm get_current_algorithm() const { return current_algorithm_; }
    [[nodiscard]] float get_current_scale() const { return current_scale_; }

    // Set scaling parameters (called from GUI)
    void set_algorithm(scaler::algorithm algo);
    void set_scale(float scale);

    // Get the image loader
    ImageLoader* get_image_loader() const { return image_loader_.get(); }

    // Get texture IDs for display
    [[nodiscard]] unsigned int get_original_texture() const { return original_texture_; }
    [[nodiscard]] unsigned int get_scaled_texture() const { return scaled_texture_; }

private:
    // Initialize SDL and create window
    bool init_sdl(int width, int height);

    // Initialize OpenGL
    bool init_opengl() const;

    // Process SDL events
    void process_events();

    // Render frame
    void render();

    // Update scaled texture when parameters change
    void update_scaled_texture();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;

    std::unique_ptr<GUI> gui_;
    std::unique_ptr<ImageLoader> image_loader_;

    scaler::algorithm current_algorithm_ = scaler::algorithm::Nearest;
    float current_scale_ = 2.0f;
    bool needs_update_ = true;
    bool running_ = true;

    // Texture IDs
    unsigned int original_texture_ = 0;
    unsigned int scaled_texture_ = 0;

    // Dimensions
    int window_width_ = 0;
    int window_height_ = 0;

    // Shutdown state
    bool shutdown_called_ = false;
};

} // namespace gpu_scaler_app