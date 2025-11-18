# Fisheye Stereo Calibration Best Practices Guide

This comprehensive guide provides techniques to achieve calibration accuracy below 0.3 pixels reprojection error for fisheye stereo camera systems.

## Table of Contents

1. [Input Data Quality](#1-input-data-quality)
2. [Model Configuration](#2-model-configuration)
3. [Image Quantity and Poses](#3-image-quantity-and-poses)
4. [Stereo Joint Optimization](#4-stereo-joint-optimization)
5. [Subpixel Corner Refinement](#5-subpixel-corner-refinement)
6. [Rectification Parameters](#6-rectification-parameters)
7. [Temperature and Mechanical Stability](#7-temperature-and-mechanical-stability)
8. [Extreme Wide-Angle Lenses](#8-extreme-wide-angle-lenses)
9. [Automatic Bad Frame Filtering](#9-automatic-bad-frame-filtering)
10. [Advanced Optimization](#10-advanced-optimization)

## Quick Checklist

Print this checklist and verify each item before and during calibration:

- [ ] Delete blurry images (Laplacian σ² > 100)
- [ ] Verify ≥100 corners detected per image
- [ ] Ensure corners reach edge regions (within 20% border)
- [ ] Verify checkerboard physical dimensions (±0.1 mm)
- [ ] Use fisheye model with all 4 distortion coefficients (k1-k4)
- [ ] Capture 30-40 images (minimum 20)
- [ ] Include edge positions (board touching 4 sides at 80%)
- [ ] Include tilted poses (±30° around 3 axes)
- [ ] Include rotated poses (90° rotation around optical axis)
- [ ] Use stereoCalibrate joint optimization (not separate mono calibration)
- [ ] Enable subpixel refinement with appropriate window size
- [ ] Set rectification alpha=0.7-0.8 (not 0.0 or 1.0)
- [ ] Test without CALIB_ZERO_DISPARITY flag
- [ ] Allow 30 min thermal stabilization in constant temperature
- [ ] Lock and seal focus ring after focusing
- [ ] Remove frames with per-view error > 0.5 pixels

**Target: All items checked → reprojection error < 0.3 pixels**

---

## 1. Input Data Quality

### 90% of calibration errors come from bad input data!

#### 1.1 Image Sharpness Check

**Problem**: Blurry images lead to inaccurate corner detection.

**Solution**: Use Laplacian variance to detect blur.

```bash
# Check all images for blur
python3 utils/laplacian_var.py imgs/ --threshold 100

# Delete blurry images (backup first!)
python3 utils/laplacian_var.py imgs/ --threshold 100 --delete
```

**Thresholds** (for 8-bit grayscale):
- Laplacian σ² > 100: Minimum acceptable
- Laplacian σ² > 150: High quality
- Laplacian σ² < 100: Delete image

**Impact**: Can reduce error by 30-50%

#### 1.2 Corner Detection Quality

**Problem**: Missing or poorly distributed corners.

**Requirements**:
- ≥100 valid corners per image (for typical checkerboard)
- Corners must reach edge regions (within 20% border from each edge)
- At least 3 out of 4 edges should have corners

```bash
# Analyze corner detection quality
python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix left

# Save visualizations to verify
python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix left --save
```

**If corners don't reach edges**:
- Recapture images with board closer to camera edges
- Move board to touch left/right/top/bottom edges (80% of frame)
- These edge images are critical for fisheye distortion modeling

#### 1.3 Checkerboard Physical Dimensions

**Problem**: Incorrect square size causes systematic calibration error.

**Solution**: 
1. Use precision calipers to measure 10×10 squares
2. Calculate average square size
3. Physical measurement error must be < 0.1 mm
4. Use the measured value for the `--square_size` / `-s` parameter

**Example**:
```bash
# If 10 squares = 242.3 mm, then square_size = 0.02423 m
./calibrate -w 9 -h 6 -s 0.02423 -n 30 ...
```

#### 1.4 Fisheye vs Pinhole Model

**Critical**: Always use `cv::fisheye::calibrate` and `cv::fisheye::stereoCalibrate` for fisheye lenses!

**Do NOT use**:
- `cv::calibrateCamera` (pinhole model)
- `CALIB_RATIONAL_MODEL` flag (pinhole-specific)
- Any other pinhole calibration functions

**Why**: Fisheye lenses have extreme radial distortion that cannot be modeled by pinhole camera assumptions.

---

## 2. Model Configuration

### Distortion Coefficients

**For fisheye lenses**: Use all 4 distortion coefficients (k1, k2, k3, k4)

```cpp
// GOOD: All coefficients enabled (default)
flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
flag |= cv::fisheye::CALIB_FIX_SKEW;
// DO NOT FIX K2, K3, K4 for fisheye!

// BAD: Fixing higher-order terms
flag |= cv::fisheye::CALIB_FIX_K3;  // ✗ Don't do this!
flag |= cv::fisheye::CALIB_FIX_K4;  // ✗ Don't do this!
```

**Why**: k3 and k4 are essential for modeling distortion at the lens edges where fisheye distortion is most extreme. For 180° FOV lenses, these terms can reduce edge error by 50%.

### Principal Point Optimization

**Allow principal point to be optimized** (don't fix to image center)

```cpp
// GOOD: Principal point optimized
// (default - don't add CALIB_FIX_PRINCIPAL_POINT)

// BAD: Fixed to center
flag |= cv::fisheye::CALIB_FIX_PRINCIPAL_POINT;  // ✗ Don't do this for fisheye!
```

**Why**: Wide-angle lenses often have optical center offset from geometric image center. Fixing it prevents the model from accurately describing edge distortion.

### Higher-Order Models

For extreme fisheye lenses (> 190° FOV):
- OpenCV `cv::omnidir` (MEI model)
- Kannala-Brandt model (KB6 with 6 distortion coefficients)
- These can achieve < 0.2 pixel error for extreme lenses

---

## 3. Image Quantity and Poses

### Minimum vs Recommended

- **Minimum**: 20 images
- **Recommended**: 30-40 images
- **Extreme fisheye (>200° FOV)**: 50-60 images

### Required Poses

#### Edge Coverage (Critical!)
The checkerboard must appear at:
- **Left edge**: Board touching 80% of left side
- **Right edge**: Board touching 80% of right side  
- **Top edge**: Board touching 80% of top
- **Bottom edge**: Board touching 80% of bottom

#### Rotation Variety
- **Tilt around X-axis**: ±30° (board tilted up/down)
- **Tilt around Y-axis**: ±30° (board tilted left/right)
- **Tilt around Z-axis**: ±30° (board rotated in-plane)
- **90° rotation**: Rotate board 90° around optical axis (portrait → landscape)

#### Distance Variety
- Near distance: ~0.3-0.5 m
- Mid distance: ~0.7-1.0 m
- Far distance: ~1.5-2.0 m

### If Limited to 15 Images

**Augmentation technique**: Take the same pose but translate the board 5 cm in any direction and capture again. This creates slightly different corner positions, effectively increasing your dataset size.

---

## 4. Stereo Joint Optimization

### ⚠️ Common Mistake: Separate Mono Calibration

**Wrong approach**:
```cpp
// ✗ Don't do this!
cv::fisheye::calibrate(..., K1, D1, ...);  // Calibrate left alone
cv::fisheye::calibrate(..., K2, D2, ...);  // Calibrate right alone
// Then use K1, D1, K2, D2 as final result
```

**Correct approach**:
```cpp
// ✓ Do this instead!
// Step 1: Get initial intrinsic guesses from mono calibration
cv::fisheye::calibrate(..., K1, D1, ...);
cv::fisheye::calibrate(..., K2, D2, ...);

// Step 2: Joint stereo optimization (refines everything together)
cv::fisheye::stereoCalibrate(object_points, left_img_points, right_img_points,
    K1, D1, K2, D2, img_size, R, T, 
    cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC | cv::fisheye::CALIB_FIX_SKEW,
    cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 1e-5));
```

**Why**: Stereo calibration jointly optimizes:
- Left intrinsics (K1, D1)
- Right intrinsics (K2, D2)
- Extrinsics (R, T)

This ensures all parameters are consistent and optimal for the stereo pair, rather than two independent monocular calibrations.

### Calibration Flags

```cpp
int flag = 0;
flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;  // Recompute extrinsics each iteration
flag |= cv::fisheye::CALIB_FIX_SKEW;             // Assume zero skew (usually valid)

// ✓ Allow all 4 distortion coefficients
// ✓ Allow principal point optimization
// ✗ Don't use CALIB_FIX_INTRINSIC (we want joint optimization)
```

---

## 5. Subpixel Corner Refinement

### Default vs Improved

**Default** (original code):
```cpp
cv::cornerSubPix(gray, corners, cv::Size(5, 5), cv::Size(-1, -1),
    cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.1));
```
Corner jitter: ~0.1 pixels

**Improved** (current code):
```cpp
cv::cornerSubPix(gray, corners, cv::Size(5, 5), cv::Size(-1, -1),
    cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.01));
```
Corner jitter: ~0.02 pixels

**For large checkerboards** (> A3 size):
```cpp
cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
    cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.01));
```
Larger window (11×11) handles larger features better.

**Impact**: Can reduce reprojection error by 0.05-0.1 pixels.

---

## 6. Rectification Parameters

### Alpha Parameter

**Purpose**: Controls the trade-off between valid pixels and image cropping.

**Values**:
- `alpha = 0.0`: Maximum crop, all pixels valid, smallest ROI
- `alpha = 1.0`: No crop, all original pixels retained, but edge regions may be invalid
- **`alpha = 0.7-0.8`**: **Recommended** - good balance

**Current implementation**:
```cpp
double alpha = 0.8;
cv::fisheye::stereoRectify(K1, D1, K2, D2, img_size, R, T, R1, R2, P1, P2, Q,
    0,  // flags (disable CALIB_ZERO_DISPARITY)
    img_size, alpha, 1.1);
```

### CALIB_ZERO_DISPARITY Flag

**Experiment**: Try with and without this flag to see which gives better results.

```cpp
// Without (current implementation - often better)
cv::fisheye::stereoRectify(..., 0, img_size, alpha, 1.1);

// With (try if rectification error is high)
cv::fisheye::stereoRectify(..., cv::CALIB_ZERO_DISPARITY, img_size, alpha, 1.1);
```

Disabling CALIB_ZERO_DISPARITY can reduce rectification error by up to 30% in some cases.

---

## 7. Temperature and Mechanical Stability

### Thermal Expansion

**Problem**: Material thermal expansion causes dimensional changes.

**Example**: PLA checkerboard + aluminum frame
- 1°C temperature change → 0.05 mm expansion
- 30°C temperature difference → 1.5 mm error
- This can increase mono error from 0.2 → 0.4 pixels

**Solution**:
1. Use thermally stable materials:
   - Glass + aluminum foil checkerboard
   - Anodized aluminum checkerboard
   - Carbon fiber frame
2. Temperature stabilization:
   - Place camera and checkerboard in same room
   - Wait 30-60 minutes before calibration
   - Keep temperature constant (±2°C) during capture

### Mechanical Stability

**Problem**: Lens focus ring can rotate during transport, changing focus.

**Solution**:
1. Set focus before calibration
2. **Lock focus ring with adhesive tape**
3. **Apply thread locker or glue** to prevent rotation
4. Mark lens position for verification

**Impact**: Defocus of 0.5° can increase error by 0.2+ pixels.

---

## 8. Extreme Wide-Angle Lenses

### For > 200° FOV or f < 1.5 mm

**Standard fisheye model may be insufficient**. Consider:

1. **OpenCV omnidir** (MEI model)
   ```cpp
   cv::omnidir::calibrate(...)
   ```

2. **Kannala-Brandt (KB6)** model (6 distortion coefficients)
   - Available in Kalibr or Basalt
   - Can achieve < 0.2 pixel error for extreme lenses

### Capture Strategy

- Capture 50-60 images (vs 30-40 for normal fisheye)
- **Critical**: Place board very close to lens (3-5 cm from lens hood)
- This ensures outermost 10% of pixels get corner coverage
- Without this, edge regions will have poor calibration

---

## 9. Automatic Bad Frame Filtering

### Per-View Error Analysis

Some frames contribute disproportionately to calibration error. Automatically detect and remove them:

**Method**:
1. Perform initial calibration
2. Calculate per-view reprojection errors
3. Remove frames with error > threshold (typically 0.4-0.6 pixels)
4. Recalibrate with filtered dataset

**Current implementation**: Built into the calibration program (filters frames with error > 0.5 pixels if ≥20 good frames remain)

**Expected impact**: Can reduce error from 0.35 → 0.2 pixels

**⚠️ Warning**: Maintain minimum 20 good frames after filtering!

---

## 10. Advanced Optimization

### Bundle Adjustment with Robust Kernel

If all above techniques still result in error > 0.3 pixels, consider advanced optimization:

**Tools**:
- Kalibr: `camchain-imucam-calibrate`
- Custom implementation: Ceres Solver or g2o

**Method**:
1. Use OpenCV calibration result as initial guess
2. Run bundle adjustment with Huber robust kernel (threshold ~0.5)
3. This can reduce error by an additional 20-30%

**Trade-off**: 10× computation time vs OpenCV

**When to use**: 
- Critical applications requiring < 0.25 pixel accuracy
- After exhausting all other optimization techniques
- When you have the computational resources and time

---

## Typical Results

Following this guide, you should achieve:

| Metric | Target | Typical After Optimization |
|--------|--------|---------------------------|
| Monocular reprojection error | < 0.3 pix | 0.15-0.25 pix |
| Stereo reprojection error | < 0.3 pix | 0.20-0.30 pix |
| Maximum stereo error | < 1.5 pix | 0.8-1.2 pix |
| Rectification Y-error (avg) | < 0.3 pix | 0.1-0.2 pix |
| Rectification Y-error (max) | < 0.7 pix | 0.3-0.5 pix |
| Baseline error | < 1 mm | 0.2-0.5 mm |

If your errors are significantly higher:
1. Review input data quality (Section 1)
2. Check checkerboard physical dimensions (Section 1.3)
3. Verify you're using fisheye model (Section 1.4)
4. Ensure adequate pose variety (Section 3)
5. Consider thermal/mechanical stability (Section 7)

---

## Troubleshooting

### "Ill-conditioned matrix" Error

**Causes**:
- Too few images or poor pose variety
- Checkerboard corners too close together (all in center)
- Numerical instability from extreme parameters

**Solutions**:
- Disable `CALIB_CHECK_COND` flag (current implementation)
- Capture more images with better distribution
- Use moderate convergence criteria (not too strict)

### High Rectification Error

**Causes**:
- Poor stereo baseline accuracy
- Insufficient edge coverage
- Incorrect stereo extrinsics

**Solutions**:
- Adjust alpha parameter (try 0.7-0.8)
- Try disabling CALIB_ZERO_DISPARITY flag
- Ensure board appears at all 4 edges in multiple images
- Verify physical baseline measurement

### Inconsistent Results

**Causes**:
- Thermal instability
- Mechanical movement of cameras
- Focus drift

**Solutions**:
- Temperature stabilization (Section 7)
- Lock focus rings
- Rigid camera mount
- Capture all images in single session

---

## Utility Scripts

The repository includes Python utilities to help with data quality:

### Blur Detection
```bash
python3 utils/laplacian_var.py imgs/ --threshold 100
```

### Corner Analysis
```bash
python3 utils/corner_analysis.py imgs/ --width 9 --height 6 --prefix left --save
```

---

## References

This guide is based on:
1. OpenCV fisheye camera model documentation
2. Industry best practices for camera calibration
3. Research on robust calibration techniques
4. Practical experience with various fisheye lens systems

For questions or improvements, please open an issue on GitHub.
