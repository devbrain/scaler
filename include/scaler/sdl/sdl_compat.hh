#pragma once

// SDL version detection and includes
#ifdef SCALER_HAS_SDL3
    #include <SDL3/SDL.h>
#elif defined(SCALER_HAS_SDL2)
    #include <SDL2/SDL.h>
#else
    #error "No SDL version defined. Please ensure SDL2 or SDL3 is found by CMake."
#endif

// SDL2/SDL3 compatibility layer
#ifdef SCALER_HAS_SDL2
    // SDL2 compatibility definitions
    using SDL_PixelFormatDetails = SDL_PixelFormat;

    // SDL2 GL context flag compatibility - SDL3 renamed this
    #ifndef SDL_GL_CONTEXT_FORWARD_COMPATIBLE
        #define SDL_GL_CONTEXT_FORWARD_COMPATIBLE SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
    #endif
    
    // SDL2 doesn't have these functions, provide alternatives
    inline SDL_Palette* SDL_GetSurfacePalette(SDL_Surface* surface) {
        return surface->format->palette;
    }
    
    inline const SDL_PixelFormatDetails* SDL_GetPixelFormatDetails(SDL_PixelFormat* format) {
        return format;
    }
    
    inline const SDL_PixelFormatDetails* SDL_GetPixelFormatDetails(Uint32 format) {
        // For SDL2, when given a format enum, allocate a format struct
        return SDL_AllocFormat(format);
    }
    
    // SDL2 already has SDL_SetSurfacePalette, no need to redefine
    
    inline bool SDL_GetSurfaceColorKey(SDL_Surface* surface, Uint32* key) {
        return SDL_GetColorKey(surface, key) == 0;
    }
    
    inline int SDL_SetSurfaceColorKey(SDL_Surface* surface, bool enable, Uint32 key) {
        return SDL_SetColorKey(surface, enable ? SDL_TRUE : SDL_FALSE, key);
    }
    
    // Modified GetRGB for SDL2 - ignore palette parameter
    inline void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormatDetails* format, 
                          [[maybe_unused]] const SDL_Palette* palette, Uint8* r, Uint8* g, Uint8* b) {
        SDL_GetRGB(pixel, const_cast<SDL_PixelFormat*>(format), r, g, b);
    }
    
    // Modified MapRGB for SDL2 - ignore palette parameter
    inline Uint32 SDL_MapRGB(const SDL_PixelFormatDetails* format, 
                             [[maybe_unused]] const SDL_Palette* palette, Uint8 r, Uint8 g, Uint8 b) {
        return SDL_MapRGB(const_cast<SDL_PixelFormat*>(format), r, g, b);
    }
    
    // SDL2 surface creation compatibility
    inline SDL_Surface* SDL_CreateSurface(int width, int height, SDL_PixelFormat* format) {
        return SDL_CreateRGBSurfaceWithFormat(0, width, height, 
                                              format->BitsPerPixel, format->format);
    }
    
    inline SDL_Surface* SDL_CreateSurface(int width, int height, Uint32 format) {
        Uint32 Rmask, Gmask, Bmask, Amask;
        int bpp;
        SDL_PixelFormatEnumToMasks(format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
        return SDL_CreateRGBSurfaceWithFormat(0, width, height, bpp, format);
    }
    
    // SDL2 uses SDL_FreeSurface instead of SDL_DestroySurface
    inline void SDL_DestroySurface(SDL_Surface* surface) {
        SDL_FreeSurface(surface);
    }
    
    // SDL2 BYTESPERPIXEL macro helper
    inline int SDL_BYTESPERPIXEL_COMPAT(SDL_PixelFormat* format) {
        return format->BytesPerPixel;
    }
    
    inline int SDL_BYTESPERPIXEL_COMPAT(Uint32 format) {
        return SDL_BYTESPERPIXEL(format);
    }
    
    // Redefine for compatibility
    #undef SDL_BYTESPERPIXEL
    #define SDL_BYTESPERPIXEL(format) SDL_BYTESPERPIXEL_COMPAT(format)
    
    // SDL2 RWops compatibility (SDL3 uses SDL_IOStream)
    using SDL_IOStream = SDL_RWops;
    
    inline SDL_IOStream* SDL_IOFromConstMem(const void* mem, size_t size) {
        return SDL_RWFromConstMem(mem, static_cast<int>(size));
    }
    
    inline SDL_Surface* SDL_LoadBMP_IO(SDL_IOStream* src, bool closeio) {
        return SDL_LoadBMP_RW(src, closeio ? 1 : 0);
    }
#else
    // SDL3 definitions - these are already in SDL3
    // Just provide helper for consistency
    inline SDL_Surface* SDL_LoadBMP_IO(SDL_IOStream* src, bool closeio) {
        return SDL_LoadBMP_IO(src, closeio);
    }
#endif