#pragma once

#include <memory>

struct SDL_Surface;

namespace gpu_scaler_app {

/**
 * Image loader class that manages loading images from embedded data or files
 */
class ImageLoader {
public:
    ImageLoader();
    ~ImageLoader();

    // Load the embedded test image
    bool load_embedded_image();

    // Load image from file
    bool load_from_file(const char* filename);

    // Get image dimensions
    [[nodiscard]] int get_width() const;
    [[nodiscard]] int get_height() const;

    // Create OpenGL texture from current image
    [[nodiscard]] unsigned int create_texture() const;

    // Get SDL surface (for compatibility)
    [[nodiscard]] SDL_Surface* get_surface() const { return surface_; }

    // Check if an image is loaded
    [[nodiscard]] bool has_image() const { return surface_ != nullptr; }

private:
    // Convert embedded data to SDL surface
    static SDL_Surface* create_surface_from_embedded();

    // Helper to flip surface vertically
    static SDL_Surface* flip_surface_vertically(SDL_Surface* surface);

private:
    SDL_Surface* surface_ = nullptr;  // We own this
};

} // namespace gpu_scaler_app