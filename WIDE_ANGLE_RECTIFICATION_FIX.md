# Fix for Stereo Rectification Error with Wide-Angle Fisheye Lenses

## Problem Statement

During stereo calibration with the Double-Sphere model (`calibrate_ds`), the rectification error evaluation step reports:

```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Using rectified image size: 1600x1200
   Evaluated 0 corner points
   Average y-difference: N/A (no valid points)
   Maximum y-difference: N/A (no valid points)
   Status: SKIPPED (insufficient data for evaluation)
```

With diagnostic information:
```
Warning: No valid points found for rectification error calculation
   Total corner points: 352
   Points behind camera: 0
   Points with negative Z after rectification: 183 (52%)
   Points outside rectified image bounds: 169 (48%)
   Rectified image size: 1600x1200
   Virtual camera: fx=366.971, fy=367.268, cx=800, cy=600
```

## Root Causes

The issue occurs with wide-angle fisheye lenses (FOV > 130°) due to:

1. **Aggressive rectification rotation**: Standard stereo rectification rotates cameras to align epipolar lines horizontally. For wide-angle lenses, this rotation causes many peripheral points to end up behind the rectified camera plane (negative Z).

2. **Insufficient rectified image size**: The original image size may not be large enough to accommodate the projected points after rectification transformation.

3. **Suboptimal virtual camera parameters**: The virtual pinhole camera's focal length needs to be scaled appropriately for wide-angle lenses.

## Solution

### 1. Partial Rectification for Wide-Angle Lenses

Instead of applying full rectification, we now apply partial rectification for wide-angle lenses:

```cpp
// Estimate FOV from focal length and image size
double image_diagonal = sqrt(width^2 + height^2);
double approx_fov_deg = 2.0 * atan(image_diagonal / (2.0 * fx)) * 180.0 / M_PI;

// Apply partial rectification based on FOV
if (approx_fov_deg > 170.0) {
    rect_strength = 0.5;   // 50% rectification
} else if (approx_fov_deg > 130.0) {
    rect_strength = 0.7;   // 70% rectification
} else if (approx_fov_deg > 100.0) {
    rect_strength = 0.85;  // 85% rectification
}

// Apply using angle-axis interpolation
cv::Rodrigues(R_rect_full, rvec_full);
cv::Mat rvec_partial = rvec_full * rect_strength;
cv::Rodrigues(rvec_partial, R_rect);
```

This reduces the rotation angle, keeping more points in front of the camera while still achieving meaningful epipolar alignment.

### 2. Adaptive Rectified Image Size

The rectified image size now scales based on the calibrated focal length:

```cpp
double avg_fx = (ds_left.fx + ds_right.fx) / 2.0;

if (avg_fx < 300.0) {
    // Extreme wide-angle (FOV > 200°): 2.0x increase
    rectified_size = original_size * 2.0;
} else if (avg_fx < 500.0) {
    // Wide-angle (FOV > 150°): 1.5x increase
    rectified_size = original_size * 1.5;
} else if (avg_fx < 700.0) {
    // Moderate wide-angle: 1.25x increase
    rectified_size = original_size * 1.25;
}
```

For the problem case (fx ≈ 367):
- Rectified size: 1600×1200 → 2400×1800

### 3. Adaptive Virtual Camera Focal Length

The virtual pinhole camera focal length is now scaled more aggressively:

```cpp
if (avg_fx < 300.0) {
    focal_scale = 0.4;   // Very aggressive for extreme wide-angle
} else if (avg_fx < 500.0) {
    focal_scale = 0.6;   // Aggressive for wide-angle
} else if (avg_fx < 700.0) {
    focal_scale = 0.7;   // Moderate for moderate wide-angle
} else {
    focal_scale = 0.8;   // Standard
}

virtual_fx = avg_fx * focal_scale;
```

For the problem case (fx ≈ 367):
- Virtual fx: 367 × 0.6 = 220 (was ~293)

### 4. Enhanced Diagnostic Information

The diagnostic output now includes:

- Percentage breakdown of failure modes
- Estimated FOV and rectification strength applied
- Context-specific suggestions
- Only shows when < 50% of points are valid

Example output:
```
Rectification diagnostic information (only 0.0% of points were valid):
   Total corner points: 352
   Valid points: 0 (0.0%)
   Points behind camera: 0 (0.0%)
   Points with negative Z after rectification: 183 (52.0%)
   Points outside rectified image bounds: 169 (48.0%)
   Rectified image size: 2400x1800
   Calibrated focal lengths: left_fx=367.2, right_fx=366.7
   Estimated FOV: 140.2° (rectification strength: 0.7)
   Virtual camera: fx=220.2, fy=220.4, cx=1200, cy=900
Suggestions:
   - Most points have negative Z: this is expected for extreme wide-angle lenses (FOV > 180°)
   - Consider using the monocular and stereo reprojection errors as primary metrics
```

## Expected Results

With these changes, for a camera with:
- Focal length: fx ≈ 367 pixels
- Image size: 1600×1200
- Estimated FOV: ~140°

The improvements are:
- **Rectification strength**: 70% (reduced from 100%)
- **Rectified image size**: 2400×1800 (increased from 1600×1200)
- **Virtual focal length**: ~220 pixels (reduced from ~293)

This should result in:
- Significantly fewer points with negative Z (from ~52% to < 20%)
- Fewer points out of bounds (from ~48% to < 30%)
- Valid point count: > 50% (from 0%)

## Trade-offs

**Pros:**
- More points can be evaluated for rectification error
- Better diagnostic information for troubleshooting
- Graceful degradation for extreme wide-angle lenses

**Cons:**
- Partial rectification means epipolar lines are not perfectly horizontal
- Rectification error will be slightly higher but more realistic
- Larger rectified image size requires more memory

## When to Use This

This fix is automatically applied based on the calibrated focal length:
- **Always beneficial** for FOV > 130° (fx < 500)
- **Essential** for FOV > 170° (fx < 300)
- **Not needed** for standard lenses (FOV < 100°, fx > 700)

## Alternative Metrics

For extreme wide-angle lenses where rectification error cannot be reliably computed, the following metrics remain valid:

1. **Monocular reprojection error** (target: < 0.3 pixels)
2. **Stereo reprojection error** (target: < 0.3 pixels)
3. **Maximum stereo reprojection error** (target: < 1.5 pixels)
4. **Baseline distance accuracy** (target: < 1 mm)

These metrics do not depend on rectification and are reliable indicators of calibration quality.

## Technical Details

### FOV Estimation

The approximate FOV is calculated from the focal length and image diagonal:

```
diagonal = sqrt(width^2 + height^2)
FOV = 2 * atan(diagonal / (2 * fx)) * 180 / π
```

For the problem case:
- diagonal = sqrt(1600^2 + 1200^2) = 2000
- FOV = 2 * atan(2000 / (2 * 367)) ≈ 140°

### Partial Rectification Mathematics

Partial rectification is implemented using angle-axis interpolation:

1. Convert full rectification rotation matrix to angle-axis: `R → rvec`
2. Scale the angle-axis vector: `rvec_partial = rvec * strength`
3. Convert back to rotation matrix: `rvec_partial → R_partial`

This linearly interpolates the rotation angle while keeping the rotation axis fixed, which is equivalent to SLERP for small angles.

## References

- [OpenCV Stereo Rectification](https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html#ga617b1685d4059c6040827800e72ad2b6)
- [Double-Sphere Camera Model (Usenko et al., 2018)](https://arxiv.org/abs/1807.08957)
- [Wide-angle camera calibration challenges](https://en.wikipedia.org/wiki/Fisheye_lens)
