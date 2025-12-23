#include "image_loader.hh"
#include "rotozoom_bmp.h"

#include <scaler/sdl/sdl_compat.hh>
#include <GL/glew.h>
#include <iostream>
#include <cstring>

namespace gpu_scaler_app {

ImageLoader::ImageLoader() = default;

ImageLoader::~ImageLoader() {
    if (surface_) {
        SDL_FreeSurface(surface_);
        surface_ = nullptr;
    }
}

bool ImageLoader::load_embedded_image() {
    // Create surface from embedded data
    surface_ = create_surface_from_embedded();
    return surface_ != nullptr;
}

bool ImageLoader::load_from_file(const char* filename) {
    // Clean up previous surface
    if (surface_) {
        SDL_FreeSurface(surface_);
        surface_ = nullptr;
    }

    // Load image using SDL
    surface_ = SDL_LoadBMP(filename);

    if (!surface_) {
        std::cerr << "Failed to load image: " << SDL_GetError() << "\n";
        return false;
    }

    // Convert to RGBA format if needed
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* converted = SDL_ConvertSurface(surface_, format, 0);
    SDL_FreeFormat(format);

    if (converted) {
        SDL_FreeSurface(surface_);
        surface_ = converted;
    }

    return true;
}

int ImageLoader::get_width() const {
    if (surface_) {
        return surface_->w;
    }
    return 0;
}

int ImageLoader::get_height() const {
    if (surface_) {
        return surface_->h;
    }
    return 0;
}

unsigned int ImageLoader::create_texture() const {
    if (!surface_) {
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 surface_->w, surface_->h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, surface_->pixels);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

SDL_Surface* ImageLoader::flip_surface_vertically(SDL_Surface* surface) {
    if (!surface) {
        return nullptr;
    }

    // Create a new surface with the same format
#if SCALER_SDL_VERSION == 3
    SDL_Surface* flipped = SDL_CreateSurface(
        surface->w, surface->h,
        surface->format->format
    );
#else
    SDL_Surface* flipped = SDL_CreateRGBSurfaceWithFormat(
        0,
        surface->w, surface->h,
        surface->format->BitsPerPixel,
        surface->format->format
    );
#endif

    if (!flipped) {
        return nullptr;
    }

    // Lock both surfaces
    SDL_LockSurface(surface);
    SDL_LockSurface(flipped);

    // Copy pixels row by row, flipping vertically
    int pitch = surface->pitch;
    auto* src_pixels = static_cast<uint8_t*>(surface->pixels);
    auto* dst_pixels = static_cast<uint8_t*>(flipped->pixels);

    for (int y = 0; y < surface->h; ++y) {
        // Copy from bottom to top
        std::memcpy(dst_pixels + y * pitch,
                    src_pixels + (surface->h - 1 - y) * pitch,
                    static_cast<size_t>(pitch));
    }

    // Unlock surfaces
    SDL_UnlockSurface(flipped);
    SDL_UnlockSurface(surface);

    return flipped;
}

SDL_Surface* ImageLoader::create_surface_from_embedded() {
    // Create SDL_RWops from embedded BMP data
    SDL_RWops* rw = SDL_RWFromMem(
        const_cast<void*>(static_cast<const void*>(rotozoom_bmp_data)),
        static_cast<int>(rotozoom_bmp_data_len)
    );

    if (!rw) {
        std::cerr << "Failed to create RWops: " << SDL_GetError() << "\n";
        return nullptr;
    }

    // Load BMP from memory
    SDL_Surface* surface = SDL_LoadBMP_RW(rw, 1);  // 1 = free RWops after use

    if (!surface) {
        std::cerr << "Failed to load BMP from memory: " << SDL_GetError() << "\n";
        return nullptr;
    }

    // Convert to RGBA format for OpenGL
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* converted = SDL_ConvertSurface(surface, format, 0);
    SDL_FreeFormat(format);

    if (converted) {
        SDL_FreeSurface(surface);
        surface = converted;
    }

    // Flip the surface vertically (BMP files are stored bottom-up)
    if (surface) {
        SDL_Surface* flipped = flip_surface_vertically(surface);
        if (flipped) {
            SDL_FreeSurface(surface);
            surface = flipped;
        }
    }

    return surface;
}

} // namespace gpu_scaler_app