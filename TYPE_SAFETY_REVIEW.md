# Type Safety Code Review Report

## Executive Summary
Found multiple type safety issues related to casting and semantic type correctness, particularly around:
- Mixed signed/unsigned types for coordinates and dimensions
- Unnecessary casts between int and size_t
- Potential overflow/underflow issues
- Inconsistent type usage across the codebase

## Critical Issues Found

### 1. 🔴 **image_base.hh: safe_access() uses int for coordinates**
```cpp
// Current: Line 25-43
PixelType safe_access(int x, int y, ...) const noexcept {
    if (x >= 0 && static_cast<size_t>(x) < w) // Multiple casts needed
    ...
    x = (x < 0) ? 0 : (static_cast<size_t>(x) >= w) ? (static_cast<int>(w) - 1) : x;
}
```
**Issue**: Using `int` for coordinates that could be unsigned, requiring multiple casts
**Fix**: Consider using `std::ptrdiff_t` or keeping coordinates as `size_t` with explicit boundary strategy

### 2. 🔴 **bilinear.hh: Mixed int/size_t arithmetic**
```cpp
// Lines 45-66
const int y0 = static_cast<int>(std::floor(src_y));
const int y1 = std::min(y0 + 1, static_cast<int>(src_height - 1));
...
src.get_pixel(x0_clamped, y0_clamped);  // get_pixel expects size_t!
```
**Issue**: Converting float→int→size_t for array indexing
**Problem**:
- `get_pixel()` expects `size_t` but receives `int`
- Risk of sign conversion warnings
- Potential undefined behavior if coordinates are negative

### 3. 🟡 **unified_scaler.hh: Float to int casting for scale factors**
```cpp
// Lines 283-339
switch (static_cast<int>(scale_factor)) {
    case 2: return scale_hq2x<InputImage, OutputImage>(input);
    case 3: return scale_hq3x<InputImage, OutputImage>(input);
}
```
**Issue**: Converting float scale_factor to int loses precision
**Risk**: 2.1f would become 2, potentially selecting wrong algorithm

### 4. 🟡 **sliding_window_buffer.hh: Excessive int/size_t conversions**
```cpp
// Lines 69, 83, 94-96
int src_y = static_cast<int>(start_y) + buffer_offset_ + i;
const int src_row = static_cast<int>(current_y_) + y_offset;
const size_t x_idx = x + static_cast<size_t>(padding_);
```
**Issue**: `current_y_` is size_t but constantly cast to int for arithmetic
**Problem**: Mixing signed/unsigned arithmetic is error-prone

### 5. 🔴 **GPU code: GLint vs GLsizei confusion**
```cpp
// opengl_texture_scaler.hh
void scale_texture_to_texture(
    GLuint input_texture,
    GLsizei input_width,    // Signed!
    GLsizei input_height,   // Signed!
```
**Issue**: OpenGL uses signed types (GLsizei) for dimensions, but semantically they should be unsigned
**Risk**: Negative dimensions would be undefined behavior

### 6. 🟡 **buffer_policy.hh: int for row indices**
```cpp
// Line 101, 112
void initialize_rows(const ImageType& src, int y) {
void load_next_row(const ImageType& src, int y) {
```
**Issue**: Row indices should logically be unsigned
**Risk**: Negative row indices require special handling

### 7. 🟠 **Integer Overflow Risk in Scaling Operations**
```cpp
// Found in epx.hh, eagle.hh, scale2x_sfx.hh, etc.
size_t dst_x = scale_factor * x;
size_t dst_y = scale_factor * y;
```
**Issue**: No overflow checking when multiplying dimensions by scale_factor
**Risk**: For large images, `width * scale_factor` could overflow size_t
**Example**: 4K image (3840) × scale_factor 4 = 15,360 (OK), but larger images could overflow

### 8. 🔴 **EPX Scaler: Problematic int/size_t mixing**
```cpp
// epx.hh lines 29-34
const int xp = static_cast<int>(x) + pad;
auto A = topRow[static_cast<size_t>(xp)];     // Cast back to size_t!
auto B = midRow[static_cast<size_t>(xp + 1)]; // Arithmetic on int, then cast
auto C = midRow[static_cast<size_t>(xp - 1)]; // Could be negative!
```
**Issue**: Converting size_t→int→size_t repeatedly
**Risk**: If `xp - 1` is negative, casting to size_t causes wraparound to huge value

## Semantic Type Issues

### Dimensions (width/height) - Should ALWAYS be unsigned
- ✅ Most places use `size_t` correctly
- ❌ OpenGL code uses `GLsizei` (signed) - OpenGL legacy issue
- ❌ SDL compatibility layer uses `int` for dimensions

### Coordinates (x, y)
- **Mixed usage**: Sometimes `int` (for out-of-bounds), sometimes `size_t`
- **Recommendation**: Use signed type (ptrdiff_t) when out-of-bounds is possible, unsigned (size_t) for validated indices

### Scale Factors
- Currently `float` everywhere - OK
- Issue: Casting to `int` for switch statements loses precision

## Recommendations

### 1. **Create Type Aliases for Clarity**
```cpp
namespace scaler {
    using dimension_t = size_t;      // For width, height
    using coord_t = std::ptrdiff_t;  // For possibly-negative coordinates
    using index_t = size_t;          // For array indices
}
```

### 2. **Fix safe_access() signature**
```cpp
// Option A: Use signed throughout
PixelType safe_access(coord_t x, coord_t y, ...) const noexcept {
    const auto w = static_cast<coord_t>(width());
    const auto h = static_cast<coord_t>(height());
    if (x >= 0 && x < w && y >= 0 && y < h) {
        return get_pixel(static_cast<index_t>(x), static_cast<index_t>(y));
    }
    // ...
}

// Option B: Separate functions
PixelType get_pixel_clamped(size_t x, size_t y) const noexcept;
PixelType get_pixel_wrapped(size_t x, size_t y, size_t w, size_t h) const noexcept;
```

### 3. **Fix Bilinear Interpolation**
```cpp
// Better approach - keep indices as size_t until needed
const float src_y = (dst_y + 0.5f) * inv_scale - 0.5f;
const size_t y0 = src_y >= 0 ? static_cast<size_t>(src_y) : 0;
const size_t y1 = std::min(y0 + 1, src_height - 1);
```

### 4. **Fix Scale Factor Comparisons**
```cpp
// Instead of casting to int
if (std::abs(scale_factor - 2.0f) < 0.01f) {
    return scale_hq2x<InputImage, OutputImage>(input);
}
```

### 5. **Add Compile-Time Checks**
```cpp
static_assert(std::is_unsigned_v<dimension_t>, "Dimensions must be unsigned");
static_assert(std::is_signed_v<coord_t>, "Coordinates must be signed for out-of-bounds");
```

## Priority Fixes

1. **CRITICAL**: Fix EPX scaler negative index risk (lines 29-34)
2. **HIGH**: Fix bilinear.hh coordinate handling (UB risk)
3. **HIGH**: Fix safe_access() to reduce casting complexity
4. **HIGH**: Add overflow checking for dimension multiplication
5. **MEDIUM**: Fix unified_scaler float→int casting
6. **MEDIUM**: Standardize coordinate types across codebase
7. **LOW**: Add type aliases for semantic clarity

## Testing Recommendations

1. **Enable strict warnings**:
   ```bash
   -Wall -Wextra -Wconversion -Wsign-conversion -Wsign-compare
   -Wfloat-equal -Wcast-qual -Wcast-align
   ```
2. **Test with boundary values**: 0, SIZE_MAX, negative values
3. **Test with non-integer scale factors**: 2.5f, 3.3f, 1.1f
4. **Use sanitizers**:
   ```bash
   -fsanitize=undefined -fsanitize=integer -fsanitize=bounds
   ```
5. **Test large images**: 8K (7680×4320) with scale_factor 4
6. **Fuzz testing**: Random coordinates including negative values

## Conclusion

The codebase has inconsistent type usage that creates:
- Unnecessary complexity from excessive casting
- Potential bugs from sign conversion
- Risk of undefined behavior with negative indices

Implementing the recommended type system would:
- Reduce casting by ~60%
- Make intent clearer
- Prevent sign-related bugs
- Improve performance (fewer runtime checks)