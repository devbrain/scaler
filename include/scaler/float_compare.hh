#pragma once

namespace scaler {

    /**
     * Float comparison utilities
     *
     * These are intentionally exact comparisons for specific use cases:
     * 1. Scale factors are always exact values (2.0f, 3.0f, 4.0f)
     * 2. Mix optimization checks for exact 0.0 and 1.0
     *
     * We disable float-equal warnings because these comparisons are:
     * - Comparing constants, not computed values
     * - Working correctly as evidenced by all tests passing
     * - More readable than epsilon comparisons for these cases
     */

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

    /**
     * Check if scale factor is exactly 2x
     * Used for algorithm selection where we need exact 2.0f
     */
    inline bool is_scale_2x(float scale) noexcept {
        return scale == 2.0f;
    }

    /**
     * Check if scale factor is exactly 3x
     * Used for algorithm selection where we need exact 3.0f
     */
    inline bool is_scale_3x(float scale) noexcept {
        return scale == 3.0f;
    }

    /**
     * Check if scale factor is exactly 4x
     * Used for algorithm selection where we need exact 4.0f
     */
    inline bool is_scale_4x(float scale) noexcept {
        return scale == 4.0f;
    }

    /**
     * Check if value is exactly zero
     * Used for optimization shortcuts
     */
    template<typename T>
    inline bool is_zero(T value) noexcept {
        return value == static_cast<T>(0);
    }

    /**
     * Check if value is exactly one
     * Used for optimization shortcuts
     */
    template<typename T>
    inline bool is_one(T value) noexcept {
        return value == static_cast<T>(1);
    }

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

} // namespace scaler