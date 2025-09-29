#include "gui.hh"
#include "app.hh"
#include "image_loader.hh"

#include <imgui.h>
#if SCALER_SDL_VERSION == 3
#include <imgui_impl_sdl3.h>
#else
#include <imgui_impl_sdl2.h>
#endif
#include <imgui_impl_opengl3.h>

#include <scaler/sdl/sdl_compat.hh>
#include <scaler/algorithm_capabilities.hh>

#include <GL/glew.h>
#include <algorithm>
#include <sstream>

namespace gpu_scaler_app {

GUI::GUI(App* app, SDL_Window* window, SDL_GLContext gl_context)
    : app_(app)
    , window_(window)
    , gl_context_(gl_context) {
    // Get list of GPU-capable algorithms
    gpu_algorithms_ = get_gpu_algorithms();
}

GUI::~GUI() {
    shutdown();
}

bool GUI::init() {
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
#if SCALER_SDL_VERSION == 3
    ImGui_ImplSDL3_InitForOpenGL(window_, gl_context_);
#else
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
#endif
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void GUI::shutdown() {
    if (is_shutdown_) {
        return;
    }
    is_shutdown_ = true;

    ImGui_ImplOpenGL3_Shutdown();
#if SCALER_SDL_VERSION == 3
    ImGui_ImplSDL3_Shutdown();
#else
    ImGui_ImplSDL2_Shutdown();
#endif
    ImGui::DestroyContext();
}

void GUI::process_event(const SDL_Event* event) {
#if SCALER_SDL_VERSION == 3
    ImGui_ImplSDL3_ProcessEvent(event);
#else
    ImGui_ImplSDL2_ProcessEvent(event);
#endif
}

void GUI::new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
#if SCALER_SDL_VERSION == 3
    ImGui_ImplSDL3_NewFrame();
#else
    ImGui_ImplSDL2_NewFrame();
#endif
    ImGui::NewFrame();
}

void GUI::render() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::draw() {
    draw_control_panel();
    draw_image_viewer();
    if (show_stats_) {
        draw_stats();
    }
}

void GUI::draw_control_panel() {
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // Algorithm selection
    ImGui::Text("Scaling Algorithm:");
    ImGui::Separator();

    if (ImGui::BeginCombo("Algorithm", get_algorithm_name(gpu_algorithms_[selected_algorithm_idx_]).c_str())) {
        for (size_t i = 0; i < gpu_algorithms_.size(); ++i) {
            bool is_selected = (selected_algorithm_idx_ == static_cast<int>(i));
            if (ImGui::Selectable(get_algorithm_name(gpu_algorithms_[i]).c_str(), is_selected)) {
                selected_algorithm_idx_ = static_cast<int>(i);
                app_->set_algorithm(gpu_algorithms_[i]);

                // Update available scales for new algorithm
                auto scales = get_supported_scales(gpu_algorithms_[i]);
                if (!scales.empty()) {
                    // Find closest scale to current
                    auto it = std::min_element(scales.begin(), scales.end(),
                        [this](float a, float b) {
                            return std::abs(a - selected_scale_) < std::abs(b - selected_scale_);
                        });
                    selected_scale_ = *it;
                    app_->set_scale(selected_scale_);
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Scale selection
    ImGui::Spacing();
    ImGui::Text("Scale Factor:");
    ImGui::Separator();

    auto scales = get_supported_scales(gpu_algorithms_[selected_algorithm_idx_]);

    // Show radio buttons for fixed scales
    if (!scales.empty()) {
        for (float scale : scales) {
            char label[32];
            std::snprintf(label, sizeof(label), "%.1fx", scale);
            if (ImGui::RadioButton(label, selected_scale_ == scale)) {
                selected_scale_ = scale;
                app_->set_scale(scale);
            }
            if (scale != scales.back()) {
                ImGui::SameLine();
            }
        }
    }

    // Display options
    ImGui::Spacing();
    ImGui::Text("Display Options:");
    ImGui::Separator();

    ImGui::Checkbox("Show Original", &show_original_);
    ImGui::Checkbox("Show Stats", &show_stats_);

    if (ImGui::Checkbox("VSync", &vsync_enabled_)) {
        SDL_GL_SetSwapInterval(vsync_enabled_ ? 1 : 0);
    }

    // Image info
    ImGui::Spacing();
    ImGui::Text("Image Info:");
    ImGui::Separator();

    auto* loader = app_->get_image_loader();
    if (loader) {
        ImGui::Text("Original: %dx%d", loader->get_width(), loader->get_height());
        ImGui::Text("Scaled: %dx%d",
                    static_cast<int>(loader->get_width() * selected_scale_),
                    static_cast<int>(loader->get_height() * selected_scale_));
    }

    ImGui::End();
}

void GUI::draw_image_viewer() {
    // Make the image viewer window larger and resizable
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Image Viewer", nullptr, ImGuiWindowFlags_NoCollapse);

    // Get the texture to display
    GLuint tex_id = 0;
    int tex_width = 0;
    int tex_height = 0;

    if (show_original_) {
        // Show original texture
        tex_id = app_->get_original_texture();
        if (tex_id != 0) {
            auto* loader = app_->get_image_loader();
            if (loader) {
                tex_width = loader->get_width();
                tex_height = loader->get_height();
            }
        }
    } else {
        // Show scaled texture
        tex_id = app_->get_scaled_texture();
        if (tex_id != 0) {
            auto* loader = app_->get_image_loader();
            if (loader) {
                tex_width = static_cast<int>(loader->get_width() * app_->get_current_scale());
                tex_height = static_cast<int>(loader->get_height() * app_->get_current_scale());
            }
        }
    }

    if (tex_id != 0 && tex_width > 0 && tex_height > 0) {
        // Debug: Show actual texture size
        ImGui::Text("Texture: %dx%d", tex_width, tex_height);

        // Create a child window with scrollbars for the image
        ImGui::BeginChild("ImageScrollRegion", ImVec2(0, 0), false,
                         ImGuiWindowFlags_HorizontalScrollbar);

        // Display at 1:1 pixel ratio (actual size)
        ImVec2 size(static_cast<float>(tex_width), static_cast<float>(tex_height));

        // Get the screen position where we're about to draw the image
        ImVec2 image_screen_pos = ImGui::GetCursorScreenPos();

        // Display the texture - flip original but not scaled (GPU scaler outputs correct orientation)
        ImVec2 uv0, uv1;
        if (show_original_) {
            // Original texture needs vertical flip
            uv0 = ImVec2(0, 1);
            uv1 = ImVec2(1, 0);
        } else {
            // Scaled texture is already in correct orientation
            uv0 = ImVec2(0, 0);
            uv1 = ImVec2(1, 1);
        }

        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex_id)),
                     size, uv0, uv1);

        // Show zoom on hover
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            float display_size = 160.0f;  // Size of zoomed display in pixels

            ImGuiIO& io = ImGui::GetIO();

            // Use the screen position we captured before drawing
            ImVec2 pos = image_screen_pos;
            ImVec2 size_actual = size;  // Use the size we passed to ImGui::Image

            // Calculate mouse position relative to the displayed image (0 to 1)
            float mouse_x_in_image = io.MousePos.x - pos.x;
            float mouse_y_in_image = io.MousePos.y - pos.y;

            // Convert to normalized coordinates (0 to 1)
            // Ensure we don't divide by zero
            float mouse_rel_x = (size_actual.x > 0) ? (mouse_x_in_image / size_actual.x) : 0.0f;
            float mouse_rel_y = (size_actual.y > 0) ? (mouse_y_in_image / size_actual.y) : 0.0f;

            // Clamp to valid range
            mouse_rel_x = std::max(0.0f, std::min(1.0f, mouse_rel_x));
            mouse_rel_y = std::max(0.0f, std::min(1.0f, mouse_rel_y));

            // For the original texture, we need to account for the vertical flip
            // The display shows the texture flipped, so we need to unflip the Y coordinate
            // to get the actual pixel position in the texture
            float texture_y = mouse_rel_y;
            if (show_original_) {
                texture_y = 1.0f - mouse_rel_y;
            }

            // Calculate pixel position in texture space
            int pixel_x = static_cast<int>(mouse_rel_x * static_cast<float>(tex_width));
            int pixel_y = static_cast<int>(texture_y * static_cast<float>(tex_height));

            // Clamp pixel position to valid range
            pixel_x = std::max(0, std::min(tex_width - 1, pixel_x));
            pixel_y = std::max(0, std::min(tex_height - 1, pixel_y));

            // Calculate UV coordinates for the zoom region
            // Size of one pixel in UV space
            float one_pixel_u = 1.0f / static_cast<float>(tex_width);
            float one_pixel_v = 1.0f / static_cast<float>(tex_height);

            // Show a 5x5 pixel region centered on the current pixel
            float zoom_region = 5.0f;
            float region_u = zoom_region * one_pixel_u;
            float region_v = zoom_region * one_pixel_v;

            // Calculate UV position of the pixel center
            // Note: pixel_y is already in the correct texture space (accounting for flip if needed)
            float pixel_u = (static_cast<float>(pixel_x) + 0.5f) / static_cast<float>(tex_width);
            float pixel_v = (static_cast<float>(pixel_y) + 0.5f) / static_cast<float>(tex_height);

            // Calculate UV bounds for the zoom region
            float u0 = std::max(0.0f, pixel_u - region_u * 0.5f);
            float v0 = std::max(0.0f, pixel_v - region_v * 0.5f);
            float u1 = std::min(1.0f, pixel_u + region_u * 0.5f);
            float v1 = std::min(1.0f, pixel_v + region_v * 0.5f);

            // Apply coordinate transformation for display
            ImVec2 display_uv0, display_uv1;
            if (show_original_) {
                // Original texture needs Y flip for display
                display_uv0 = ImVec2(u0, 1.0f - v0);
                display_uv1 = ImVec2(u1, 1.0f - v1);
                // Swap Y coordinates to maintain proper orientation
                std::swap(display_uv0.y, display_uv1.y);
            } else {
                // Scaled texture displays as-is
                display_uv0 = ImVec2(u0, v0);
                display_uv1 = ImVec2(u1, v1);
            }

            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex_id)),
                        ImVec2(display_size, display_size),
                        display_uv0, display_uv1,
                        ImVec4(1, 1, 1, 1),  // Tint color (white = no tint)
                        ImVec4(0.8f, 0.8f, 0.8f, 1)); // Border color

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("5x5 zoom");
            ImGui::Text("Pixel: %d, %d", pixel_x, pixel_y);
            ImGui::EndGroup();
            ImGui::EndTooltip();
        }

        ImGui::EndChild();  // End ImageScrollRegion
    } else {
        ImGui::Text("No image to display");
    }

    ImGui::End();
}

void GUI::draw_stats() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Stats", &show_stats_,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);

    ImGui::End();
}

std::vector<scaler::algorithm> GUI::get_gpu_algorithms() {
    std::vector<scaler::algorithm> result;

    auto all_algos = scaler::algorithm_capabilities::get_all_algorithms();
    for (auto algo : all_algos) {
        const auto& info = scaler::algorithm_capabilities::get_info(algo);
        if (info.gpu_accelerated) {
            result.push_back(algo);
        }
    }

    return result;
}

std::string GUI::get_algorithm_name(scaler::algorithm algo) {
    const auto& info = scaler::algorithm_capabilities::get_info(algo);
    return info.name;
}

std::vector<float> GUI::get_supported_scales(scaler::algorithm algo) {
    const auto& info = scaler::algorithm_capabilities::get_info(algo);
    std::vector<float> scales;

    if (info.gpu_arbitrary_scale) {
        // For arbitrary scale, provide common options
        scales = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    } else {
        // Use fixed scales
        for (int scale : info.gpu_supported_scales) {
            scales.push_back(static_cast<float>(scale));
        }
    }

    return scales;
}

} // namespace gpu_scaler_app