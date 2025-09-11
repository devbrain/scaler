#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

namespace scaler {
    // Improved Scale3x algorithm by Sp00kyFox
    // https://web.archive.org/web/20160527015550/https://libretro.com/forums/archive/index.php?t-1655.html
    template<typename InputImage, typename OutputImage>
    auto scaleScale3xSFX(const InputImage& src, int scale_factor = 3)
        -> OutputImage {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 5x5 neighborhood (needed for SFX variant)
        using PixelType = decltype(src.get_pixel(0, 0));
        SlidingWindow5x5<PixelType> window(src.width());
        window.initialize(src, 0);

        for (int y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            for (int x = 0; x < src.width(); x++) {
                // Get 5x5 neighborhood from cache-friendly buffer
                PixelType neighborhood[5][5];
                window.getNeighborhood(x, neighborhood);

                // Map to variable names used in algorithm
                auto J = neighborhood[0][2];  // y-2, x

                auto A = neighborhood[1][1];  // y-1, x-1
                auto B = neighborhood[1][2];  // y-1, x
                auto C = neighborhood[1][3];  // y-1, x+1

                auto K = neighborhood[2][0];  // y, x-2
                auto D = neighborhood[2][1];  // y, x-1
                auto E = neighborhood[2][2];  // y, x (center)
                auto F = neighborhood[2][3];  // y, x+1
                auto L = neighborhood[2][4];  // y, x+2

                auto G = neighborhood[3][1];  // y+1, x-1
                auto H = neighborhood[3][2];  // y+1, x
                auto I = neighborhood[3][3];  // y+1, x+1

                auto M = neighborhood[4][2];  // y+2, x

                // Improved Scale3x SFX algorithm rules
                auto E4 = E; // Center always stays the same

                // Corner pixels use averaging for smoother interpolation
                auto E0 = ((B==D && B!=F && D!=H && (E!=A || E==C || E==G || A==J || A==K)) ||
                          (B==D && C==E && C!=J && A!=E) ||
                          (B==D && E==G && A!=E && G!=K)) ? mix(B, D, 0.5f) : E;

                auto E2 = ((B==F && B!=D && F!=H && (E!=C || E==A || E==I || C==J || C==L)) ||
                          (B==F && A==E && A!=J && C!=E) ||
                          (B==F && E==I && C!=E && I!=L)) ? mix(B, F, 0.5f) : E;

                auto E6 = ((D==H && B!=D && F!=H && (E!=G || E==A || E==I || G==K || G==M)) ||
                          (D==H && A==E && A!=K && E!=G) ||
                          (D==H && E==I && E!=G && I!=M)) ? mix(D, H, 0.5f) : E;

                auto E8 = ((F==H && B!=F && D!=H && (E!=I || E==C || E==G || I==L || I==M)) ||
                          (F==H && C==E && C!=L && E!=I) ||
                          (F==H && E==G && E!=I && G!=M)) ? mix(F, H, 0.5f) : E;

                // Edge pixels
                auto E1 = ((B==D && B!=F && D!=H && (E!=A || E==C || E==G || A==J || A==K) && E!=C) ||
                          (B==F && B!=D && F!=H && (E!=C || E==A || E==I || C==J || C==L) && E!=A)) ? B : E;

                auto E3 = ((B==D && B!=F && D!=H && (E!=A || E==C || E==G || A==J || A==K) && E!=G) ||
                          (D==H && B!=D && F!=H && (E!=G || E==A || E==I || G==K || G==M) && E!=A)) ? D : E;

                auto E5 = ((F==H && B!=F && D!=H && (E!=I || E==C || E==G || I==L || I==M) && E!=C) ||
                          (B==F && B!=D && F!=H && (E!=C || E==A || E==I || C==J || C==L) && E!=I)) ? F : E;

                auto E7 = ((F==H && B!=F && D!=H && (E!=I || E==C || E==G || I==L || I==M) && E!=G) ||
                          (D==H && B!=D && F!=H && (E!=G || E==A || E==I || G==K || G==M) && E!=I)) ? H : E;

                int dst_x = scale_factor * x;
                int dst_y = scale_factor * y;

                // Write 3x3 output block
                result.set_pixel(dst_x, dst_y, E0);
                result.set_pixel(dst_x + 1, dst_y, E1);
                result.set_pixel(dst_x + 2, dst_y, E2);

                result.set_pixel(dst_x, dst_y + 1, E3);
                result.set_pixel(dst_x + 1, dst_y + 1, E4);
                result.set_pixel(dst_x + 2, dst_y + 1, E5);

                result.set_pixel(dst_x, dst_y + 2, E6);
                result.set_pixel(dst_x + 1, dst_y + 2, E7);
                result.set_pixel(dst_x + 2, dst_y + 2, E8);
            }
        }
        return result;
    }
}