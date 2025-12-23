#pragma once

#include <scaler/sdl/sdl_compat.hh>
#include <scaler/image_base.hh>
#include <scaler/vec3.hh>
#include <algorithm>
namespace scaler {
    class sdl_output_image;  // Forward declaration

    class sdl_input_image : public input_image_base<sdl_input_image, uvec3> {
        friend class sdl_output_image;
        public:
            explicit sdl_input_image(SDL_Surface* surface)
                : m_surface(surface),
        #ifdef SCALER_HAS_SDL3
                  m_bpp(static_cast<unsigned int>(SDL_BYTESPERPIXEL(surface->format))),
                  m_details(SDL_GetPixelFormatDetails(surface->format)),
        #else
                  m_bpp(static_cast<unsigned int>(surface->format->BytesPerPixel)),
                  m_details(surface->format),
        #endif
                  m_palette(SDL_GetSurfacePalette(surface)) {}

            [[nodiscard]] size_t width_impl() const {
                return static_cast<size_t>(m_surface->w);
            }

            [[nodiscard]] size_t height_impl() const {
                return static_cast<size_t>(m_surface->h);
            }

            [[nodiscard]] uvec3 get_pixel_impl(size_t x, size_t y) const {
                const Uint8* const src_pixel = static_cast<const Uint8*>(m_surface->pixels)
                                               + y * static_cast<size_t>(m_surface->pitch)
                                               + x * m_bpp;

                Uint32 pixel;
                switch (m_bpp) {
                    case 1:
                        pixel = *src_pixel;
                        break;
                    case 2:
                        pixel = *reinterpret_cast<const Uint16*>(src_pixel);
                        break;
                    case 3:
                        if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                            pixel = static_cast<Uint32>(src_pixel[0]) << 16 | static_cast<Uint32>(src_pixel[1]) << 8 | static_cast<Uint32>(src_pixel[2]);
                        } else {
                            pixel = static_cast<Uint32>(src_pixel[0]) | static_cast<Uint32>(src_pixel[1]) << 8 | static_cast<Uint32>(src_pixel[2]) << 16;
                        }
                        break;
                    case 4:
                        pixel = *reinterpret_cast<const Uint32*>(src_pixel);
                        break;
                    default:
                        return {0, 0, 0};
                }

                Uint8 r, g, b;
                SDL_GetRGB(pixel, m_details, m_palette, &r, &g, &b);
                return {static_cast<unsigned int>(r),
                        static_cast<unsigned int>(g),
                        static_cast<unsigned int>(b)};
            }

        private:
            SDL_Surface* m_surface;
            unsigned int m_bpp;
            const SDL_PixelFormatDetails* m_details;
            SDL_Palette* m_palette;
    };

    class sdl_output_image : public output_image_base<sdl_output_image, uvec3> {
        public:
            // Constructor with SDL_Surface template
            sdl_output_image(size_t width, size_t height, const SDL_Surface* template_surface)
                : m_surface(nullptr),
                  m_palette(nullptr),
                  m_details(nullptr),
        #ifdef SCALER_HAS_SDL3
                  m_bpp(static_cast<unsigned int>(SDL_BYTESPERPIXEL(template_surface->format))) {

                m_surface = SDL_CreateSurface(static_cast<int>(width), static_cast<int>(height), template_surface->format);
#else
            m_bpp(static_cast<unsigned int>(template_surface->format->BytesPerPixel)) {

                m_surface = SDL_CreateRGBSurfaceWithFormat(0, static_cast<int>(width), static_cast<int>(height),
                                                           template_surface->format->BitsPerPixel,
                                                           template_surface->format->format);
#endif
                m_palette = SDL_GetSurfacePalette(const_cast<SDL_Surface*>(template_surface));
#ifdef SCALER_HAS_SDL3
                m_details = SDL_GetPixelFormatDetails(template_surface->format);
#else
                m_details = const_cast<SDL_Surface*>(template_surface)->format;
#endif

                if (m_palette) {
#ifdef SCALER_HAS_SDL3
                    SDL_SetSurfacePalette(m_surface, m_palette);
#else
                    SDL_SetSurfacePalette(m_surface, m_palette);  // SDL2 returns int, we ignore it
#endif
                }

                Uint32 color_key;
                if (SDL_GetSurfaceColorKey(const_cast<SDL_Surface*>(template_surface), &color_key)) {
                    SDL_SetSurfaceColorKey(m_surface, true, color_key);
                }
            }

            // Constructor with sdl_input_image template
            sdl_output_image(size_t width, size_t height, const sdl_input_image& template_img)
                : sdl_output_image(width, height, template_img.m_surface) {}

            // Constructor with another sdl_output_image as template
            sdl_output_image(size_t width, size_t height, const sdl_output_image& template_img)
                : sdl_output_image(width, height, template_img.m_surface) {}

            ~sdl_output_image() {
                if (m_surface) {
                    SDL_DestroySurface(m_surface);
                }
            }

            // Move constructor
            sdl_output_image(sdl_output_image&& other) noexcept
                : m_surface(other.m_surface),
                  m_palette(other.m_palette),
                  m_details(other.m_details),
                  m_bpp(other.m_bpp) {
                other.m_surface = nullptr;
            }

            // Move assignment
            sdl_output_image& operator=(sdl_output_image&& other) noexcept {
                if (this != &other) {
                    if (m_surface) {
                        SDL_DestroySurface(m_surface);
                    }
                    m_surface = other.m_surface;
                    m_palette = other.m_palette;
                    m_details = other.m_details;
                    m_bpp = other.m_bpp;
                    other.m_surface = nullptr;
                }
                return *this;
            }

            // Delete copy operations
            sdl_output_image(const sdl_output_image&) = delete;
            sdl_output_image& operator=(const sdl_output_image&) = delete;

            [[nodiscard]] size_t width_impl() const {
                return m_surface ? static_cast<size_t>(m_surface->w) : 0;
            }

            [[nodiscard]] size_t height_impl() const {
                return m_surface ? static_cast<size_t>(m_surface->h) : 0;
            }

            // Add get_pixel method for algorithms that need to read from output
            [[nodiscard]] uvec3 get_pixel(size_t x, size_t y) const {
                return get_pixel_impl(x, y);
            }

            // Add safe_access for algorithms that use output as intermediate input
            [[nodiscard]] uvec3 safe_access(int x, int y,
                                           out_of_bounds_strategy strategy = NEAREST) const {
                const int w = static_cast<int>(width_impl());
                const int h = static_cast<int>(height_impl());

                // Handle out-of-bounds cases
                if (x < 0 || x >= w || y < 0 || y >= h) {
                    switch (strategy) {
                        case ZERO:
                            return {0, 0, 0};
                        case NEAREST:
                            x = std::max(0, std::min(w - 1, x));
                            y = std::max(0, std::min(h - 1, y));
                            break;
                    }
                }

                return get_pixel_impl(static_cast<size_t>(x), static_cast<size_t>(y));
            }

            void set_pixel_impl(size_t x, size_t y, const uvec3& pixel) {
                Uint32 color = SDL_MapRGB(m_details, m_palette,
                                          static_cast<Uint8>(pixel.x),
                                          static_cast<Uint8>(pixel.y),
                                          static_cast<Uint8>(pixel.z));

                Uint8* const target_pixel = static_cast<Uint8*>(m_surface->pixels)
                                           + y * static_cast<size_t>(m_surface->pitch)
                                           + x * m_bpp;

                switch (m_bpp) {
                    case 1:
                        *target_pixel = static_cast<Uint8>(color);
                        break;
                    case 2:
                        *reinterpret_cast<Uint16*>(target_pixel) = static_cast<Uint16>(color);
                        break;
                    case 3:
                        if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                            target_pixel[0] = (color >> 16) & 0xff;
                            target_pixel[1] = (color >> 8) & 0xff;
                            target_pixel[2] = color & 0xff;
                        } else {
                            target_pixel[0] = color & 0xff;
                            target_pixel[1] = (color >> 8) & 0xff;
                            target_pixel[2] = (color >> 16) & 0xff;
                        }
                        break;
                    case 4:
                        *reinterpret_cast<Uint32*>(target_pixel) = color;
                        break;
                    default:
                        // Should not happen with valid SDL surfaces
                        break;
                }
            }

            [[nodiscard]] uvec3 get_pixel_impl(size_t x, size_t y) const {
                const Uint8* const src_pixel = static_cast<const Uint8*>(m_surface->pixels)
                                               + y * static_cast<size_t>(m_surface->pitch)
                                               + x * m_bpp;

                Uint32 pixel;
                switch (m_bpp) {
                    case 1:
                        pixel = *src_pixel;
                        break;
                    case 2:
                        pixel = *reinterpret_cast<const Uint16*>(src_pixel);
                        break;
                    case 3:
                        if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                            pixel = static_cast<Uint32>(src_pixel[0]) << 16 | static_cast<Uint32>(src_pixel[1]) << 8 | static_cast<Uint32>(src_pixel[2]);
                        } else {
                            pixel = static_cast<Uint32>(src_pixel[0]) | static_cast<Uint32>(src_pixel[1]) << 8 | static_cast<Uint32>(src_pixel[2]) << 16;
                        }
                        break;
                    case 4:
                        pixel = *reinterpret_cast<const Uint32*>(src_pixel);
                        break;
                    default:
                        return {0, 0, 0};
                }

                Uint8 r, g, b;
                SDL_GetRGB(pixel, m_details, m_palette, &r, &g, &b);
                return {static_cast<unsigned int>(r),
                        static_cast<unsigned int>(g),
                        static_cast<unsigned int>(b)};
            }

            [[nodiscard]] SDL_Surface* get_surface() const {
                return m_surface;
            }

            SDL_Surface* release() {
                SDL_Surface* surf = m_surface;
                m_surface = nullptr;
                return surf;
            }

        private:
            SDL_Surface* m_surface;
            SDL_Palette* m_palette;
            const SDL_PixelFormatDetails* m_details;
            unsigned int m_bpp;
    };
}