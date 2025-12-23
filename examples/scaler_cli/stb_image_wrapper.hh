#pragma once

#include <scaler/vec3.hh>
#include <scaler/warning_macros.hh>
#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cctype>

// STB image implementation in this header
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// Disable all warnings for third-party STB headers
SCALER_DISABLE_ALL_WARNINGS_PUSH

#include "stb_image.h"
#include "stb_image_write.h"

// Re-enable warnings
SCALER_DISABLE_ALL_WARNINGS_POP

namespace scaler {

/**
 * STB Image wrapper that implements the interface required by unified_scaler
 *
 * This class wraps STB image data and provides the methods needed for the
 * unified scaler template: width(), height(), get_pixel(), set_pixel()
 */
class stb_image {
public:
    using pixel_type = vec3<uint8_t>;

    /**
     * Load image from file
     * @param filename Path to image file
     * @throws std::runtime_error if loading fails
     */
    explicit stb_image(const char* filename)
        : m_stb_allocated(true)
        , m_data(nullptr, smart_deleter(true)) {
        int w, h, channels;
        unsigned char* data = stbi_load(filename, &w, &h, &channels, 0);

        if (!data) {
            throw std::runtime_error(std::string("Failed to load image: ") + stbi_failure_reason());
        }

        m_width = static_cast<size_t>(w);
        m_height = static_cast<size_t>(h);
        m_channels = channels;
        m_data.reset(data);

        // Convert to RGBA if needed for consistent handling
        if (m_channels < 3) {
            convert_to_rgb();
        }
    }

    /**
     * Create empty image with specified dimensions
     * @param width Image width
     * @param height Image height
     * @param channels Number of color channels (3 for RGB, 4 for RGBA)
     */
    stb_image(size_t width, size_t height, int channels = 3)
        : m_width(width)
        , m_height(height)
        , m_channels(channels)
        , m_stb_allocated(false)
        , m_data(new unsigned char[width * height * static_cast<size_t>(channels)](), smart_deleter(false)) {
    }

    /**
     * Constructor for compatibility with unified_scaler
     * Some algorithms create images with a copy of another image
     */
    stb_image(size_t width, size_t height, const stb_image& /*source*/)
        : m_width(width)
        , m_height(height)
        , m_channels(3)  // Default to RGB
        , m_stb_allocated(false)
        , m_data(new unsigned char[width * height * 3](), smart_deleter(false)) {
    }

    /**
     * Copy constructor
     */
    stb_image(const stb_image& other)
        : m_width(other.m_width)
        , m_height(other.m_height)
        , m_channels(other.m_channels)
        , m_stb_allocated(false)  // Always use new[] for copies
        , m_data(new unsigned char[m_width * m_height * static_cast<size_t>(m_channels)], smart_deleter(false)) {
        std::memcpy(m_data.get(), other.m_data.get(),
                    m_width * m_height * static_cast<size_t>(m_channels));
    }

    /**
     * Move constructor
     */
    stb_image(stb_image&& other) noexcept = default;

    /**
     * Assignment operators
     */
    stb_image& operator=(const stb_image& other) {
        if (this != &other) {
            m_width = other.m_width;
            m_height = other.m_height;
            m_channels = other.m_channels;
            m_stb_allocated = false;  // Always use new[] for copies
            m_data = std::unique_ptr<unsigned char[], smart_deleter>(
                new unsigned char[m_width * m_height * static_cast<size_t>(m_channels)],
                smart_deleter(false));
            std::memcpy(m_data.get(), other.m_data.get(),
                        m_width * m_height * static_cast<size_t>(m_channels));
        }
        return *this;
    }

    stb_image& operator=(stb_image&& other) noexcept = default;

    // Interface required by unified_scaler

    /**
     * Get image width
     */
    size_t width() const { return m_width; }

    /**
     * Get image height
     */
    size_t height() const { return m_height; }

    /**
     * Get number of channels
     */
    int channels() const { return m_channels; }

    /**
     * Get pixel at (x, y)
     * @param x X coordinate
     * @param y Y coordinate
     * @return Pixel value as vec3<uint8_t>
     */
    pixel_type get_pixel(size_t x, size_t y) const {
        if (x >= m_width || y >= m_height) {
            return pixel_type{0, 0, 0};
        }

        size_t idx = (y * m_width + x) * static_cast<size_t>(m_channels);

        return pixel_type{
            m_data[idx],
            m_data[idx + 1],
            m_data[idx + 2]
        };
    }

    /**
     * Set pixel at (x, y)
     * @param x X coordinate
     * @param y Y coordinate
     * @param pixel Pixel value to set
     */
    void set_pixel(size_t x, size_t y, const pixel_type& pixel) {
        if (x >= m_width || y >= m_height) {
            return;
        }

        size_t idx = (y * m_width + x) * static_cast<size_t>(m_channels);

        m_data[idx] = pixel.x;     // R
        m_data[idx + 1] = pixel.y; // G
        m_data[idx + 2] = pixel.z; // B

        // Preserve alpha if present
        if (m_channels == 4 && idx + 3 < m_width * m_height * static_cast<size_t>(m_channels)) {
            // Keep existing alpha value
        }
    }

    /**
     * Safe pixel access with boundary clamping
     * Required by some algorithms (HQ2x/3x) that use buffer_policy
     * @param x X coordinate (can be negative or out of bounds)
     * @param y Y coordinate (can be negative or out of bounds)
     * @return Pixel value at clamped coordinates
     */
    pixel_type safe_access(int x, int y) const {
        // Clamp coordinates to valid range
        x = std::max(0, std::min(x, static_cast<int>(m_width) - 1));
        y = std::max(0, std::min(y, static_cast<int>(m_height) - 1));

        return get_pixel(static_cast<size_t>(x), static_cast<size_t>(y));
    }

    /**
     * Save image to file
     * @param filename Output filename (format determined by extension)
     * @param quality JPEG quality (1-100, only for JPEG)
     * @return true on success, false on failure
     */
    bool save(const char* filename, int quality = 95) const {
        // Determine format from extension
        std::string fname(filename);
        size_t dot = fname.find_last_of('.');
        if (dot == std::string::npos) {
            return false;
        }

        std::string ext = fname.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        int w = static_cast<int>(m_width);
        int h = static_cast<int>(m_height);

        if (ext == "png") {
            return stbi_write_png(filename, w, h, m_channels,
                                 m_data.get(), w * m_channels) != 0;
        } else if (ext == "jpg" || ext == "jpeg") {
            return stbi_write_jpg(filename, w, h, m_channels,
                                 m_data.get(), quality) != 0;
        } else if (ext == "bmp") {
            return stbi_write_bmp(filename, w, h, m_channels,
                                 m_data.get()) != 0;
        } else if (ext == "tga") {
            return stbi_write_tga(filename, w, h, m_channels,
                                 m_data.get()) != 0;
        }

        return false;
    }

    /**
     * Get raw pixel data
     */
    const unsigned char* data() const { return m_data.get(); }
    unsigned char* data() { return m_data.get(); }

private:
    /**
     * Convert grayscale or other formats to RGB
     */
    void convert_to_rgb() {
        if (m_channels >= 3) return;

        size_t new_size = m_width * m_height * 3;
        unsigned char* new_data = new unsigned char[new_size];

        for (size_t i = 0; i < m_width * m_height; ++i) {
            unsigned char value = m_data[i * static_cast<size_t>(m_channels)];
            new_data[i * 3] = value;     // R
            new_data[i * 3 + 1] = value; // G
            new_data[i * 3 + 2] = value; // B
        }

        // Update with new[] allocated data
        m_stb_allocated = false;
        m_data = std::unique_ptr<unsigned char[], smart_deleter>(
            new_data, smart_deleter(false));
        m_channels = 3;
    }

    size_t m_width = 0;
    size_t m_height = 0;
    int m_channels = 0;
    bool m_stb_allocated = false;  // Track allocation source

    // Custom deleter that handles both STB and new[] allocations
    struct smart_deleter {
        bool is_stb_allocated;

        explicit smart_deleter(bool stb = false) : is_stb_allocated(stb) {}

        void operator()(unsigned char* p) const {
            if (p) {
                if (is_stb_allocated) {
                    stbi_image_free(p);
                } else {
                    delete[] p;
                }
            }
        }
    };

    // Use unique_ptr with custom deleter for RAII
    std::unique_ptr<unsigned char[], smart_deleter> m_data;
};

} // namespace scaler