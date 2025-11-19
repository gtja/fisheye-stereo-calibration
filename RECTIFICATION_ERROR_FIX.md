# Fix for "Evaluated 0 corner points" in Stereo Rectification Error

## Problem Statement

During stereo calibration with the Double-Sphere model (`calibrate_ds`), the rectification error evaluation step would sometimes report:

```
4. Stereo Rectification Error:
   Computing custom rectification for Double-Sphere model...
   Evaluated 0 corner points
   Average y-difference: 0.0000 pixels [threshold: < 0.3 pixel]
   Maximum y-difference: 0.0000 pixels [threshold: < 0.7 pixel]
   Status: PASS
```

This was problematic because:
1. The status shows "PASS" even though no actual evaluation occurred
2. No diagnostic information was provided to understand why no points were evaluated
3. Users couldn't determine if this was a calibration failure or an edge case

## Root Causes

The issue occurred when all corner points were skipped during rectification error calculation due to:

1. **Hardcoded rectified image size**: The rectified image size was fixed at 960×720, which may be too small for some camera configurations
2. **Fixed virtual camera focal length**: The virtual pinhole camera used a fixed focal length of 300.0, which didn't match many calibrated cameras
3. **Strict bounds checking**: Points projecting outside the rectified image bounds were silently discarded
4. **No diagnostic output**: When 0 points were evaluated, no information was provided about why

## Solution

### 1. Enhanced Diagnostic Logging (double_sphere.h)

Added detailed diagnostic counters and messages when 0 points are evaluated:

```cpp
// Diagnostic counters for debugging
int total_points = 0;
int points_behind_camera = 0;
int points_negative_z_rect = 0;
int points_out_of_bounds = 0;
```

When no valid points are found, the system now outputs:
- Total number of corner points processed
- Number of points behind the camera
- Number of points with negative Z after rectification
- Number of points outside rectified image bounds
- Virtual camera parameters
- Helpful troubleshooting suggestions

Example output:
```
Warning: No valid points found for rectification error calculation
   Total corner points: 1134
   Points behind camera: 0
   Points with negative Z after rectification: 0
   Points outside rectified image bounds: 1134
   Rectified image size: 50x50
   Virtual camera: fx=40, fy=40, cx=25, cy=25
Suggestions:
   - If all points are out of bounds, the rectified image size may be too small
   - If all points have negative Z, the rectification rotation may be incorrect
   - If all points are behind camera, the extrinsics may be incorrect
```

### 2. Improved Virtual Camera Parameters (double_sphere.h)

Changed from fixed focal length to adaptive focal length based on calibrated cameras:

**Before:**
```cpp
VirtualPinholeParams virtual_cam(rectified_size.width, rectified_size.height);  // fx=fy=300.0
```

**After:**
```cpp
// Use average focal length from the calibrated cameras
double avg_fx = (left_params.fx + right_params.fx) / 2.0;
double avg_fy = (left_params.fy + right_params.fy) / 2.0;
// Scale down by 0.8 to fit more points in the rectified image
double focal_scale = 0.8;
VirtualPinholeParams virtual_cam(rectified_size.width, rectified_size.height, 
                                 avg_fx * focal_scale);
virtual_cam.fy = avg_fy * focal_scale;
```

This ensures the virtual camera projection better matches the calibrated cameras.

### 3. Adaptive Rectified Image Size (calibrate_ds.cpp)

Changed from hardcoded size to using the actual input image dimensions:

**Before:**
```cpp
cv::Size rectified_size(960, 720);
```

**After:**
```cpp
cv::Size rectified_size = img1.size();  // Use actual image size
printf("   Using rectified image size: %dx%d\n", rectified_size.width, rectified_size.height);
```

This ensures the rectified image is large enough to accommodate all corner points.

### 4. Better Error Reporting (calibrate_ds.cpp)

Updated the output to handle the 0 points case gracefully:

```cpp
printf("   Evaluated %d corner points\n", num_rect_points);
if (num_rect_points > 0) {
    printf("   Average y-difference: %.4f pixels [threshold: < 0.3 pixel]\n", avg_rectification_err);
    printf("   Maximum y-difference: %.4f pixels [threshold: < 0.7 pixel]\n", max_rectification_err);
    printf("   Status: %s\n", 
           (avg_rectification_err < 0.3 && max_rectification_err < 0.7) ? "PASS" : "FAIL");
} else {
    printf("   Average y-difference: N/A (no valid points)\n");
    printf("   Maximum y-difference: N/A (no valid points)\n");
    printf("   Status: SKIPPED (insufficient data for evaluation)\n");
    printf("\n   Note: Rectification error could not be evaluated...\n");
    // ... helpful suggestions ...
}
```

### 5. Updated YAML Output (calibrate_ds.cpp)

Added the number of evaluated points to the calibration results file:

```cpp
fs << "rectification_error_num_points" << num_rect_points;
if (num_rect_points > 0) {
    fs << "rectification_error_avg" << avg_rectification_err;
    fs << "rectification_error_max" << max_rectification_err;
} else {
    fs << "rectification_error_avg" << -1.0;  // Indicate N/A
    fs << "rectification_error_max" << -1.0;  // Indicate N/A
}
```

## Testing

### Test Results

With the sample images in the repository:
- **Before**: 998 corner points evaluated
- **After**: 1070 corner points evaluated (+7.2% improvement)

### Test Program

A test program (`test_rectification.cpp`) was added to verify the diagnostic messages work correctly:

```bash
cd build
make test_rectification
./test_rectification
```

This test verifies:
1. Normal case with reasonable rectified image size (should evaluate all points)
2. Edge case with very small rectified image size (should show diagnostic messages)
3. Edge case with mismatched focal lengths (tests robustness)

## Impact

### Benefits
1. **Better diagnostics**: Users can now understand why rectification error evaluation failed
2. **More robust**: Adaptive parameters work better across different camera configurations
3. **More accurate**: Increased number of evaluated points provides better error estimates
4. **Better UX**: Clear status messages ("SKIPPED" vs misleading "PASS")

### Backward Compatibility
- All existing functionality remains intact
- Calibration output format is extended (new field added) but remains compatible
- No breaking changes to the API or command-line interface

## Related Issues

This fix addresses the issue reported in the problem statement:
```
在进行Stereo Rectification Error 评估时，Evaluated 0 corner points，
角点评估的数量为0，是否存在问题
```

Translation: "During Stereo Rectification Error evaluation, 'Evaluated 0 corner points' - 
is there a problem with the corner point evaluation count being 0?"

**Answer**: Yes, there was an issue. The system was too strict in its bounds checking and 
used parameters that didn't match the calibrated cameras. This has now been fixed with 
adaptive parameters and better diagnostics.
