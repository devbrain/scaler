#pragma once

#include <scaler/image_base.hh>
#include <scaler/cpu/scaler_common.hh>
#include <scaler/cpu/sliding_window_buffer.hh>

namespace scaler {
    namespace detail {
        /**
         * Compute if C and D exclusively match either A or B.
         *
         * @param A First candidate for majority match
         * @param B Second candidate for majority match
         * @param C First value to check for majority match
         * @param D Second value to check for majority match
         *
         * @return 0 if no majority match on A or B; 1 if majority match ONLY on A; -1 if majority match ONLY on B
         */
        template<typename T>
        int8_t majorityMatch(T A, T B, T C, T D) {
            int8_t x, y, r;
            x = y = r = 0;
            if (A == C) { x += 1; } else if (B == C) { y += 1; }
            if (A == D) { x += 1; } else if (B == D) { y += 1; }
            if (x <= 1) { r -= 1; }
            if (y <= 1) { r += 1; }
            return r;
        }
    }

    // Generic 2xSaI scaler using CRTP - writes directly to output
    template<typename InputImage, typename OutputImage>
    void scale_2x_sai(const InputImage& src, OutputImage& result, size_t scale_factor = 2) {

        // Use cache-friendly sliding window buffer for 4x4 neighborhood
        using PixelType = decltype(src.get_pixel(0, 0));
        sliding_window_4x4 <PixelType> window(src.width());
        window.initialize(src, 0);

        for (size_t y = 0; y < src.height(); y++) {
            // Advance sliding window for next row
            if (y > 0) {
                window.advance(src);
            }

            for (size_t x = 0; x < src.width(); x++) {
                // Get 4x4 grid from cache-friendly buffer
                PixelType grid[4][4];
                window.get4x4(x, grid);

                // Map to original variable names for clarity
                auto I = grid[0][0]; // (x-1, y-1)
                auto E = grid[0][1]; // (x,   y-1)
                auto F = grid[0][2]; // (x+1, y-1)
                auto J = grid[0][3]; // (x+2, y-1)

                auto G = grid[1][0]; // (x-1, y)
                auto A = grid[1][1]; // (x,   y)
                auto B = grid[1][2]; // (x+1, y)
                auto K = grid[1][3]; // (x+2, y)

                auto H = grid[2][0]; // (x-1, y+1)
                auto C = grid[2][1]; // (x,   y+1)
                auto D = grid[2][2]; // (x+1, y+1)
                auto L = grid[2][3]; // (x+2, y+1)

                auto M = grid[3][0]; // (x-1, y+2)
                auto N = grid[3][1]; // (x,   y+2)
                auto O = grid[3][2]; // (x+1, y+2)
                [[maybe_unused]] auto P = grid[3][3]; // (x+2, y+2)

                decltype(A) right_interp, bottom_interp, bottom_right_interp;

                // First filter layer: check for edges (i.e. same colour) along A-D and B-C edge
                // Second filter layer: acquire concrete values for interpolated pixels based on matching of neighbour pixel colours
                if (A == D && B != C) {
                    if ((A == E && B == L) || (A == C && A == F && B != E && B == J)) {
                        right_interp = A;
                    } else {
                        right_interp = mix(A, B, 0.50f);
                    }

                    if ((A == G && C == O) || (A == B && A == H && G != C && C == M)) {
                        bottom_interp = A;
                    } else {
                        bottom_interp = A;
                    }

                    bottom_right_interp = A;
                } else if (A != D && B == C) {
                    if ((B == F && A == H) || (B == E && B == D && A != F && A == I)) {
                        right_interp = B;
                    } else {
                        right_interp = mix(A, B, 0.5f);
                    }

                    if ((C == H && A == F) || (C == G && C == D && A != H && A == I)) {
                        bottom_interp = C;
                    } else {
                        bottom_interp = mix(A, C, 0.5f);
                    }

                    bottom_right_interp = B;
                } else if (A == D && B == C) {
                    if (A == B) {
                        right_interp = bottom_interp = bottom_right_interp = A;
                    } else {
                        right_interp = mix(A, B, 0.5f);
                        bottom_interp = mix(A, C, 0.5f);

                        int8_t majority_accumulator = 0;
                        majority_accumulator += detail::majorityMatch(B, A, G, E);
                        majority_accumulator += detail::majorityMatch(B, A, K, F);
                        majority_accumulator += detail::majorityMatch(B, A, H, N);
                        majority_accumulator += detail::majorityMatch(B, A, L, O);

                        if (majority_accumulator > 0) {
                            bottom_right_interp = A;
                        } else if (majority_accumulator < 0) {
                            bottom_right_interp = B;
                        } else {
                            bottom_right_interp = bilinear_interpolation(A, B, C, D, 0.5f, 0.5f);
                        }
                    }
                } else {
                    bottom_right_interp = bilinear_interpolation(A, B, C, D, 0.5f, 0.5f);

                    if (A == C && A == F && B != E && B == J) {
                        right_interp = A;
                    } else if (B == E && B == D && A != F && A == I) {
                        right_interp = B;
                    } else {
                        right_interp = mix(A, B, 0.5f);
                    }

                    if (A == B && A == H && G != C && C == M) {
                        bottom_interp = A;
                    } else if (C == G && C == D && A != H && A == I) {
                        bottom_interp = C;
                    } else {
                        bottom_interp = mix(A, C, 0.5f);
                    }
                }

                size_t dst_x = scale_factor * x;
                size_t dst_y = scale_factor * y;
                result.set_pixel(dst_x, dst_y, A);
                result.set_pixel(dst_x + 1, dst_y, right_interp);
                result.set_pixel(dst_x, dst_y + 1, bottom_interp);
                result.set_pixel(dst_x + 1, dst_y + 1, bottom_right_interp);
            }
        }
    }

    // Legacy wrapper for backward compatibility
    template<typename InputImage, typename OutputImage>
    OutputImage scale_2x_sai(const InputImage& src, size_t scale_factor = 2) {
        OutputImage result(src.width() * scale_factor, src.height() * scale_factor, src);
        scale_2x_sai(src, result, scale_factor);
        return result;
    }
}
