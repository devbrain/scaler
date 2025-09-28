#pragma once

#include <scaler/compiler_compat.hh>
#include <scaler/vec3.hh>
#include <scaler/types.hh>

namespace scaler {
    enum out_of_bounds_strategy { ZERO, NEAREST };

    // CRTP base class for input images
    template<typename Derived, typename PixelType = uvec3>
    class input_image_base {
        public:
            [[nodiscard]] inline dimension_t width() const noexcept {
                return static_cast <const Derived*>(this)->width_impl();
            }

            [[nodiscard]] inline dimension_t height() const noexcept {
                return static_cast <const Derived*>(this)->height_impl();
            }

            [[nodiscard]] inline PixelType get_pixel(index_t x, index_t y) const noexcept {
                return static_cast <const Derived*>(this)->get_pixel_impl(x, y);
            }

            [[nodiscard]] inline PixelType safe_access(coord_t x, coord_t y,
                                                       out_of_bounds_strategy strategy = NEAREST) const noexcept {
                // Optimize for the common case where access is within bounds
                // Use likely/unlikely hints for branch prediction
                const dimension_t w = width();
                const dimension_t h = height();

                if (SCALER_LIKELY(coord_in_bounds(x, w) && coord_in_bounds(y, h))) {
                    return get_pixel(static_cast<index_t>(x), static_cast<index_t>(y));
                }

                // Handle out-of-bounds cases
                switch (strategy) {
                    case ZERO:
                        return {};
                    case NEAREST:
                        const coord_t max_x = dim_to_coord(w) - 1;
                        const coord_t max_y = dim_to_coord(h) - 1;
                        x = clamp_coord(x, 0, max_x);
                        y = clamp_coord(y, 0, max_y);
                        return get_pixel(static_cast<index_t>(x), static_cast<index_t>(y));
                }

                return {}; // Unreachable, but keeps compiler happy
            }
    };

    // CRTP base class for output images
    template<typename Derived, typename PixelType = uvec3>
    class output_image_base {
        public:
            [[nodiscard]] dimension_t width() const {
                return static_cast <const Derived*>(this)->width_impl();
            }

            [[nodiscard]] dimension_t height() const {
                return static_cast <const Derived*>(this)->height_impl();
            }

            void set_pixel(index_t x, index_t y, const PixelType& pixel) {
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
