#pragma once

#include <algorithm>
#include <scaler/compiler_compat.hh>
#include <scaler/vec3.hh>

enum OutOfBoundsStrategy { ZERO, NEAREST };

// CRTP base class for input images
template<typename Derived, typename PixelType = uvec3>
class InputImageBase {
public:
    [[nodiscard]] inline int width() const noexcept {
        return static_cast<const Derived*>(this)->width_impl();
    }
    
    [[nodiscard]] inline int height() const noexcept {
        return static_cast<const Derived*>(this)->height_impl();
    }
    
    [[nodiscard]] inline PixelType get_pixel(int x, int y) const noexcept {
        return static_cast<const Derived*>(this)->get_pixel_impl(x, y);
    }
    
    [[nodiscard]] inline PixelType safeAccess(int x, int y, OutOfBoundsStrategy strategy = NEAREST) const noexcept {
        // Optimize for the common case where access is within bounds
        // Use likely/unlikely hints for branch prediction
        const int w = width();
        const int h = height();
        
        if (SCALER_LIKELY((x >= 0 && x < w) && (y >= 0 && y < h))) {
            return get_pixel(x, y);
        }
        
        // Handle out-of-bounds cases
        switch (strategy) {
            case ZERO:
                return {};
            case NEAREST:
                x = (x < 0) ? 0 : (x >= w) ? (w - 1) : x;
                y = (y < 0) ? 0 : (y >= h) ? (h - 1) : y;
                return get_pixel(x, y);
        }
        
        return {};  // Unreachable, but keeps compiler happy
    }
};

// CRTP base class for output images
template<typename Derived, typename PixelType = uvec3>
class OutputImageBase {
public:
    [[nodiscard]] int width() const {
        return static_cast<const Derived*>(this)->width_impl();
    }
    
    [[nodiscard]] int height() const {
        return static_cast<const Derived*>(this)->height_impl();
    }
    
    void set_pixel(int x, int y, const PixelType& pixel) {
        static_cast<Derived*>(this)->set_pixel_impl(x, y, pixel);
    }
    
    [[nodiscard]] auto& get() {
        return *static_cast<Derived*>(this);
    }
    
    [[nodiscard]] const auto& get() const {
        return *static_cast<const Derived*>(this);
    }
};