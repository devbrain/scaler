//
// Created by igor on 2/23/25.
//

#pragma once

#include <algorithm>
#include <scaler/sdl_compat.hh>
#include <scaler/image_base.hh>  // For OutOfBoundsStrategy enum

class output_image {
    public:
        output_image(int w, int h, const SDL_Surface* src)
            : m_surface(nullptr),
              m_palette(nullptr),
              m_details(nullptr),
              m_bpp(SDL_BYTESPERPIXEL(src->format)) {
            m_surface = SDL_CreateSurface(w, h, src->format);
            m_palette = SDL_GetSurfacePalette(const_cast <SDL_Surface*>(src));
            m_details = SDL_GetPixelFormatDetails(src->format);
            if (m_palette) {
                SDL_SetSurfacePalette(m_surface, m_palette);
            }
            Uint32 color_key;
            if (SDL_GetSurfaceColorKey(const_cast <SDL_Surface*>(src), &color_key)) {
                SDL_SetSurfaceColorKey(m_surface, true, color_key);
            }
        }

        template<typename T>
        void set_pixel(int x, int y, const T& v) {
            Uint32 pixel = SDL_MapRGB(m_details, m_palette, (Uint8)v.x, (Uint8)v.y, (Uint8)v.z);
            auto* const target_pixel = reinterpret_cast <Uint32*>(static_cast <Uint8*>(m_surface->pixels)
                                                                  + y * m_surface->pitch
                                                                  + x * m_bpp);
            *target_pixel = pixel;
        }

        [[nodiscard]] SDL_Surface* get() const {
            return m_surface;
        }

    private:
        SDL_Surface* m_surface;
        SDL_Palette* m_palette;
        const SDL_PixelFormatDetails* m_details;
        int m_bpp;
};

template<typename T>
class input_image {
    public:
        explicit input_image(const SDL_Surface* srf)
            : m_surface(srf),
              m_palette(SDL_GetSurfacePalette(const_cast <SDL_Surface*>(srf))),
              m_details(SDL_GetPixelFormatDetails(srf->format)),
              m_bpp(SDL_BYTESPERPIXEL(srf->format)) {
        }

        [[nodiscard]] int width() const {
            return m_surface->w;
        }

        [[nodiscard]] int height() const {
            return m_surface->h;
        }

        T safeAccess(int x, int y, OutOfBoundsStrategy out_of_bounds_strategy = NEAREST) const {
            bool out_of_bounds = (x < 0 || x >= m_surface->w) || (y < 0 || y >= m_surface->h);
            Uint32 color = {};
            if (out_of_bounds) {
                switch (out_of_bounds_strategy) {
                    case ZERO:
                        return {};
                    case NEAREST:
                        color = getpixel(std::clamp(x, 0, m_surface->w - 1), std::clamp(y, 0, m_surface->h - 1));
                }
            } else {
                color = getpixel(x, y);
            }
            SDL_Color rgb;
            SDL_GetRGB(color, m_details, m_palette, &rgb.r, &rgb.g, &rgb.b);
            using scalar_t = typename T::value_type;
            return {static_cast <scalar_t>(rgb.r), static_cast <scalar_t>(rgb.g), static_cast <scalar_t>(rgb.b)};
        }

        [[nodiscard]] output_image get_output(int w, int h) const {
            return {w, h, m_surface};
        }

    private:
        const SDL_Surface* m_surface;
        const SDL_Palette* m_palette;
        const SDL_PixelFormatDetails* m_details;
        int m_bpp;

        [[nodiscard]] Uint32 getpixel(int x, int y) const {
            union {
                Uint8* bytes;
                Uint16* words;
                Uint32* dwords;
            } p{};
            /* Here p is the address to the pixel we want to retrieve */
            p.bytes = static_cast <Uint8*>(m_surface->pixels) + y * m_surface->pitch + x * m_bpp;

            switch (m_bpp) {
                case 1:
                    return *p.bytes;
                    break;

                case 2:
                    return *p.words;
                    break;

                case 3:
                    if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                        return (static_cast <Uint32>(p.bytes[0]) << 16) | (static_cast <Uint32>(p.bytes[1]) << 8) |
                               static_cast <Uint32>(p.bytes[2]);
                    else
                        return (static_cast <Uint32>(p.bytes[2]) << 16) | (static_cast <Uint32>(p.bytes[1]) << 8) |
                               static_cast <Uint32>(p.bytes[0]);
                    break;

                case 4:
                    return *p.dwords;
                    break;

                default:
                    return 0; /* shouldn't happen, but avoids warnings */
            }
        }
};


