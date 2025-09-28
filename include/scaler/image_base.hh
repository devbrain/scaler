#pragma once

#include <scaler/compiler_compat.hh>
#include <scaler/vec3.hh>

namespace scaler {
    enum out_of_bounds_strategy { ZERO, NEAREST };

    // CRTP base class for input images
    template<typename Derived, typename PixelType = uvec3>
    class input_image_base {
        public:
            [[nodiscard]] inline size_t width() const noexcept {
                return static_cast <const Derived*>(this)->width_impl();
            }

            [[nodiscard]] inline size_t height() const noexcept {
                return static_cast <const Derived*>(this)->height_impl();
            }

            [[nodiscard]] inline PixelType get_pixel(size_t x, size_t y) const noexcept {
                return static_cast <const Derived*>(this)->get_pixel_impl(x, y);
            }

            [[nodiscard]] inline PixelType safe_access(int x, int y,
                                                       out_of_bounds_strategy strategy = NEAREST) const noexcept {
                // Optimize for the common case where access is within bounds
                // Use likely/unlikely hints for branch prediction
                const size_t w = width();
                const size_t h = height();

                if (SCALER_LIKELY((x >= 0 && static_cast<size_t>(x) < w) && (y >= 0 && static_cast<size_t>(y) < h))) {
                    return get_pixel(static_cast <size_t>(x), static_cast <size_t>(y));
                }

                // Handle out-of-bounds cases
                switch (strategy) {
                    case ZERO:
                        return {};
                    case NEAREST:
                        x = (x < 0) ? 0 : (static_cast <size_t>(x) >= w) ? (static_cast <int>(w) - 1) : x;
                        y = (y < 0) ? 0 : (static_cast <size_t>(y) >= h) ? (static_cast <int>(h) - 1) : y;
                        return get_pixel(static_cast <size_t>(x), static_cast <size_t>(y));
                }

                return {}; // Unreachable, but keeps compiler happy
            }
    };

    // CRTP base class for output images
    template<typename Derived, typename PixelType = uvec3>
    class output_image_base {
        public:
            [[nodiscard]] size_t width() const {
                return static_cast <const Derived*>(this)->width_impl();
            }

            [[nodiscard]] size_t height() const {
                return static_cast <const Derived*>(this)->height_impl();
            }

            void set_pixel(size_t x, size_t y, const PixelType& pixel) {
                static_cast <Derived*>(this)->set_pixel_impl(x, y, pixel);
            }

            [[nodiscard]] auto& get() {
                return *static_cast <Derived*>(this);
            }

            [[nodiscard]] const auto& get() const {
                return *static_cast <const Derived*>(this);
            }
    };
}
