#pragma once

// Check if SDL is available
#if defined(SCALER_HAS_SDL2) || defined(SCALER_HAS_SDL3)

#include <scaler/sdl/sdl_image.hh>
#include <scaler/epx.hh>
#include <scaler/eagle.hh>
#include <scaler/2xsai.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>
#include <scaler/hq3x.hh>
#include <scaler/scale2x_sfx.hh>
#include <scaler/scale3x.hh>
#include <scaler/scale3x_sfx.hh>
#include <scaler/omniscale.hh>

namespace scaler {
    // Convenience functions for SDL users - works with both SDL2 and SDL3
    // NOTE: HQ3x uses an optimized fast path for images <= 4096 pixels wide.
    // For best performance with other algorithms, consider implementing similar
    // optimizations using fixed-size arrays instead of dynamic vectors.

    inline SDL_Surface* scaleEpxSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleEpx<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleAdvMameSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleAdvMame<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleEagleSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleEagle<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scale2xSaISDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scale2xSaI<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleXbrSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleXbr<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleHq2xSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        // Automatically uses optimized fixed buffers for images <= 4096 pixels wide
        auto output = scaleHq2x<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleHq3xSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        // Automatically uses optimized fixed buffers for images <= 4096 pixels wide
        auto output = scaleHq3x<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale2xSFXSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleScale2xSFX<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale3xSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleScale3x<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale3xSFXSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleScale3xSFX<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleOmniScale2xSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleOmniScale2x<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }

    inline SDL_Surface* scaleOmniScale3xSDL(SDL_Surface* src) {
        SDLInputImage input(src);
        auto output = scaleOmniScale3x<SDLInputImage, SDLOutputImage>(input);
        return output.release();
    }
}
#endif // SCALER_HAS_SDL2 || SCALER_HAS_SDL3