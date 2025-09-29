#pragma once

#include <scaler/image_base.hh>
#include <scaler/types.hh>
#include <scaler/warning_macros.hh>

// OpenGL headers
#ifdef SCALER_PLATFORM_MACOS
    #include <OpenGL/gl3.h>
#else
    #include <GL/glew.h>
    #include <GL/gl.h>
#endif

namespace scaler::gpu {

    /**
     * Reference to an OpenGL texture with dimensions
     * This is a lightweight wrapper that doesn't own the texture
     */
    struct texture_ref {
        GLuint id;
        size_t width;
        size_t height;

        texture_ref() : id(0), width(0), height(0) {}
        texture_ref(GLuint tex_id, size_t w, size_t h)
            : id(tex_id), width(w), height(h) {}

        bool valid() const { return id != 0 && width > 0 && height > 0; }
    };

    /**
     * Input texture wrapper that implements the input_image_base interface
     * This allows texture_ref to be used with the unified scaler
     */
    class input_texture : public input_image_base<input_texture, uvec3> {
        texture_ref tex_;

    public:
        input_texture(const texture_ref& tex) : tex_(tex) {}
        input_texture(GLuint id, size_t width, size_t height)
            : tex_(id, width, height) {}

        // Required interface methods
        size_t width_impl() const { return tex_.width; }
        size_t height_impl() const { return tex_.height; }

        // GPU textures don't support direct pixel access
        // This is only used for CPU algorithms which won't be called for GPU
        uvec3 get_pixel_impl([[maybe_unused]] size_t x, [[maybe_unused]] size_t y) const {
            throw std::runtime_error("GPU textures don't support direct pixel access");
        }

        // Expose the texture reference
        const texture_ref& texture() const { return tex_; }
        GLuint id() const { return tex_.id; }
    };

    /**
     * Output texture wrapper that implements both input and output interfaces
     * This allows reading from and writing to textures
     */
    class output_texture : public input_image_base<output_texture, uvec3>,
                          public output_image_base<output_texture, uvec3> {
        texture_ref tex_;

    public:
        output_texture(const texture_ref& tex) : tex_(tex) {}
        output_texture(GLuint id, size_t width, size_t height)
            : tex_(id, width, height) {}

        // Constructor for creating from input (for compatibility)
        template<typename T>
        output_texture(size_t width, size_t height, const T&)
            : tex_(0, width, height) {
            // Note: Texture ID should be set externally
        }

        // Use input_image_base methods to avoid ambiguity
        using input_image_base<output_texture, uvec3>::width;
        using input_image_base<output_texture, uvec3>::height;

        // Required interface methods
        size_t width_impl() const { return tex_.width; }
        size_t height_impl() const { return tex_.height; }

        // GPU textures don't support direct pixel access
        uvec3 get_pixel_impl([[maybe_unused]] size_t x, [[maybe_unused]] size_t y) const {
            throw std::runtime_error("GPU textures don't support direct pixel access");
        }

        void set_pixel_impl([[maybe_unused]] size_t x, [[maybe_unused]] size_t y,
                           [[maybe_unused]] const uvec3& pixel) {
            throw std::runtime_error("GPU textures don't support direct pixel write");
        }

        // Expose the texture reference
        const texture_ref& texture() const { return tex_; }
        texture_ref& texture() { return tex_; }
        GLuint id() const { return tex_.id; }

        // Set texture ID (needed for deferred creation)
        void set_texture_id(GLuint id) { tex_.id = id; }
    };

} // namespace scaler::gpu