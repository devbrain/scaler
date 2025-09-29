#pragma once

// Check if SDL is available
#if defined(SCALER_HAS_SDL2) || defined(SCALER_HAS_SDL3)

#include <scaler/sdl/sdl_image.hh>
#include <scaler/cpu/epx.hh>
#include <scaler/cpu/eagle.hh>
#include <scaler/cpu/2xsai.hh>
#include <scaler/cpu/xbr.hh>
#include <scaler/cpu/hq2x.hh>
#include <scaler/cpu/hq3x.hh>
#include <scaler/cpu/scale2x_sfx.hh>
#include <scaler/cpu/scale3x.hh>
#include <scaler/cpu/scale3x_sfx.hh>
#include <scaler/cpu/omniscale.hh>

namespace scaler {
    // Convenience functions for SDL users - works with both SDL2 and SDL3
    // NOTE: HQ3x uses an optimized fast path for images <= 4096 pixels wide.
    // For best performance with other algorithms, consider implementing similar
    // optimizations using fixed-size arrays instead of dynamic vectors.

    inline SDL_Surface* scaleEpxSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_epx<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleAdvMameSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_adv_mame<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleEagleSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_eagle<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scale2xSaISDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_2x_sai<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleXbrSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_xbr<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleHq2xSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        // Automatically uses optimized fixed buffers for images <= 4096 pixels wide
        auto output = scale_hq2x<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleHq3xSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        // Automatically uses optimized fixed buffers for images <= 4096 pixels wide
        auto output = scale_hq_3x<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale2xSFXSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_scale_2x_sfx<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale3xSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_scale_3x<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleScale3xSFXSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_scale_3x_sfx<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleOmniScale2xSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_omni_scale_2x<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }

    inline SDL_Surface* scaleOmniScale3xSDL(SDL_Surface* src) {
        sdl_input_image input(src);
        auto output = scale_omni_scale_3x<sdl_input_image, sdl_output_image>(input);
        return output.release();
    }
}
#endif // SCALER_HAS_SDL2 || SCALER_HAS_SDL3