#pragma once

// Check if SDL is available
#if defined(SCALER_HAS_SDL2) || defined(SCALER_HAS_SDL3)

#include <scaler/sdl_image.hh>
#include <scaler/epx.hh>
#include <scaler/eagle.hh>
#include <scaler/2xsai.hh>
#include <scaler/xbr.hh>
#include <scaler/hq2x.hh>

// Convenience functions for SDL users - works with both SDL2 and SDL3

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
    auto output = scaleHq2x<SDLInputImage, SDLOutputImage>(input);
    return output.release();
}

#endif // SCALER_HAS_SDL2 || SCALER_HAS_SDL3