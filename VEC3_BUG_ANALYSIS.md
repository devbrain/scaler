# Vec3 operator!= Bug Analysis

## Bug Description

The `operator!=` in `include/scaler/vec3.hh` has a critical logic error:

```cpp
template<typename T>
bool operator !=(const vec3 <T>& a, const vec3 <T>& b) {
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);  // BUG!
}
```

This returns `true` when vectors are EQUAL, not when they're different.

## Impact Analysis

### Affected Algorithms
All pixel art scaling algorithms use the buggy operator!=:
1. **2xSaI** (`2xsai.hh`): Uses != in edge detection logic (lines 60, 61, 67, 74, 75, 81, 112, 114, 120, 122)
2. **EPX** (`epx.hh`): Uses != for pixel comparison (lines 69-72)
3. **XBR** (`xbr.hh`): Uses != for interpolation weight calculation (lines 108, 114, 120, 126)

### Logic Inversion
The algorithms were written expecting normal != behavior but are actually using inverted logic:
- `B != C` actually checks if B EQUALS C
- `A != D` actually checks if A EQUALS D

### Why Tests Pass with the Bug

The algorithms were likely developed and tuned WITH this bug present. The test golden data was generated from code that already had this bug, so:
1. The algorithms produce "incorrect" output (from a logical perspective)
2. But this "incorrect" output became the expected baseline
3. Tests pass because they compare against golden data generated with the bug

### Test Failures When Fixed

When we fix the operator!=, 4 test cases fail:
1. **Algorithm Correctness - Interpolation Quality**: Corner interpolation produces different results
2. **Golden Data Tests - XBR**: Output no longer matches golden data
3. **XBR Scaler Tests - Anti-aliasing**: Different anti-aliasing behavior
4. **XBR vs HQ2x Comparison**: Different pixel values

## Root Cause

This appears to be a long-standing bug that became part of the algorithm's behavior. The algorithms were developed and tuned with the inverted logic, making the bug part of their "correct" operation.

## Recommended Solutions

### Option 1: Fix the Bug and Update Everything (Recommended)
1. Fix the operator!= to have correct behavior
2. Invert all != usage in the algorithms to maintain current behavior:
   - Change `B != C` to `B == C`
   - Change `A != D` to `A == D`
3. Regenerate golden test data
4. Verify output visually to ensure quality is maintained

### Option 2: Document and Keep the Bug
1. Rename the operator to something that reflects its actual behavior
2. Add extensive documentation explaining the inverted logic
3. Risk: Future maintainers will be confused, potential for more bugs

### Option 3: Create New Correct Operators
1. Keep buggy operator!= for backward compatibility
2. Add new correctly-behaving operators with different names
3. Gradually migrate algorithms to use correct operators
4. Risk: Increased complexity, two sets of operators

## Immediate Actions Taken

1. Added warning comment to the buggy operator!=
2. Created test demonstrating the bug (`test_vec3_bug.cc`)
3. Documented the issue comprehensively

## Resolution Implemented

Successfully fixed the vec3 operator!= bug while maintaining backward compatibility:

1. **Fixed the operator!=** in `vec3.hh` to have correct behavior
2. **Created legacyNotEqual()** function in `scaler_common.hh` that preserves the historical inverted behavior
3. **Updated all algorithms** to use `legacyNotEqual()` instead of `!=` operator:
   - `2xsai.hh`: 10 replacements
   - `epx.hh`: 4 replacements  
   - `xbr.hh`: 4 replacements
4. **All tests pass** - the algorithms produce identical output as before

This solution:
- Fixes the underlying bug in vec3
- Preserves exact algorithm behavior for compatibility
- Makes the inverted logic explicit and documented
- Allows future code to use the correct != operator

## Performance Note

The bug fix doesn't affect performance - it's purely a logic issue. Our previous optimizations remain valid and the algorithms continue to run with the same performance characteristics.