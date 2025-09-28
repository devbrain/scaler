#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace scaler {

    /**
     * Semantic type aliases for image dimensions and coordinates
     *
     * These types clarify intent and reduce casting errors:
     * - dimension_t: For width, height (always non-negative)
     * - coord_t: For coordinates that may be out-of-bounds (signed)
     * - index_t: For validated array indices (always valid)
     */

    // Dimensions (width, height) - always non-negative
    using dimension_t = std::size_t;

    // Coordinates - may be negative for out-of-bounds handling
    using coord_t = std::ptrdiff_t;

    // Array indices - always valid, non-negative
    using index_t = std::size_t;

    // Scale factors
    using scale_t = float;

    // Compile-time checks to ensure types have expected properties
    static_assert(std::is_unsigned_v<dimension_t>,
                  "Dimensions must be unsigned");
    static_assert(std::is_signed_v<coord_t>,
                  "Coordinates must be signed for out-of-bounds handling");
    static_assert(std::is_unsigned_v<index_t>,
                  "Indices must be unsigned");

    /**
     * Helper functions for safe type conversions
     */

    // Safe conversion from coordinate to index (with bounds checking)
    inline index_t coord_to_index(coord_t coord) {
        return coord >= 0 ? static_cast<index_t>(coord) : 0;
    }

    // Clamp coordinate to valid range
    inline coord_t clamp_coord(coord_t coord, coord_t min, coord_t max) {
        return coord < min ? min : (coord > max ? max : coord);
    }

    // Convert dimension to coordinate (for calculations)
    inline coord_t dim_to_coord(dimension_t dim) {
        return static_cast<coord_t>(dim);
    }

    // Check if coordinate is within bounds
    inline bool coord_in_bounds(coord_t coord, dimension_t dim) {
        return coord >= 0 && static_cast<dimension_t>(coord) < dim;
    }

} // namespace scaler