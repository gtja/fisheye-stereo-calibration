# Testing Plan for Rectification Optimizations

## Overview

This document outlines the testing strategy for validating the stereo rectification optimizations implemented in this PR.

## Changes to Test

1. **Rectification strength adjustment** (double_sphere.h)
   - Reduced from 0.75 to 0.50-0.55 for wide-angle lenses
   - Target: ~120° rectified FOV instead of ~150°

2. **Outlier frame filtering** (calibrate_ds.cpp)
   - Per-frame reprojection error calculation
   - Filtering threshold: 1.5 pixels
   - Safety check: minimum 3 frames

## Test Strategy

### 1. Compilation Test

**Objective**: Ensure code compiles without errors

```bash
cd /home/runner/work/fisheye-stereo-calibration/fisheye-stereo-calibration
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

**Expected result**:
- No compilation errors
- Executables created: `calibrate`, `calibrate_ds`, `test_rectification`

### 2. Unit Test (Manual Code Review)

**Objective**: Verify logic correctness

**Test cases**:

#### A. Rectification Strength Selection
```cpp
// Input: approx_fov_deg = 180°
// Expected: rect_strength = 0.50 (matches FOV > 170° condition)

// Input: approx_fov_deg = 140°
// Expected: rect_strength = 0.55 (matches FOV > 130° condition)

// Input: approx_fov_deg = 110°
// Expected: rect_strength = 0.70 (matches FOV > 100° condition)

// Input: approx_fov_deg = 90°
// Expected: rect_strength = 1.0 (default, no matching condition)
```

✓ **Verified**: Logic is correct in double_sphere.h lines 792-807

#### B. Outlier Filtering Logic
```cpp
// Test case 1: All frames good (error <= 1.5 px)
// Expected: good_frame_indices contains all frame indices

// Test case 2: Some frames bad (error > 1.5 px)
// Expected: good_frame_indices contains only good frames

// Test case 3: Too many frames filtered (< 3 good)
// Expected: Fallback to keep all frames
```

✓ **Verified**: Logic is correct in calibrate_ds.cpp lines 626-698

### 3. Integration Test with Sample Data

**Objective**: Verify end-to-end functionality

```bash
cd build
./calibrate_ds -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o test_output.yml
```

**Expected output sections**:

#### A. Outlier Filtering Output
```
Step 4.1: Filtering outlier frames before Bundle Adjustment...
  Filtering out frame X with avg reprojection error Y.YYY px (> 1.5 px)
Kept N / M frames after outlier filtering (threshold: 1.5 px)
```

**Validation criteria**:
- At least 80% of frames should be kept
- If < 50% kept, investigate image quality
- No crash or segfault

#### B. Bundle Adjustment Output
```
[LOG] Adding residuals for filtered observations...
[LOG] Added residuals for image pair X
[LOG] Adding stereo extrinsics constraints...
[LOG] Adding stereo correspondence constraints...
```

**Validation criteria**:
- Only good frame indices appear
- Number of "Added residuals" messages matches number of good frames
- Optimization converges successfully

#### C. Rectification Error Output
```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200 (original: 1600x1200, avg_fx: 454.0)
   Evaluated N corner points
   Average y-difference: X.XXXX pixels [threshold: < 0.3 pixel]
   Maximum y-difference: Y.YYYY pixels [threshold: < 0.7 pixel]
   Status: PASS/FAIL

Rectification diagnostic information (only ZZ.ZZ% of points were valid):
   Total corner points: 352
   Valid points: N (XX.XX%)
   Points behind camera: M (YY.YY%)
   Points with negative Z after rectification: K (ZZ.ZZ%)
   Points outside rectified image bounds: L (WW.WW%)
   Estimated FOV: XXX.XXX° (rectification strength: 0.XX)
```

**Validation criteria**:
- Average y-difference < 0.5 pixels (target: < 0.3)
- Maximum y-difference < 1.0 pixels (target: < 0.7)
- Valid point ratio > 20% (target: > 25%)
- Negative Z points < 10% (target: < 5%)
- Rectification strength matches expected value (0.50 or 0.55 for wide-angle)
- Status = PASS

### 4. Regression Test

**Objective**: Ensure changes don't break existing functionality

**Test scenarios**:

#### A. Standard Lens (FOV < 100°)
- Should not be affected by rectification strength changes
- Outlier filtering should work normally
- All existing tests should pass

#### B. Moderate Wide-Angle (100° < FOV < 130°)
- rect_strength = 0.70 (was 0.80, minor adjustment)
- Should maintain similar or better accuracy

#### C. Extreme Wide-Angle (FOV > 170°)
- rect_strength = 0.50 (was 0.75, significant change)
- Should see dramatic improvement in valid point ratio
- Should see significant reduction in negative Z points

### 5. Performance Test

**Objective**: Ensure filtering doesn't significantly impact runtime

**Measurement**:
```bash
time ./calibrate_ds -w 9 -h 6 -s 0.02423 -d ../imgs/ -l left -r right -o test_output.yml
```

**Expected**:
- Additional time for filtering: < 1 second
- Total runtime increase: < 5%
- Most time still spent in bundle adjustment

### 6. Edge Cases

#### A. All Frames Bad (unlikely)
```
Scenario: All frames have error > 1.5 px
Expected behavior: Warning message, keep all frames (fallback)
```

#### B. Very Few Frames (< 5)
```
Scenario: Only 3-4 frames available
Expected behavior: Keep all frames if filtering would drop below 3
```

#### C. Perfect Frames (all error = 0)
```
Scenario: All frames have error = 0 (theoretical)
Expected behavior: Keep all frames, normal optimization
```

## Success Criteria

### Primary Metrics (Must Pass)
1. ✅ Code compiles without errors
2. ✅ No runtime crashes or segfaults
3. ✅ Average y-difference < 1.0 pixels (target: < 0.3)
4. ✅ Maximum y-difference < 2.0 pixels (target: < 0.7)
5. ✅ Valid point ratio > 15% (target: > 25%)

### Secondary Metrics (Should Pass)
6. ⚠️ Negative Z points < 15% (target: < 5%)
7. ⚠️ Points out of bounds < 50% (target: < 30%)
8. ⚠️ Rectification strength matches expected (0.50-0.55 for wide-angle)
9. ⚠️ At least 70% of frames kept after filtering

### Tertiary Metrics (Nice to Have)
10. 🎯 Average y-difference < 0.3 pixels
11. 🎯 Maximum y-difference < 0.7 pixels
12. 🎯 Valid point ratio > 25%
13. 🎯 Negative Z points < 5%

## Test Execution Checklist

- [ ] 1. Compilation test passed
- [ ] 2. Unit test (code review) completed
- [ ] 3. Integration test with sample data
  - [ ] 3A. Outlier filtering output verified
  - [ ] 3B. Bundle adjustment output verified
  - [ ] 3C. Rectification error output verified
- [ ] 4. Regression test
  - [ ] 4A. Standard lens test
  - [ ] 4B. Moderate wide-angle test
  - [ ] 4C. Extreme wide-angle test
- [ ] 5. Performance test completed
- [ ] 6. Edge cases tested
  - [ ] 6A. All frames bad scenario
  - [ ] 6B. Very few frames scenario
  - [ ] 6C. Perfect frames scenario

## Known Limitations

1. **Outlier filtering threshold (1.5 px)**: Fixed value, not adaptive
   - May need adjustment for different camera setups
   - Consider making it a parameter in future versions

2. **Minimum frame requirement (3)**: Hard-coded safety check
   - May be too conservative for some scenarios
   - May be insufficient for others

3. **Rectification strength values**: Based on FOV estimation
   - FOV estimation may not be perfect for all lens models
   - Consider more sophisticated adaptive strength selection

## Troubleshooting Guide

### Issue: Too Many Frames Filtered
**Symptoms**: Warning "Only X frames passed filtering"
**Causes**:
- Poor image quality (blur, motion)
- Incorrect camera model initialization
- Wrong checkerboard size parameters

**Solutions**:
1. Review image quality
2. Try re-running with different images
3. Check calibration parameters (board size, square size)

### Issue: Rectification Error Still High
**Symptoms**: Average y-difference > 10 pixels
**Causes**:
- Incorrect stereo extrinsics (R, T)
- Bundle adjustment not converging
- Outlier frames still present

**Solutions**:
1. Check bundle adjustment convergence
2. Verify stereo constraints are being applied
3. Manually review filtered frames

### Issue: Valid Point Ratio Still Low (< 10%)
**Symptoms**: "Valid points: N (< 10%)"
**Causes**:
- Rectification FOV still too wide
- Image size too small
- Virtual focal length incorrect

**Solutions**:
1. Try further reducing rectification strength (e.g., 0.45)
2. Check rectified image size
3. Verify virtual focal length calculation

## References

- Implementation: RECTIFICATION_OPTIMIZATION_2025.md
- Problem statement: Original issue describing 246 px error
- Verification guide: VALIDATION_GUIDE.md
