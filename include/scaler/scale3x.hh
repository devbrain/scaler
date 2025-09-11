#pragma once

#include <scaler/image_base.hh>
#include <scaler/scaler_common.hh>
#include <scaler/sliding_window_buffer.hh>

namespace scaler {
    // Scale3x algorithm - 3x magnification version of Scale2x
    // http://www.scale2x.it/algorithm
    template<typename InputImage, typename OutputImage>
    auto scaleScale3x(const InputImage& src, size_t scale_factor = 3)
        -> OutputImage {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);

        // Use cache-friendly sliding window buffer for 3x3 neighborhood
        using PixelType = decltype(src.get_pixel(0, 0));
        SlidingWindow3x3<PixelType> window(src.width());
        window.initialize(src, 0);

        for (size_t y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            // Get row references once per scanline for better performance
            const auto& topRow = window.getRow(-1);
            const auto& midRow = window.getRow(0);
            const auto& botRow = window.getRow(1);
            const int pad = window.getPadding();

            for (size_t x = 0; x < src.width(); x++) {
                // Get 3x3 neighborhood from cached row references
                const int xp = static_cast<int>(x) + pad;
                auto A = topRow[static_cast<size_t>(xp - 1)];
                auto B = topRow[static_cast<size_t>(xp)];
                auto C = topRow[static_cast<size_t>(xp + 1)];

                auto D = midRow[static_cast<size_t>(xp - 1)];
                auto E = midRow[static_cast<size_t>(xp)];
                auto F = midRow[static_cast<size_t>(xp + 1)];

                auto G = botRow[static_cast<size_t>(xp - 1)];
                auto H = botRow[static_cast<size_t>(xp)];
                auto I = botRow[static_cast<size_t>(xp + 1)];

                // Scale3x algorithm rules
                auto E0 = E;
                auto E1 = E;
                auto E2 = E;
                auto E3 = E;
                auto E4 = E;
                auto E5 = E;
                auto E6 = E;
                auto E7 = E;
                auto E8 = E;

                if (B != H && D != F) {
                    E0 = D == B ? D : E;
                    E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
                    E2 = B == F ? F : E;
                    E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
                    E4 = E;
                    E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
                    E6 = D == H ? D : E;
                    E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
                    E8 = H == F ? F : E;
                }

                size_t dst_x = scale_factor * x;
                size_t dst_y = scale_factor * y;

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