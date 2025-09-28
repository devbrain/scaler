#pragma once

#include <scaler/cpu/sliding_window_buffer.hh>

namespace scaler {
    // Improved Scale2x algorithm by Sp00kyFox
    // https://web.archive.org/web/20160527015550/https://libretro.com/forums/archive/index.php?t-1655.html
    template<typename InputImage, typename OutputImage>
    OutputImage scale_scale_2x_sfx(const InputImage& src, size_t scale_factor = 2) {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 5x5 neighborhood (needed for SFX variant)
        using PixelType = decltype(src.get_pixel(0, 0));
        sliding_window_5x5 <PixelType> window(src.width());
        window.initialize(src, 0);

        for (size_t y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            for (size_t x = 0; x < src.width(); x++) {
                // Get 5x5 neighborhood from cache-friendly buffer
                PixelType neighborhood[5][5];
                window.get_neighborhood(x, neighborhood);

                // Map to variable names used in algorithm
                auto J = neighborhood[0][2]; // y-2, x

                auto A = neighborhood[1][1]; // y-1, x-1
                auto B = neighborhood[1][2]; // y-1, x
                auto C = neighborhood[1][3]; // y-1, x+1

                auto K = neighborhood[2][0]; // y, x-2
                auto D = neighborhood[2][1]; // y, x-1
                auto E = neighborhood[2][2]; // y, x (center)
                auto F = neighborhood[2][3]; // y, x+1
                auto L = neighborhood[2][4]; // y, x+2

                auto G = neighborhood[3][1]; // y+1, x-1
                auto H = neighborhood[3][2]; // y+1, x
                auto I = neighborhood[3][3]; // y+1, x+1

                auto M = neighborhood[4][2]; // y+2, x

                // Improved Scale2x SFX algorithm rules
                // E0 = B==D && B!=F && D!=H && (E!=A || E==C || E==G || A==J || A==K) ? D : E
                // E1 = B==F && B!=D && F!=H && (E!=C || E==A || E==I || C==J || C==L) ? F : E
                // E2 = D==H && B!=D && F!=H && (E!=G || E==A || E==I || G==K || G==M) ? D : E
                // E3 = F==H && B!=F && D!=H && (E!=I || E==C || E==G || I==L || I==M) ? F : E

                auto E0 = (B == D && B != F && D != H && (E != A || E == C || E == G || A == J || A == K)) ? D : E;
                auto E1 = (B == F && B != D && F != H && (E != C || E == A || E == I || C == J || C == L)) ? F : E;
                auto E2 = (D == H && B != D && F != H && (E != G || E == A || E == I || G == K || G == M)) ? D : E;
                auto E3 = (F == H && B != F && D != H && (E != I || E == C || E == G || I == L || I == M)) ? F : E;

                size_t dst_x = scale_factor * x;
                size_t dst_y = scale_factor * y;

                // Write 2x2 output block
                result.set_pixel(dst_x, dst_y, E0);
                result.set_pixel(dst_x + 1, dst_y, E1);
                result.set_pixel(dst_x, dst_y + 1, E2);
                result.set_pixel(dst_x + 1, dst_y + 1, E3);
            }
        }
        return result;
    }
}
