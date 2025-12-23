#pragma once

#include <scaler/algorithm.hh>
#include <vector>
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;
union SDL_Event;

namespace gpu_scaler_app {

class App;

/**
 * GUI management class using ImGui
 * Provides controls for algorithm selection and scaling parameters
 */
class GUI {
public:
    GUI(App* app, SDL_Window* window, SDL_GLContext gl_context);
    ~GUI();

    // Initialize ImGui
    bool init();

    // Cleanup ImGui
    void shutdown();

    // Process SDL event
    static void process_event(const SDL_Event* event);

    // Begin new frame
    static void new_frame();

    // Render GUI
    static void render();

    // Draw GUI windows
    void draw();

    // Get the currently selected algorithm
    [[nodiscard]] scaler::algorithm get_selected_algorithm() const {
        if (selected_algorithm_idx_ >= 0 && static_cast<size_t>(selected_algorithm_idx_) < gpu_algorithms_.size()) {
            return gpu_algorithms_[static_cast<size_t>(selected_algorithm_idx_)];
        }
        return scaler::algorithm::Nearest;
    }

private:
    // Draw control panel
    void draw_control_panel();

    // Draw image viewer
    void draw_image_viewer();

    // Draw performance stats
    void draw_stats();

    // Get list of available GPU algorithms
    [[nodiscard]] static std::vector<scaler::algorithm> get_gpu_algorithms() ;

    // Get algorithm display name
    [[nodiscard]] static std::string get_algorithm_name(scaler::algorithm algo) ;

    // Get supported scales for algorithm
    [[nodiscard]] static std::vector<float> get_supported_scales(scaler::algorithm algo) ;

private:
    App* app_;
    SDL_Window* window_;
    SDL_GLContext gl_context_;

    // GUI state
    int selected_algorithm_idx_ = 0;
    float selected_scale_ = 2.0f;
    bool show_original_ = false;
    bool show_stats_ = true;
    bool vsync_enabled_ = true;

    // Available algorithms (GPU-capable only)
    std::vector<scaler::algorithm> gpu_algorithms_;

    // Reserved for future texture display functionality
    // unsigned int display_texture_ = 0;
    // int display_width_ = 0;
    // int display_height_ = 0;

    // Shutdown state
    bool is_shutdown_ = false;
};

} // namespace gpu_scaler_app