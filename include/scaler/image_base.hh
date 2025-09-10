#pragma once

#include <algorithm>
#include <scaler/vec3.hh>

enum OutOfBoundsStrategy { ZERO, NEAREST };

// CRTP base class for input images
template<typename Derived, typename PixelType = uvec3>
class InputImageBase {
public:
    [[nodiscard]] int width() const {
        return static_cast<const Derived*>(this)->width_impl();
    }
    
    [[nodiscard]] int height() const {
        return static_cast<const Derived*>(this)->height_impl();
    }
    
    [[nodiscard]] PixelType get_pixel(int x, int y) const {
        return static_cast<const Derived*>(this)->get_pixel_impl(x, y);
    }
    
    [[nodiscard]] PixelType safeAccess(int x, int y, OutOfBoundsStrategy strategy = NEAREST) const {
        bool out_of_bounds = (x < 0 || x >= width()) || (y < 0 || y >= height());
        
        if (out_of_bounds) {
            switch (strategy) {
                case ZERO:
                    return {};
                case NEAREST:
                    x = std::clamp(x, 0, width() - 1);
                    y = std::clamp(y, 0, height() - 1);
                    break;
            }
        }
        
        return get_pixel(x, y);
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